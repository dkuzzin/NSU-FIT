    #include <mpi.h>
    #include <iostream>
    #include <vector>
    #include <cmath>
    #include <algorithm>


    const double EPS = 1e-5;
    const double TAU = 0.001;
    const int MAXITER = 100000;
    const int N = 1200;
    const int TAG_X = 1;


    void splitRows(int N, int size, 
                std::vector<int>&rowsCount, std::vector<int>& rowsStart)
    {
        int base = N / size;
        int rem = N % size;

        rowsCount[0] = base + rem;
        rowsStart[0] = 0;

        for (int p = 1; p < size; ++p){
            rowsCount[p] = base;
            rowsStart[p] = rem+p*base;
        }
    }


    void ring_sendrecv(
                        int rank, int size,
                        const std::vector<int>& rowsCount,
                        const std::vector<int>& rowsStart,
                        const std::vector<double>& A_loc,
                        const std::vector<double>& x_loc,
                        std::vector<double>& Ax_loc ,
                        std::vector<double>& buffer,
                        std::vector<double>& recvbuf )
    {
        const int localRows = rowsCount[rank];
        Ax_loc.assign(localRows, 0.0);
        

        int prev = (rank - 1 + size) % size;
        int next = (rank + 1) % size;

        int owner = rank;
        std::copy(x_loc.begin(), x_loc.end(), buffer.begin());
        
        
        for (int p = 0; p < size; ++p){
            int col0 = rowsStart[owner]; //startOfcols
            int cols = rowsCount[owner]; //countOfcols

            for (int i = 0; i < localRows; ++i){
                double sum = 0.0;
                for (int j = 0; j < cols; ++j){
                    sum += A_loc[i*N + (col0 + j)] * buffer[j];
                }
                Ax_loc[i] += sum;
            }

            if (p == size - 1) break;
            
            int new_owner = (owner - 1 + size) % size;
            
            int recvcount = rowsCount[new_owner];
            MPI_Sendrecv(
                buffer.data(), cols, MPI_DOUBLE, next, TAG_X,
                recvbuf.data(), recvcount, MPI_DOUBLE, prev, TAG_X,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE
            );
            
            buffer.swap(recvbuf);
            owner = new_owner;
        }
    }


    void buildLocal(int rank,
                    const std::vector<int>& rowsCount,
                    const std::vector<int>& rowsStart,
                    std::vector<double>& A_loc,
                    std::vector<double>& b_loc,
                    std::vector<double>& u_full)
    {
        int localRows = rowsCount[rank];
        int row0 = rowsStart[rank];
        const double pi = std::acos(-1.0);
        
        double sum_u = 0.0;
        
        
        if (rank == 0){
            u_full.resize(N);
            for (int i = 0; i < N; ++i){
                u_full[i] = std::sin(2.0 * pi * i / N);
                sum_u += u_full[i];
            }
        }else{
            for (int i = 0; i < N; ++i){
                sum_u += std::sin(2.0 * pi * i / N);
            }
        }

        

        A_loc.assign(localRows * N, 0.0);
        b_loc.assign(localRows, 0.0);
        
        for (int i = 0; i < localRows; ++i){
            int globalRow = row0 + i;
            double u_i = std::sin(2.0 * pi * globalRow / N);
            b_loc[i] = sum_u + u_i;

            for (int j = 0; j < N; ++j){
                A_loc[i*N + j] = (globalRow == j) ? 2.0 : 1.0;
            }
        }
    }

    double norm(const std::vector<double> &x){
        double sum = 0.0;
        for (double i:x){
            sum += i*i;
        }
        return std::sqrt(sum);
    }



    double norm_global(const std::vector<double>& v_loc){
        double localNorm2 = 0.0;
        for (double x : v_loc) localNorm2 += x*x;

        double globalNorm2 = 0.0;
        MPI_Allreduce(&localNorm2, &globalNorm2, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        return std::sqrt(globalNorm2);
    }


    int runIterationsRing(int rank, int size,
        int localRows,
        const std::vector<int>& rowsCount,
        const std::vector<int>& rowsStart,
        const std::vector<double>& A_loc,
        const std::vector<double>& b_loc,
        std::vector<double>& x_loc,
        double normB, double& normR,
        bool& converged)
    {
        std::vector<double> Ax_loc(localRows, 0.0);
        std::vector<double> r_loc(localRows, 0.0);

        converged = false;
        normR = 0.0;

        int maxBlock = 0;
        for (int c : rowsCount) maxBlock = std::max(maxBlock, c);

        std::vector<double> buffer(maxBlock);
        std::vector<double> recvbuf(maxBlock);
        
        int it = 0;
        for (; it < MAXITER; ++it) {    
            ring_sendrecv(rank, size, rowsCount, rowsStart, A_loc, x_loc, Ax_loc, buffer, recvbuf);


            for (int i = 0; i < localRows; ++i) {
                r_loc[i] = Ax_loc[i] - b_loc[i];
            }

            normR = norm_global(r_loc);

            if (normR / normB < EPS) {
                converged = true;
                break;
            }

            for (int i = 0; i < localRows; ++i) {
                x_loc[i] -= TAU * r_loc[i];
            }
        }

        return it;
    }




    int main(){    
        int rank, size;
        MPI_Init(nullptr, nullptr);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        
        std::vector<int> rowsCount(size), rowsStart(size);
        splitRows(N, size, rowsCount, rowsStart);
        int localRows = rowsCount[rank];
        

        std::vector<double> u_full;
        std::vector<double> A_loc;
        std::vector<double> b_loc(localRows);
        std::vector<double> x_loc(localRows, 0.0);
        std::vector<double> x_full;
        if (rank == 0) x_full.resize(N);

        buildLocal(rank, rowsCount, rowsStart, A_loc, b_loc, u_full);

        const double normB = norm_global(b_loc);
        if (normB == 0.0){
            MPI_Finalize(); 
            return 1;
        }

        
        
        
        
        //-----  iterations
        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = MPI_Wtime();

        double normR = 0.0;
        bool converged = false;
        int it = runIterationsRing(rank, size,
            localRows,rowsCount, rowsStart, A_loc, b_loc, x_loc, normB, normR, converged);

        MPI_Barrier(MPI_COMM_WORLD);
        double t1 = MPI_Wtime();
        double time = t1 - t0;
        
        
        MPI_Gatherv(x_loc.data(), localRows, MPI_DOUBLE, x_full.data(),
            rowsCount.data(), rowsStart.data(), MPI_DOUBLE, 0, MPI_COMM_WORLD);
        


        if (rank == 0) {
            std::cout << "TIME : " << time << "sec\n";
            std::cout << (converged ? "CONVERGED" : "NOT CONVERGED") << "\n";
            std::cout << "iters = " << it << "\n";
            
            std::vector<double> diff(N);
            for (int i = 0; i < N; ++i) diff[i] = x_full[i] - u_full[i];
            std::cout << "DIFF = " << (norm(diff) / norm(u_full)) << "\n";
        }



        MPI_Finalize(); 
        return 0;
    }