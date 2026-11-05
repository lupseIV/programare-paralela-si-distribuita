#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "src/Matrix.h"

namespace {

enum class Strategy { Rows, Columns, KthElement };

constexpr Strategy kStrategies[] = {Strategy::Rows, Strategy::Columns, Strategy::KthElement};

std::string_view name(Strategy s) {
    switch (s) {
        case Strategy::Rows: return "rows";
        case Strategy::Columns: return "columns";
        case Strategy::KthElement: return "kth";
    }
    return "?";
}

bool gDebug = false;
std::mutex gPrintMutex;

// Computes a single element of the result: result(row, col) = sum_k a(row, k) * b(k, col).
std::int64_t computeElement(const Matrix& a, const Matrix& b, std::size_t row, std::size_t col,
                            unsigned threadId) {
    if (gDebug) {
        std::lock_guard lock(gPrintMutex);
        std::cout << "thread " << threadId << " -> (" << row << ", " << col << ")\n";
    }
    std::int64_t sum = 0;
    for (std::size_t k = 0; k < a.cols(); ++k) {
        sum += a(row, k) * b(k, col);
    }
    return sum;
}

// Calls fn(row, col) for every element of a rows x cols result that thread `threadId` owns.
// Elements are numbered 0 .. rows*cols-1; for Rows the numbering is row-major, for Columns it is
// column-major. The consecutive strategies give each thread total/k elements, the last thread also
// takes the remainder (9x9 with 4 threads -> 20, 20, 20, 21). KthElement takes idx = t, t+k, ...
template <typename Fn>
void forEachAssigned(Strategy strategy, unsigned threadId, unsigned nrThreads, std::size_t rows,
                     std::size_t cols, Fn&& fn) {
    const std::size_t total = rows * cols;
    if (strategy == Strategy::KthElement) {
        for (std::size_t idx = threadId; idx < total; idx += nrThreads) fn(idx / cols, idx % cols);
        return;
    }
    const std::size_t chunk = total / nrThreads;
    const std::size_t begin = threadId * chunk;
    const std::size_t end = threadId + 1 == nrThreads ? total : begin + chunk;
    for (std::size_t idx = begin; idx < end; ++idx) {
        if (strategy == Strategy::Rows)
            fn(idx / cols, idx % cols);
        else
            fn(idx % rows, idx / rows);
    }
}

// Thread function: computes, one by one, all elements assigned to `threadId`.
void worker(const Matrix& a, const Matrix& b, Matrix& result, Strategy strategy, unsigned threadId,
            unsigned nrThreads) {
    forEachAssigned(strategy, threadId, nrThreads, result.rows(), result.cols(),
                    [&](std::size_t row, std::size_t col) {
                        result(row, col) = computeElement(a, b, row, col, threadId);
                    });
}

Matrix multiplyParallel(const Matrix& a, const Matrix& b, Strategy strategy, unsigned nrThreads) {
    Matrix result(a.rows(), b.cols());
    std::vector<std::thread> threads;
    threads.reserve(nrThreads);
    for (unsigned t = 0; t < nrThreads; ++t) {
        threads.emplace_back(worker, std::cref(a), std::cref(b), std::ref(result), strategy, t,
                             nrThreads);
    }
    for (auto& th : threads) th.join();
    return result;
}

Matrix multiplySequential(const Matrix& a, const Matrix& b) {
    Matrix result(a.rows(), b.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t k = 0; k < a.cols(); ++k)
            for (std::size_t j = 0; j < b.cols(); ++j)
                result(i, j) += a(i, k) * b(k, j);
    return result;
}

Matrix randomMatrix(std::size_t rows, std::size_t cols, std::mt19937_64& rng) {
    std::uniform_int_distribution<std::int64_t> dist(-100, 100);
    Matrix m(rows, cols);
    for (std::size_t i = 0; i < rows; ++i)
        for (std::size_t j = 0; j < cols; ++j) m(i, j) = dist(rng);
    return m;
}

// Prints, for every strategy, which thread computed which element, plus a map of the result matrix
// showing the owning thread of each cell.
int runDebug(std::size_t n, unsigned nrThreads) {
    std::mt19937_64 rng(42);
    const Matrix a = randomMatrix(n, n, rng);
    const Matrix b = randomMatrix(n, n, rng);
    const Matrix expected = multiplySequential(a, b);
    bool allCorrect = true;

    for (const Strategy s : kStrategies) {
        std::cout << "=== strategy: " << name(s) << ", " << n << "x" << n << ", " << nrThreads
                  << " threads ===\n";
        gDebug = true;
        const Matrix result = multiplyParallel(a, b, s, nrThreads);
        gDebug = false;

        std::vector<int> owner(n * n, -1);
        std::vector<std::size_t> counts(nrThreads, 0);
        for (unsigned t = 0; t < nrThreads; ++t) {
            forEachAssigned(s, t, nrThreads, n, n, [&](std::size_t row, std::size_t col) {
                owner[row * n + col] = static_cast<int>(t);
                ++counts[t];
            });
        }
        std::cout << "elements per thread:";
        for (unsigned t = 0; t < nrThreads; ++t) std::cout << ' ' << counts[t];
        std::cout << "\nowner map (thread id per cell):\n";
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) std::cout << std::setw(3) << owner[i * n + j];
            std::cout << '\n';
        }
        const bool ok = result == expected;
        allCorrect = allCorrect && ok;
        std::cout << "correct: " << (ok ? "yes" : "NO") << "\n\n";
    }
    return allCorrect ? 0 : 1;
}

