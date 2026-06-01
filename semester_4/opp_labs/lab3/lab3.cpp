#include <mpi.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#define ROOT_RANK 0
#define GRID_DIMS 2
#define NO_REORDER 0
#define ERR 1
#define START_INDEX 0

inline int matrixIndex(int i, int j, int cols) {
    return i * cols + j;
}

void fillMatrix(std::vector<double>& M, int rows, int cols){
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            M[matrixIndex(i, j, cols)] = 1.0;
        }
    }
}

struct GridInfo {
    MPI_Comm gridComm{};
    MPI_Comm rowComm{};
    MPI_Comm colComm{};

    int worldRank{};
    int worldSize{};

    int gridRank{};
    int p1{};
    int p2{};

    int x{};
    int y{};
};


GridInfo createGrid() {
    GridInfo grid{};

    MPI_Comm_rank(MPI_COMM_WORLD, &grid.worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &grid.worldSize);

    int dims[GRID_DIMS] = {0, 0};

    MPI_Dims_create(grid.worldSize, GRID_DIMS, dims);
    grid.p1 = dims[0];
    grid.p2 = dims[1];

    int periods[GRID_DIMS] = {0, 0};
    MPI_Cart_create(MPI_COMM_WORLD, GRID_DIMS, dims, periods, NO_REORDER, &grid.gridComm);
    

    int coords[GRID_DIMS] = {0, 0}; //Узнаем координаты процесса в сетке по rank сетки
    MPI_Comm_rank(grid.gridComm, &grid.gridRank);
    MPI_Cart_coords(grid.gridComm, grid.gridRank, GRID_DIMS, coords);
    grid.x = coords[0];
    grid.y = coords[1];

    MPI_Comm_split(grid.gridComm, grid.x, grid.y, &grid.rowComm);
    MPI_Comm_split(grid.gridComm, grid.y, grid.x, &grid.colComm);

    return grid;
}

void freeGrid(GridInfo& grid) {
    MPI_Comm_free(&grid.rowComm);
    MPI_Comm_free(&grid.colComm);
    MPI_Comm_free(&grid.gridComm);
}

void checkSizes(int n1, int n3, const GridInfo& grid) {
    if (n1 % grid.p1 != 0 || n3 % grid.p2 != 0) {
        if (grid.worldRank == 0) {
            std::cerr << "Error: matrix sizes must satisfy:\n";
            std::cerr << "n1 % p1 == 0 and n3 % p2 == 0\n";
        }
        MPI_Abort(grid.gridComm, ERR);
    }
}

bool checkResult(const std::vector<double>& C, int n1, int n3, int n2) {
    const double expected = static_cast<double>(n2);
    const double eps = 1e-9;

    for (int i = 0; i < n1; ++i) {
        for (int j = 0; j < n3; ++j) {
            double value = C[matrixIndex(i, j, n3)];

            if (std::fabs(value - expected) > eps) {
                return false;
            }
        }
    }

    return true;
}

void scatterA(const std::vector<double>& A, std::vector<double>& localA, int n1,int n2, const GridInfo& grid) {
    const int localRows = n1 / grid.p1;

    int send_count = localRows * n2;
    if (grid.y == 0) {    
        MPI_Scatter(A.data(), send_count, MPI_DOUBLE, localA.data(),
                    send_count, MPI_DOUBLE, ROOT_RANK, grid.gridComm);

        MPI_Comm_free(&firstColComm);
    }

    MPI_Bcast(localA.data(), send_count, MPI_DOUBLE, ROOT_RANK, grid.rowComm);
}

void scatterB(const std::vector<double>& B, std::vector<double>& localB,
              int n2, int n3, const GridInfo& grid) {
    const int localCols = n3 / grid.p2;


    if (grid.x == 0) {
        MPI_Datatype stripeType;
        //n2 блоков (построчно), размер localCols, шаг n3
        MPI_Type_vector(n2, localCols, n3, MPI_DOUBLE, &stripeType); //Описывает одну вертикальную полосу

        MPI_Datatype resizedStripeType;
        MPI_Type_create_resized(stripeType, START_INDEX, static_cast<MPI_Aint>(localCols) * sizeof(double), &resizedStripeType);

        MPI_Type_commit(&resizedStripeType);

        MPI_Scatter(B.data(), 1, resizedStripeType, localB.data(),
                    n2 * localCols, MPI_DOUBLE, ROOT_RANK, grid.rowComm);

        MPI_Type_free(&resizedStripeType);
        MPI_Type_free(&stripeType);
        MPI_Comm_free(&firstRowComm);
    }

    MPI_Bcast(localB.data(), n2 * localCols, MPI_DOUBLE, ROOT_RANK, grid.colComm);
}

