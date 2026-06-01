#include <mpi.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>


struct Grid1D {
    int zStart = 0;
    int zCount = 0;
};

struct ProblemData {
    int Nx = 128;
    int Ny = 128;
    int Nz = 128;

    double x0 = -1.0;
    double y0 = -1.0;
    double z0 = -1.0;

    double Dx = 2.0;
    double Dy = 2.0;
    double Dz = 2.0;

    double a = 1e5;
    double eps = 1e-8;
    int maxIters = 1'000'000;
};

struct SolverData {
    double hx = 0.0;
    double hy = 0.0;
    double hz = 0.0;

    double invHx2 = 0.0;
    double invHy2 = 0.0;
    double invHz2 = 0.0;

    double denom = 0.0;
};

inline std::size_t idx(int i, int j, int k, int Nx, int Ny) {
    return static_cast<std::size_t>(k) * Ny * Nx +
           static_cast<std::size_t>(j) * Nx + static_cast<std::size_t>(i);
}

inline double exactPhi(double x, double y, double z) {
    return x * x + y * y + z * z;
}

inline double rhoFunc(double x, double y, double z, double a) {
    return 6.0 - a * exactPhi(x, y, z); //Лапласиан phi = 6 : laplas phi - a * phi = rho
}

Grid1D split1D(int Nz, int rank, int size) {
    Grid1D g;
    int base = Nz / size;
    int rem = Nz % size;

    g.zCount = base + (rank < rem ? 1 : 0);
    g.zStart = rank * base + std::min(rank, rem);

    return g;
}

SolverData buildSolverData(const ProblemData& p) {
    SolverData s;

    s.hx = p.Dx / (p.Nx - 1); //шаги сетки
    s.hy = p.Dy / (p.Ny - 1);
    s.hz = p.Dz / (p.Nz - 1);

    s.invHx2 = 1.0 / (s.hx * s.hx);
    s.invHy2 = 1.0 / (s.hy * s.hy);
    s.invHz2 = 1.0 / (s.hz * s.hz);

    s.denom = 2.0 * (s.invHx2 + s.invHy2 + s.invHz2) + p.a; //знаменатель формулы якоби

    return s;
}

void buildCoordinates(const ProblemData& p,
                      const SolverData& s,
                      const Grid1D& part,
                      std::vector<double>& xs,
                      std::vector<double>& ys,
                      std::vector<double>& zs) {
    xs.resize(p.Nx);
    ys.resize(p.Ny);
    zs.resize(part.zCount);

    for (int i = 0; i < p.Nx; ++i) {
        xs[i] = p.x0 + i * s.hx;
    }

    for (int j = 0; j < p.Ny; ++j) {
        ys[j] = p.y0 + j * s.hy;
    }

    for (int lk = 0; lk < part.zCount; ++lk) {
        int gk = part.zStart + lk;
        zs[lk] = p.z0 + gk * s.hz;
    }
}

bool isGlobalBoundary(int i, int j, int gk, const ProblemData& p) {
    return (i == 0 || i == p.Nx - 1 ||
            j == 0 || j == p.Ny - 1 ||
            gk == 0 || gk == p.Nz - 1);
}

//Начальное положение сетки. Границы фи а внутренние ноль
void initializeGrid(const ProblemData& p,
                    const Grid1D& part,
                    const std::vector<double>& xs,
                    const std::vector<double>& ys,
                    const std::vector<double>& zs,
                    std::vector<double>& cur,
                    std::vector<double>& next) {
    for (int lk = 1; lk <= part.zCount; ++lk) {
        int gk = part.zStart + (lk - 1);
        double z = zs[lk - 1];

        for (int j = 0; j < p.Ny; ++j) {
            double y = ys[j];
            for (int i = 0; i < p.Nx; ++i) {
                double x = xs[i];
                std::size_t pos = idx(i, j, lk, p.Nx, p.Ny);

                if (isGlobalBoundary(i, j, gk, p)) {
                    double val = exactPhi(x, y, z);
                    cur[pos] = val;
                    next[pos] = val;
                } else {
                    cur[pos] = 0.0;
                    next[pos] = 0.0;
                }
            }
        }
    }
}

