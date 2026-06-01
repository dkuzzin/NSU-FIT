#include <iostream>
#include <vector>
#include <cmath>
#include <omp.h>


void mulMatVec(const std::vector<double> &A, const std::vector<double> &x, std::vector<double> &Ax, int N){
    //Пералелим по строкам, строки независимые, сумма объясвлена внутри каждой итерации
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        double sum = 0.0;
        for (int j = 0; j < N; j++) {
            sum += A[i * N + j] * x[j];
        }
        Ax[i] = sum;
    }
}

void vecSub(const std::vector<double> &Ax, const std::vector<double> &b, std::vector<double> &r, int N){
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        r[i] = Ax[i] - b[i];
    }
}

double norm(const std::vector<double> &x){
    double sum = 0.0;
    int len = static_cast<int>(x.size());
    //Каждому потоку локальную сумму, потом складываем
    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < len; i++) {
        sum += x[i] * x[i];
    }

    return std::sqrt(sum);
}


void testFill_2(std::vector<double> &A, std::vector<double> &b, std::vector<double> &u, int N)
{
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

    mulMatVec(A, u, b, N);
}

int main()
{
    const int N = 1000;

    std::vector<double> u(N);

    std::vector<double> A(N * N);
    std::vector<double> b(N);
    testFill_2(A, b, u, N);

    std::vector<double> x(N, 0.0);
    std::vector<double> r(N);
    std::vector<double> Ax(N);

    const double eps = 1e-5;
    const double tau = 0.001;
    const int maxIter = 1000000;

    
    
    double t0 = omp_get_wtime();

    const double normB = norm(b);
    double normR = 0.0;

    if (normB == 0.0) {
        std::cerr << "Wrong vector b\n";
        return 1;
    }

    bool converged = false;
    int it = 0;

    for (; it < maxIter; ++it) {
        mulMatVec(A, x, Ax, N);
        vecSub(Ax, b, r, N);
        normR = norm(r);

        if (normR / normB < eps) {
            converged = true;
            break;
        }

        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            x[i] -= tau * r[i];
        }
    }

    double t1 = omp_get_wtime();

    if (!converged) {
        std::cout << "Not converged\n";
    }

    std::vector<double> diff(N);
    #pragma omp parallel for
    for (int i = 0; i < N; ++i) {
        diff[i] = x[i] - u[i];
    }

    std::cout << "threads = " << omp_get_max_threads() << "\n";
    std::cout << "time = " << (t1 - t0) << " sec\n";
    std::cout << "iters = " << it << "\n";
    std::cout << "rel_residual = " << normR / normB << "\n";
    std::cout << "x[0] = " << x[0] << ", x[N-1] = " << x[N - 1] << "\n";

    return 0;
}