#include <iostream>
#include <vector>
#include <cmath>
#include <omp.h>

void testFill_2(std::vector<double> &A, std::vector<double> &b, std::vector<double> &u, int N) {
    #pragma omp parallel for
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            A[i * N + j] = (i == j) ? 2.0 : 1.0;
        }
    }

    const double pi = std::acos(-1.0);

    #pragma omp parallel for
    for (int i = 0; i < N; ++i) {
        u[i] = std::sin(2.0 * pi * i / N);
    }

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        double sum = 0.0;
        for (int j = 0; j < N; j++) {
            sum += A[i * N + j] * u[j];
        }
        b[i] = sum;
    }
}

int main() {
    const int N = 1000;

    std::vector<double> u(N);
    std::vector<double> A(N * N);
    std::vector<double> b(N);
    testFill_2(A, b, u, N);

    std::vector<double> x(N, 0.0);
    std::vector<double> r(N);
    std::vector<double> Ax(N);
    std::vector<double> diff(N);

    const double eps = 1e-5;
    const double tau = 0.001;
    const int maxIter = 1000000;

    double t0 = omp_get_wtime();

    double normB = 0.0;
    
    #pragma omp parallel for reduction(+:normB)
    for (int i = 0; i < N; i++) {
        normB += b[i] * b[i];
    }

    normB = std::sqrt(normB);

    if (normB == 0.0) {
        std::cerr << "Wrong vector b\n";
        return 1;
    }

    double normR = 0.0;
    bool converged = false;
    int it = 0;
    double sumSquare = 0.0;
    #pragma omp parallel shared(A, b, x, Ax, r, normB, normR, converged, it)
    {
        for (int iter = 0; iter < maxIter; ++iter) {
            #pragma omp for
            for (int i = 0; i < N; i++) {
                double sum = 0.0;
                for (int j = 0; j < N; j++) {
                    sum += A[i * N + j] * x[j];
                }
                Ax[i] = sum;
            }

            #pragma omp for
            for (int i = 0; i < N; i++) {
                r[i] = Ax[i] - b[i];
            }
            
            #pragma omp for reduction(+:sumSquare)
            for (int i = 0; i < N; i++) {
                sumSquare += r[i] * r[i];
            }
            //Один раз. Поток не гарантируется. master
            #pragma omp single
            {
                normR = std::sqrt(sumSquare);

                if (normR / normB < eps) {
                    converged = true;
                }

                it = iter;
                sumSquare = 0.0;
            }
            
            if (converged) {
                break;
            }

            #pragma omp for
            for (int i = 0; i < N; i++) {
                x[i] -= tau * r[i];
            }
        }
    }

    double t1 = omp_get_wtime();

    if (!converged) {
        std::cout << "Not converged\n";
    }

    #pragma omp parallel for
    for (int i = 0; i < N; ++i) {
        diff[i] = x[i] - u[i];
    }

    double normDiff = 0.0;
    double normU = 0.0;

    #pragma omp parallel for reduction(+:normDiff, normU)
    for (int i = 0; i < N; ++i) {
        normDiff += diff[i] * diff[i];
        normU += u[i] * u[i];
    }

    normDiff = std::sqrt(normDiff);
    normU = std::sqrt(normU);

    std::cout << "threads = " << omp_get_max_threads() << "\n";
    std::cout << "time = " << (t1 - t0) << " sec\n";
    std::cout << "iters = " << it << "\n";
    std::cout << "rel_residual = " << normR / normB << "\n";
    std::cout << "rel_error = " << normDiff / normU << "\n";
    std::cout << "x[0] = " << x[0] << ", x[N-1] = " << x[N - 1] << "\n";

    return 0;
}