int startHaloExchange(std::vector<double>& grid,
                      const ProblemData& p,
                      int localNz,
                      int prevRank,
                      int nextRank,
                      int recvFromPrevTag,
                      int sendToPrevTag,
                      int recvFromNextTag,
                      int sendToNextTag,
                      MPI_Request reqs[4]) {
    int reqCount = 0;
    int planeSize = p.Nx * p.Ny;

    if (prevRank != MPI_PROC_NULL) {
        MPI_Irecv(&grid[idx(0, 0, 0, p.Nx, p.Ny)], planeSize, MPI_DOUBLE,
                  prevRank, recvFromPrevTag, MPI_COMM_WORLD, &reqs[reqCount++]);

        MPI_Isend(&grid[idx(0, 0, 1, p.Nx, p.Ny)], planeSize, MPI_DOUBLE,
                  prevRank, sendToPrevTag, MPI_COMM_WORLD, &reqs[reqCount++]);
    }

    if (nextRank != MPI_PROC_NULL) {
        MPI_Irecv(&grid[idx(0, 0, localNz + 1, p.Nx, p.Ny)], planeSize, MPI_DOUBLE,
                  nextRank, recvFromNextTag, MPI_COMM_WORLD, &reqs[reqCount++]);

        MPI_Isend(&grid[idx(0, 0, localNz, p.Nx, p.Ny)], planeSize, MPI_DOUBLE,
                  nextRank, sendToNextTag, MPI_COMM_WORLD, &reqs[reqCount++]);
    }

    return reqCount;
}

void exchangeHalos(std::vector<double>& grid,
                   const ProblemData& p,
                   int localNz,
                   int prevRank,
                   int nextRank,
                   int recvFromPrevTag,
                   int sendToPrevTag,
                   int recvFromNextTag,
                   int sendToNextTag) {
    MPI_Request reqs[4];
    int reqCount = startHaloExchange(grid, p, localNz, prevRank, nextRank,
                                     recvFromPrevTag, sendToPrevTag,
                                     recvFromNextTag, sendToNextTag,
                                     reqs);

    if (reqCount > 0) {
        MPI_Waitall(reqCount, reqs, MPI_STATUSES_IGNORE);
    }
}

double updatePoint(int i, int j, int lk,
                   const ProblemData& p,
                   const SolverData& s,
                   const std::vector<double>& cur,
                   const std::vector<double>& xs,
                   const std::vector<double>& ys,
                   const std::vector<double>& zs) {
    double x = xs[i];
    double y = ys[j];
    double z = zs[lk - 1];

    return (
        (cur[idx(i - 1, j, lk, p.Nx, p.Ny)] +
         cur[idx(i + 1, j, lk, p.Nx, p.Ny)]) * s.invHx2 +
        (cur[idx(i, j - 1, lk, p.Nx, p.Ny)] +
         cur[idx(i, j + 1, lk, p.Nx, p.Ny)]) * s.invHy2 +
        (cur[idx(i, j, lk - 1, p.Nx, p.Ny)] +
         cur[idx(i, j, lk + 1, p.Nx, p.Ny)]) * s.invHz2 - 
        rhoFunc(x, y, z, p.a)
    ) / s.denom;
}

//обработка плоскости по z
void computePlane(int lk,
                  const ProblemData& p,
                  const SolverData& s,
                  const Grid1D& part,
                  const std::vector<double>& cur,
                  std::vector<double>& next,
                  const std::vector<double>& xs,
                  const std::vector<double>& ys,
                  const std::vector<double>& zs,
                  double& localMaxDiff) {
    if (lk < 1 || lk > part.zCount) { //проверка на halo слои
        return;
    }

    int gk = part.zStart + (lk - 1);
    if (gk == 0 || gk == p.Nz - 1) { //Проверка на границу тк там не меняем
        return;
    }

    for (int j = 1; j < p.Ny - 1; ++j) {
        for (int i = 1; i < p.Nx - 1; ++i) {
            std::size_t pos = idx(i, j, lk, p.Nx, p.Ny);
            double newVal = updatePoint(i, j, lk, p, s, cur, xs, ys, zs);
            localMaxDiff = std::max(localMaxDiff, std::fabs(newVal - cur[pos]));
            next[pos] = newVal;
        }
    }
}
//Обработка всех плоскостей помимо граничных и halo
void computeInteriorPlanes(const ProblemData& p,
                           const SolverData& s,
                           const Grid1D& part,
                           const std::vector<double>& cur,
                           std::vector<double>& next,
                           const std::vector<double>& xs,
                           const std::vector<double>& ys,
                           const std::vector<double>& zs,
                           double& localMaxDiff) {
    for (int lk = 2; lk <= part.zCount - 1; ++lk) {
        computePlane(lk, p, s, part, cur, next, xs, ys, zs, localMaxDiff);
    }
}

