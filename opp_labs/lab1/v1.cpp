#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>

const double EPS = 1e-5;
const double TAU = 0.001;
const int MAXITER = 100000;
const int N = 1200;

double norm(const std::vector<double> &x)
{
    double sum = 0.0;
    for (double i : x) sum += i * i;
    return std::sqrt(sum);
}


void splitRows(int N, int size,
               std::vector<int> &rowsCount, std::vector<int> &rowsStart)
{
    int base = N / size;
    int rem = N % size;

    rowsCount[0] = base + rem;
    rowsStart[0] = 0;

    for (int p = 1; p < size; ++p)
    {
        rowsCount[p] = base;
        rowsStart[p] = rem + p * base;
    }
}


void buildLocalSystem(int rank, int size, int N,
                const std::vector<int> &rowsCount,
                const std::vector<int> &rowsStart,
                std::vector<double> &A_loc,
                std::vector<double> &b, std::vector<double> &u)
{
    int localRows = rowsCount[rank];
    int row0 = rowsStart[rank];

    if (rank == 0){
        u.resize(N);
        const double pi = std::acos(-1.0);

        double sum_u = 0.0;
        for (int i = 0; i < N; ++i){
            u[i] = std::sin(2.0 * pi * i / N);
            sum_u += u[i];
        }
        
        for (int i = 0; i < N; ++i){
            b[i] = sum_u + u[i];
        }
    }


    MPI_Bcast(b.data(), N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    A_loc.resize(localRows * N);
    for (int i = 0; i < localRows; ++i){
        int globalRow = row0 + i;
        
        for (int j = 0; j < N; ++j){
            A_loc[i * N + j] = (globalRow == j) ? 2.0 : 1.0;
        }
    }
}


int runIterations(int rank,
                  int localRows, int row0,
                  const std::vector<int> &rowsCount,
                  const std::vector<int> &rowsStart,
                  const std::vector<double> &A_loc,
                  const std::vector<double> &b,
                  std::vector<double> &x,
                  double &normR,
                  bool &converged)
{
    std::vector<double> r(N);
    std::vector<double> Ax_loc(localRows);
    std::vector<double> r_loc(localRows);

    const double normB = norm(b);
    if (normB == 0.0){
        converged = false;
        normR = 0.0;
        return 0;
    }

    
    int it = 0;
    for (; it < MAXITER; ++it){
        
        //Ax_loc
        for (int i = 0; i < localRows; ++i){
            double sum = 0.0;
            for (int j = 0; j < N; ++j){
                sum += A_loc[i * N + j] * x[j];
            }
            Ax_loc[i] = sum;
        }

        for (int i = 0; i < localRows; ++i){
            int globalRow = row0 + i;
            r_loc[i] = Ax_loc[i] - b[globalRow];
        }
        
        MPI_Allgatherv(r_loc.data(), localRows, MPI_DOUBLE, r.data(),
                       rowsCount.data(), rowsStart.data(), MPI_DOUBLE, MPI_COMM_WORLD);
                       
        normR = norm(r);

        if (normR / normB < EPS){
            converged = true;
            break;
        }

        for (int i = 0; i < N; ++i){
            x[i] -= TAU * r[i];
        }
    }
    return it;
}


int main(){
    MPI_Init(nullptr, nullptr);
    int rank, size;
    std::vector<double> u;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);


    std::vector<int> rowsCount(size), rowsStart(size);
    splitRows(N, size, rowsCount, rowsStart);

    int localRows = rowsCount[rank];
    int row0 = rowsStart[rank];

    std::vector<double> b(N), A_loc;
    buildLocalSystem(rank, size, N, rowsCount, rowsStart, A_loc, b, u);
    std::vector<double> x(N, 0.0);

    //start iterations
    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    double normR = 0.0;
    bool converged = false;
    int it = runIterations(rank,
            localRows, row0,
            rowsCount, rowsStart,
            A_loc, b, x,
            normR, converged);


    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    double time = t1 - t0;
    
    
    
    if (rank == 0){
        std::cout << "TIME : " << time << "sec\n";
        std::cout << (converged ? "CONVERGED" : "NOT CONVERGED") << "\n";
        std::cout << "iters = " << it << "\n";

        std::vector<double> diff(N);
        for (int i = 0; i < N; ++i)
            diff[i] = x[i] - u[i];
        std::cout << "DIFF = " << (norm(diff) / norm(u)) << "\n";
    }

    MPI_Finalize();
    return 0;
}