void multiplyLocal(const std::vector<double>& localA,
                   const std::vector<double>& localB,
                   std::vector<double>& localC,
                   int localRows, int n2, int localCols) {
    
    
    for (int i = 0; i < localRows; ++i) {
        for (int j = 0; j < localCols; ++j) {
            double sum = 0.0;
            for (int k = 0; k < n2; ++k) {
                sum += localA[matrixIndex(i, k, n2)] * localB[matrixIndex(k, j, localCols)];
            }
            localC[matrixIndex(i, j, localCols)] = sum;
        }
    }
}


void gatherC(std::vector<double>& C,
             const std::vector<double>& localC,
             int n1, int n3, const GridInfo& grid) {
    
    const int localRows = n1 / grid.p1;
    const int localCols = n3 / grid.p2;

    MPI_Datatype blockType;
    MPI_Datatype resizedBlockType;

    int sizes[GRID_DIMS] = {n1, n3};
    int subsizes[GRID_DIMS] = {localRows, localCols};
    int starts[GRID_DIMS] = {0, 0};

    MPI_Type_vector(localRows, localCols, n3, MPI_DOUBLE, &blockType);

    MPI_Type_create_resized(blockType, START_INDEX, sizeof(double), &resizedBlockType);

    MPI_Type_commit(&resizedBlockType);

    std::vector<int> recvCounts;
    std::vector<int> displs;

    if (grid.gridRank == ROOT_RANK) {
        recvCounts.resize(grid.p1 * grid.p2, 1); //От всех процессов 1 объект
        displs.resize(grid.p1 * grid.p2);

        for (int blockX = 0; blockX < grid.p1; blockX++) {
            for (int blockY = 0; blockY < grid.p2; blockY++) {
                int coords[GRID_DIMS] = {blockX, blockY};

                int srcRank = 0;
                MPI_Cart_rank(grid.gridComm, coords, &srcRank);

                displs[srcRank] = blockX * localRows * n3 + blockY * localCols;
            }
        }
    }

    int size = localCols*localRows;
    MPI_Gatherv(localC.data(), size, MPI_DOUBLE,
                grid.gridRank == ROOT_RANK ? C.data() : nullptr,
                grid.gridRank == ROOT_RANK ? recvCounts.data() : nullptr,
                grid.gridRank == ROOT_RANK ? displs.data() : nullptr,
                resizedBlockType,
                ROOT_RANK,
                grid.gridComm);

    MPI_Type_free(&resizedBlockType);
    MPI_Type_free(&blockType);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    GridInfo grid = createGrid();

    int n1 = 1024;
    int n2 = 1024;
    int n3 = 1024;

    if (argc >= 4) {
        n1 = std::atoi(argv[1]);
        n2 = std::atoi(argv[2]);
        n3 = std::atoi(argv[3]);
    }

    checkSizes(n1, n3, grid);

    const int localRows = n1 / grid.p1;
    const int localCols = n3 / grid.p2;

    std::vector<double> A;
    std::vector<double> B;
    std::vector<double> C;

    if (grid.worldRank == ROOT_RANK) {
        A.resize(n1 * n2);
        B.resize(n2 * n3);
        C.resize(n1 * n3);

        fillMatrix(A, n1, n2);
        fillMatrix(B, n2, n3);
    }

    std::vector<double> localA(localRows * n2);
    std::vector<double> localB(n2 * localCols);
    std::vector<double> localC(localRows * localCols, 0.0);

    MPI_Barrier(grid.gridComm);
    double t0 = MPI_Wtime();

    scatterA(A, localA, n1, n2, grid);
    scatterB(B, localB, n2, n3, grid);

    multiplyLocal(localA, localB, localC, localRows, n2, localCols);

    gatherC(C, localC, n1, n3, grid);

    MPI_Barrier(grid.gridComm);
    double t1 = MPI_Wtime();

    double time = t1 - t0;
    


    if (grid.worldRank == ROOT_RANK) {
        bool ok = checkResult(C, n1, n3, n2);

        std::cout << "Matrix A: " << n1 << " x " << n2 << "\n";
        std::cout << "Matrix B: " << n2 << " x " << n3 << "\n";
        std::cout << "Matrix C: " << n1 << " x " << n3 << "\n";
        std::cout << "Grid: " << grid.p1 << " x " << grid.p2 << "\n";
        std::cout << "Local block: " << localRows << " x " << localCols << "\n";
        std::cout << "Time: " << std::fixed << std::setprecision(6)
                  << time << " sec\n";
        std::cout << "Check: " << (ok ? "OK" : "FAILED") << "\n";
    }

    freeGrid(grid);
    MPI_Finalize();

    return 0;
}