int runBenchmark(unsigned runs) {
    const std::vector<std::size_t> sizes = {100, 500, 1000, 2500, 5000, 10000};
    const std::vector<unsigned> threadCounts = {1, 2, 4, 8, 16, 32, 64, 256};

    std::cout << "Hardware threads: " << std::thread::hardware_concurrency()
              << ", runs per configuration: " << runs << "\n\n"
              << std::left << std::setw(8) << "size" << std::setw(10) << "strategy" << std::setw(10)
              << "threads" << std::setw(14) << "avg_ms" << std::setw(14) << "min_ms" << "ok\n";

    std::ofstream csv("results.csv");
    csv << "size,strategy,threads,avg_ms,min_ms,correct\n";

    std::mt19937_64 rng(42);
    bool allCorrect = true;

    for (const std::size_t n : sizes) {
        const Matrix a = randomMatrix(n, n, rng);
        const Matrix b = randomMatrix(n, n, rng);
        const Matrix expected = multiplySequential(a, b);

        for (const Strategy s : kStrategies) {
            for (const unsigned threads : threadCounts) {
                double total = 0.0;
                double best = 0.0;
                bool correct = true;
                for (unsigned run = 0; run < runs; ++run) {
                    const auto start = std::chrono::steady_clock::now();
                    const Matrix result = multiplyParallel(a, b, s, threads);
                    const auto end = std::chrono::steady_clock::now();
                    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
                    total += ms;
                    best = run == 0 ? ms : std::min(best, ms);
                    correct = correct && result == expected;
                }
                allCorrect = allCorrect && correct;
                const double avg = total / runs;

                std::cout << std::left << std::setw(8) << n << std::setw(10) << name(s)
                          << std::setw(10) << threads << std::fixed << std::setprecision(3)
                          << std::setw(14) << avg << std::setw(14) << best
                          << (correct ? "yes" : "NO") << "\n";
                csv << n << ',' << name(s) << ',' << threads << ',' << avg << ',' << best << ','
                    << (correct ? 1 : 0) << '\n';
            }
        }
        std::cout << '\n';
    }

    std::cout << "Results written to results.csv\n";
    return allCorrect ? 0 : 1;
}

}  // namespace

// Usage:
//   Lab3_SimpleParallelTasks                      -> benchmark (writes results.csv)
//   Lab3_SimpleParallelTasks --runs R             -> benchmark with R runs per configuration
//   Lab3_SimpleParallelTasks --debug [n] [threads]-> debug trace (default 9x9, 4 threads)
int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (!args.empty() && args[0] == "--debug") {
        const std::size_t n = args.size() > 1 ? std::stoul(args[1]) : 9;
        const unsigned threads = args.size() > 2 ? static_cast<unsigned>(std::stoul(args[2])) : 4;
        return runDebug(n, threads);
    }
    unsigned runs = 3;
    if (args.size() > 1 && args[0] == "--runs") runs = static_cast<unsigned>(std::stoul(args[1]));
    return runBenchmark(runs);
}
