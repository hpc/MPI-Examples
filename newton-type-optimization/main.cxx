#include<iostream>
#include<cassert>
#include<vector>
#include<cmath>
#include<mpi.h>
#include "minimize.cxx"


// Function to be optimized
long double f(vector<long double> x)
{
   long double result=1;
   for(int i = 0; i < (int)x.size();i++)
           result += (x[i]*sqrt(x[i]*x[i]));
   return result/(double)x.size()-25;
}

int main(int argc, char *argv[])
{
    vector<long double> guess;
    long double epsilon = EPSILON;
    for(int i = 0; i < 30; i++)
        guess.push_back((float)(rand() + ((rand() % 100)/100.0)-1.0));
    // Initialize MPI
    MPI::Init(argc,argv);
    // Minimize argument
    minimize(epsilon, guess,&f); 
    // Finalize MPI
    MPI::Finalize();
}
