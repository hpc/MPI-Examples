#ifndef MINIMIZE_CXX
#define MINIMIZE_CXX
// File: minimize.cxx
// Authors: Jharrod LaFon, Ghololami Rahnavard
// Purpose:  Optimizes a function in parallel
#include<iostream>
#include<cassert>
#include<vector>
#include<cmath>
#include<mpi.h>
#include "matrix.cxx"
// Accuracy
#define EPSILON 1E-6
// Block Partitioning
#define BLOCK_LOW(id,p,n) ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) (BLOCK_LOW((id)+1,p,n)-1)

using namespace std;

// Prints a std::vector<long double> list
void print_vec(vector<long double> v)
{
    for(size_t i = 0; i < v.size(); i++)
        cout << v[i] << " ";
    cout << endl;
}

// Approximates the first derivative
long double first_derivative(vector<long double> x,int i,long double epsilon,long double (*f)(vector<long double>))
{
    vector<long double> xn(x);
    xn[i] += epsilon;
    return ((*f)(xn) - (*f)(x))/(epsilon);
}

// Approximates the second derivative
long double second_derivative(vector<long double> x,int i,long double epsilon,long double (*f)(vector<long double>))
{
    vector<long double> xn(x);
    xn[i] += epsilon;
    return (first_derivative(xn,i,epsilon,f) - first_derivative(x,i,epsilon,f))/(epsilon);
}

// Calculates the gradient of a function at x
vector<long double> gradient(vector<long double> x, long double epsilon,long double (*f)(vector<long double>))
{
    vector<long double> result(x.size());
    // Calculate gradient, loop once for every variable to be differentiated
    for(size_t i = 0; i < x.size(); i++)
    {
        // Copy x
        vector<long double> xe (x);
        xe[i] += epsilon;
        result[i] = -1*(((*f)(xe)-(*f)(x))/epsilon);
    }
    return result;
}

// Calculates a diagonal entry in the Hessian Matrix
long double hessian_diagonal(vector<long double> xn,int i, long double epsilon,long double (*f)(vector<long double>))
{
    vector<long double> xp(xn);
    vector<long double> xm(xn);
    // x_i + delta
    xp[i] += epsilon;
    // x_i - delta
    xm[i] -= epsilon;
    return ((*f)(xp) - 2 * (*f)(xn)+(*f)(xm))/(epsilon*epsilon);
}

// Calculates a non-diagonal entry in the Hessian Matrix
long double hessian_off_diagonal(vector<long double> xn, int i,int j, long double epsilon,long double (*f)(vector<long double>))
{
    // xn + delta at i,j
    vector<long double> xp(xn);
    xp[i] += epsilon;
    xp[j] += epsilon;
    // xn - delta at i,j
    vector<long double> xm(xn);
    xm[i] -= epsilon;
    xm[j] -= epsilon;
    return (((*f)(xp) - 2*(*f)(xn) + (*f)(xm))/(2*epsilon*epsilon) - (second_derivative(xn,i,epsilon,f)+second_derivative(xn,j,epsilon,f))/(2));
}

// Calculates one row of the Hessian Matrix
vector<long double> hessian_row(vector<long double> xn, long double epsilon, int row_num,long double (*f)(vector<long double>))
{
    vector<long double>result(xn.size());
    for(int i = 0; i < (int)xn.size(); i++)
        if(i == row_num)
            result[i] = hessian_diagonal(xn,i,(epsilon/2.0),f);
        else
            result[i] = hessian_off_diagonal(xn,i,row_num,(epsilon/2.0),f);
    return result;
}

// Computes a Hessian Matrix. This is a serial only function, not used with MPI
matrix hessian(vector<long double> xn, long double epsilon,long double (f)(vector<long double>))
{
    int count = xn.size();
    matrix h = matrix(count,count);
    for(int i = 0; i < count; i++)
    {
        for(int j = 0; j < count; j++)
        {
            if(i == j)
                h(i,j) = hessian_diagonal(xn,i,(epsilon/2.0),f);
            else
                h(i,j) = hessian_off_diagonal(xn,i,j,(epsilon/2.0),f);
        }
    }
    return h;
}

