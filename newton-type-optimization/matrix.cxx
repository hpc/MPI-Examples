#ifndef MATRIX_CXX
#define MATRIX_CXX
// File: matrix.cxx
// Author: Jharrod LaFon
// Date: Spring 2011
// Purpose: Simple Matrix Class

#include<cstdlib>
#include<cmath>
#include<cassert>
#include<iostream>
#include<iomanip>
#include<vector>

using std::cout;
using std::endl;

// Matrix class definition
class matrix
{
    public:
        // Constructors
        matrix( int Row = 10,  int Col = 10);
        // Copies another
        matrix(matrix * m);
        
        // Destructor
        ~matrix();

        // Accessors
         int rowCount();
         int colCount();

        // Print
        void print();

        // Returns a pivot row, used for inversion only
        int pivot();

        // Returns the inverse of a matrix
        matrix invert();

        // Swaps two matrix rows
        void swap_rows(const  int r1, const  int r2);

        // Multiplies the matrix by a vector
        matrix vector_multiply(const std::vector<long double> v);

        // Reference Operator
        long double  access(const  int row, const  int col);
        long double & operator() (const  int row, const  int col);
        // Alternative accessor
        long double & set(const  int row, const  int col, long double value);
    private:
        int row, col;
        long double ** array;
};

// Multiplies the matrix by a vector
matrix matrix::vector_multiply(const std::vector<long double> v)
{
    assert(this->col == ( int) v.size());
    matrix result(this->row,1);
    int i,j;
    long double temp;
    for(i = 0; i < this->row; i++)
    {
        temp = 0.0;
        for(j = 0; j < this->col; j++)
            temp += array[i][j]*v[j];
            result(i,0) = temp;
    }
    return result;
}

// Finds a pivot row, used for inverting a matrix
int matrix::pivot()
{

     int i,j,k;
    // check each row
    for(i = 0; i < this->row; i++)
        // check each column
	for(j = 0; j < this->col; j++)
        if(i == j)
        {
            if(array[i][j] != 1.0)
            {
                return i;
            }
            else
            {
                for(k = i+1; k < this->row; k++)
                    if(array[k][j] != 0)
                    {
                        return i;
                    }
            }
        }
    return i-1;
}

// Inverts a matrix.  The matrix must be invertible.
matrix matrix::invert()
{
    // make sure matrix is square
    assert(this->row == this->col);

    // allocate augmented matrix
    matrix augmatrix = matrix(this->row,this->col*2);
     int i,j,l;
    // create an augmented matrix
    for(i=0;i<this->rowCount();i++)
        for(j=0;j<this->colCount();j++)
            augmatrix(i,j) = this->access(i,j);
    for(i=0;i<this->rowCount();i++)
        for(j=this->colCount();j<augmatrix.colCount();j++)
            if((j-this->colCount()) == i)
                augmatrix(i,j) = 1;
            else
                augmatrix(i,j) = 0;
     int pivot;
     int count = 0;
    long double scale = 1.0;
   // augmatrix.print();
    while(count++ < augmatrix.rowCount())
    {
        pivot = augmatrix.pivot();
//	cout << "Pivot = " << pivot << endl; 
	if(augmatrix(pivot,pivot) == 0.0)
	{
	     int c = 1;
	    while(augmatrix(pivot,pivot) == 0.0 && ((c+pivot) < augmatrix.rowCount()))
	    {
		if(c == augmatrix.rowCount())
		    break;
//		cout << "c = " << c <<" Row count = " << augmatrix.rowCount() << endl;
//		cout << "Swapping row " << pivot << " with " << pivot+c << endl;
		augmatrix.swap_rows(pivot,pivot+c);
//		cout << "Swapped row " << pivot << " with " << pivot+c-1 << endl;
	    	c++;
	    }
	}
	scale = augmatrix(pivot,pivot);	
//	cout << "Scale = " << scale << endl;
        if(augmatrix(pivot,pivot) != (long double) 1.0)
	    for(l = 0; l < augmatrix.colCount(); l++)
	    {
		augmatrix(pivot,l) = augmatrix(pivot,l)/scale;
//		cout << "augmatrix(" << pivot << "," << l << ") = " << augmatrix(pivot,l) << endl;
	    }
//	cout << "Done with row " << pivot << endl;
        // find first non zero value below pivot
        for(i = pivot + 1; i < augmatrix.rowCount(); i++)
        {
            if(augmatrix(i,pivot) != (long double)0.0)
            {
                scale = augmatrix(i,pivot);
//                std::cout << "i = " << i << " Scale = " << augmatrix(i,pivot) <<  std::endl;
                for(l = 0; l < augmatrix.colCount(); l++)
                {
           //         std::cout << "(" << i << "," << l << ") = " << augmatrix(i,l) - augmatrix(i,k)*augmatrix(pivot,l) << std::endl;
                    augmatrix(i,l) = augmatrix(i,l) - scale * augmatrix(pivot,l);
		}
            }
        }
//	cout << "Done with all rows below pivot" << endl;
        // find first non zero value above pivot
        if(pivot > ( int)0)
	    for(i = pivot - 1; i >= ( int)0;i--)
	    {
//		cout << "i = " << i << " pivot = " << pivot << endl;
		if(augmatrix(i,pivot) != (long double)0.0)
		{
		    scale = augmatrix(i,pivot);
//		    cout << "scale = " << scale << endl;
		    for(l = 0; l < augmatrix.colCount(); l++)
		    {
			augmatrix(i,l) = augmatrix(i,l)-scale*augmatrix(pivot,l);
		    }
		}
//		cout << "Done with row " << i << endl;
	    }
//	cout << "Done with all rows above pivot" << endl;
    }
    matrix result(*this);
    for(i = 0; i < result.row; i++)
        for(j = 0; j < result.col; j++)
            result(i,j) = augmatrix(i,j+result.col);
    return result;
}

// Swaps two rows in the matrix
void matrix::swap_rows(const  int r1, const  int r2)
{
    assert(r1 >= 0 && r1 < this->row && r2 >= 0 && r2 < this->row);
    long double * temp = this->array[r1];
    this->array[r1] = this->array[r2];
    this->array[r2] = temp;
}

// Prints a matrix
void matrix::print()
{
    for(  int i = 0; i < this->row; i++)
    {
	std::cout << "[" << i << "] ";
        for(  int j = 0; j < this->col; j++)
            std::cout << array[i][j] << " ";
        std::cout << std::endl;
    }
}

// Constructor
matrix::matrix( int Row,  int Col)
{
    this->row = Row;
    this->col = Col;
    array = (long double **) malloc(sizeof(long double*)*row);
    for(  int i = 0; i < row; i++)
    {
        array[i] = (long double *) malloc(sizeof(long double)*col);
        for(  int j = 0; j < col; j++)
            array[i][j] = 0.0;
    }
}

// Destructor
matrix::~matrix()
{
    for(  int i = 0; i < this->row; i++)
;//        free(array[i]);
}

// Accessors
 int matrix::rowCount(){ return this->row; }
 int matrix::colCount(){ return this->col; }

long double  matrix::access(const  int row, const  int col)
{
    assert(row >= 0 && row < this->row && col >= 0 && col < this->col);
    return array[row][col];
}
long double & matrix::set(const  int row, const  int col, long double value)
{
    assert(row >= 0 && row < this->row && col >= 0 && col < this->col);
    array[row][col] = value;
    return array[row][col];
}

// Reference Operator
long double& matrix::operator () ( int row,  int col)
{
    assert(row >= 0 && row < this->row && col >= 0 && col < this->col);
    return array[row][col];
}

#endif