void computeBoundaryPlanes(const ProblemData& p,
                           const SolverData& s,
                           const Grid1D& part,
                           const std::vector<double>& cur,
                           std::vector<double>& next,
                           const std::vector<double>& xs,
                           const std::vector<double>& ys,
                           const std::vector<double>& zs,
                           double& localMaxDiff) {
    computePlane(1, p, s, part, cur, next, xs, ys, zs, localMaxDiff);

    if (part.zCount > 1) {
        computePlane(part.zCount, p, s, part, cur, next, xs, ys, zs, localMaxDiff);
    }
}

double computeMaxError(const ProblemData& p,
                       const Grid1D& part,
                       const std::vector<double>& cur,
                       const std::vector<double>& xs,
                       const std::vector<double>& ys,
                       const std::vector<double>& zs) {
    double localMaxErr = 0.0;

    for (int lk = 1; lk <= part.zCount; ++lk) {
        double z = zs[lk - 1];

        for (int j = 0; j < p.Ny; ++j) {
            double y = ys[j];
            for (int i = 0; i < p.Nx; ++i) {
                double x = xs[i];
                double exact = exactPhi(x, y, z);
                double err = std::fabs(cur[idx(i, j, lk, p.Nx, p.Ny)] - exact);
                localMaxErr = std::max(localMaxErr, err);
            }
        }
    }

    return localMaxErr;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    ProblemData p;

    if (argc >= 4) {
        p.Nx = std::stoi(argv[1]);
        p.Ny = std::stoi(argv[2]);
        p.Nz = std::stoi(argv[3]);
    }
    if (argc >= 5) {
        p.eps = std::stod(argv[4]);
    }
    if (argc >= 6) {
        p.maxIters = std::stoi(argv[5]);
    }

    if (p.Nx < 2 || p.Ny < 2 || p.Nz < 2) {
        if (rank == 0) {
            std::cerr << "Nx, Ny, Nz must be >= 2\n";
        }
        MPI_Finalize();
        return 1;
    }

    if (size > p.Nz) {
        if (rank == 0) {
            std::cerr << "Number of MPI processes must be <= Nz\n";
        }
        MPI_Finalize();
        return 1;
    }

    SolverData s = buildSolverData(p);
    Grid1D part = split1D(p.Nz, rank, size);

    int prevRank = (rank == 0 ? MPI_PROC_NULL : rank - 1);
    int nextRank = (rank == size - 1 ? MPI_PROC_NULL : rank + 1);

    std::size_t totalSize = static_cast<std::size_t>(p.Nx) * p.Ny * (part.zCount + 2);

    std::vector<double> cur(totalSize, 0.0);
    std::vector<double> next(totalSize, 0.0);

    std::vector<double> xs;
    std::vector<double> ys;
    std::vector<double> zs;

    buildCoordinates(p, s, part, xs, ys, zs);
    initializeGrid(p, part, xs, ys, zs, cur, next);

    // Заполняем halo слои до первой итерации, т.к еще нету корректных
    exchangeHalos(cur, p, part.zCount, prevRank, nextRank, 100, 101, 101, 100);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    int iter = 0;
    double globalMaxDiff = 0.0;

    do {
        ++iter;
        double localMaxDiff = 0.0;

        MPI_Request reqs[4];
        int reqCount = startHaloExchange(cur, p, part.zCount, prevRank, nextRank, 200, 201, 201, 200, reqs);

        computeInteriorPlanes(p, s, part, cur, next, xs, ys, zs, localMaxDiff);

        MPI_Waitall(reqCount, reqs, MPI_STATUSES_IGNORE);

        computeBoundaryPlanes(p, s, part, cur, next, xs, ys, zs, localMaxDiff);

        MPI_Allreduce(&localMaxDiff, &globalMaxDiff, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

        std::swap(cur, next);

    } while (globalMaxDiff >= p.eps && iter < p.maxIters);

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();

    double localMaxErr = computeMaxError(p, part, cur, xs, ys, zs);
    double globalMaxErr = 0.0;

    MPI_Reduce(&localMaxErr, &globalMaxErr, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << std::fixed << std::setprecision(10);
        std::cout << "Grid: " << p.Nx << " x " << p.Ny << " x " << p.Nz << "\n";
        std::cout << "Processes: " << size << "\n";
        std::cout << "a = " << p.a << "\n";
        std::cout << "eps = " << p.eps << "\n";
        std::cout << "iters = " << iter << "\n";
        std::cout << "max_diff = " << globalMaxDiff << "\n";
        std::cout << "max_error = " << globalMaxErr << "\n";
        std::cout << "time = " << (t1 - t0) << " sec\n";
    }

    MPI_Finalize();
    return 0;
}