// Minimizes a function using MPI
int minimize(long double epsilon, vector<long double> guess,long double (*f)(vector<long double>))
{
    
    int rank, size;
    // length of vector
    int msize = guess.size();
    // get size and rank
    size = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    // set up block partitioning
    int low = BLOCK_LOW(rank-1,size-1,msize);
    int high = BLOCK_HIGH(rank-1,size-1,msize);

    // holds the gradient
    vector<long double> gradient_vec;
    
    // needed for termination
    long double last = 2 * epsilon;

    // needed for timing
    double temp_gradient, average_gradient, temp_hessian, average_hessian, temp_ihessian, average_ihessian, temp_vmult, average_vmult, global_start, global_end, temp_iter, average_iter;

    // number of iterations
    int count = 0;

    // buffer size to hold vector
    int buff_size = sizeof(long double)*msize;

    // can't minimize better than zero
    if(fabs((*f)(guess)) == 0.0)
        return 0;

    // start clock
    global_start = MPI_Wtime();

    // stores the hessian
    matrix hessian_matrix(msize,msize);

    // temporary buffer
    long double * temp = (long double *) malloc(buff_size);

    // stores one row of the hessian matrix
    vector<long double> hessian_r(guess);
    for(int i = 0; i < msize; i++)
        temp[i] = guess[i];
    MPI::Status status;

    // loop until done
    while(1)
    {
        temp_iter = MPI_Wtime();
        // Broadcast out the newest vector 
        MPI::COMM_WORLD.Bcast(temp,buff_size,MPI::BYTE,0);
        for(int i = 0; i < msize; i++)
            guess[i] = temp[i];

        // check termination condition
        if(fabs((*f)(guess)-last) < epsilon)
        {
            break;
        }
        if(rank == 0) cout << "f(guess) = " << (*f)(guess) << " diff = " << (*f)(guess)-last << endl;
        last = f(guess);
     
        // master rank
        if(rank == 0)
        {
            temp_gradient = MPI_Wtime();
            // compute gradient
            gradient_vec = gradient(guess,epsilon,f);
            average_gradient += MPI_Wtime() - temp_gradient;

            // compute the hessian
            temp_hessian = MPI_Wtime();
            int row = 0;
            // receive one result for every row in the hessian
            for(int i = 0; i < msize; i++)
            {
                // get a result from any rank
                MPI::COMM_WORLD.Recv(temp,buff_size,MPI::BYTE,MPI::ANY_SOURCE,MPI::ANY_TAG,status);
                // get the row number
                row = status.Get_tag();
                // copy it over
                for(int j = 0; j < msize; j++)
                    hessian_matrix(row,j) = temp[j];
            }
            average_hessian += MPI_Wtime() - temp_hessian;
           
            // invert the hessian
            temp_ihessian = MPI_Wtime();
            matrix inverse_hession = hessian_matrix.invert();
            average_ihessian += MPI_Wtime() - temp_ihessian;

            // multiply the hessian inverse by the gradient
            temp_vmult = MPI_Wtime();
            matrix new_guess = inverse_hession.vector_multiply(gradient_vec);
            average_vmult += MPI_Wtime() - temp_vmult;

            for(int j = 0; j < msize; j++)
            {
                guess[j] += new_guess(j,0);
                temp[j] = guess[j];
            }
            count++;
        }
        // a slave's life is simple
        else
        {
            // slave computes each row and returns it
            for(int i = low; i <= high; i++)
            {
                hessian_r = hessian_row(guess,epsilon,i,f);
                for(int j = 0; j < msize; j++)
                    temp[j] = hessian_r[j];
                MPI::COMM_WORLD.Send(temp,sizeof(long double)*msize,MPI::BYTE,0,i);
            }
        }
        average_iter += MPI_Wtime() - temp_iter;
    }
    if(rank == 0)
    {
        global_end = MPI_Wtime() - global_start;
        average_gradient /= count;
        average_hessian /= count;
        average_ihessian /= count;
        average_vmult /= count;
        average_iter /= count;
        if(msize <= 35) { cout << "Solution: "; print_vec(guess); cout << endl; }
        cout << "Elapsed time: " << global_end << endl;
        cout << count << " " << average_iter << " second iterations, f(guess) = " << (*f)(guess) << endl;
        cout << "Average Gradient: " << average_gradient << " " << average_gradient/average_iter*100 << "%" << endl;
        cout << "Average Hessian: " << average_hessian << " " << average_hessian/average_iter*100 << "%" << endl;
        cout << "Average Hessian Inverse: " << average_ihessian << " " << average_ihessian/average_iter*100 << "%" << endl;
        cout << "Average Vector Multiply: " << average_vmult << " " << average_vmult/average_iter*100 << "%" << endl;
    }
    return 0;
}
#endif
