#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include "src/BoundedQueue.h"

namespace {

constexpr std::size_t kVectorSize = 1'000'000;
constexpr unsigned kRuns = 5;
const std::vector<std::size_t> kQueueSizes = {1, 2, 4, 8, 16, 64, 256, 1024, 10'000, 100'000};

std::vector<std::int64_t> randomVector(std::size_t n, std::mt19937_64& rng) {
    std::uniform_int_distribution<std::int64_t> dist(-100, 100);
    std::vector<std::int64_t> v(n);
    for (auto& x : v) x = dist(rng);
    return v;
}

struct RunResult {
    std::int64_t sum;
    double millis;
};

RunResult scalarProduct(const std::vector<std::int64_t>& a,
                        const std::vector<std::int64_t>& b,
                        std::size_t queueSize) {
    BoundedQueue<std::int64_t> queue(queueSize);
    std::int64_t sum = 0;

    const auto start = std::chrono::steady_clock::now();

    std::thread producer([&] {
        for (std::size_t i = 0; i < a.size(); ++i) {
            queue.push(a[i] * b[i]);
        }
        queue.close();
    });

    std::thread consumer([&] {
        while (auto product = queue.pop()) {
            sum += *product;
        }
    });

    producer.join();
    consumer.join();

    const auto end = std::chrono::steady_clock::now();
    return {sum, std::chrono::duration<double, std::milli>(end - start).count()};
}

}

int main() {
    std::mt19937_64 rng(42);
    const auto a = randomVector(kVectorSize, rng);
    const auto b = randomVector(kVectorSize, rng);
    const std::int64_t expected = std::inner_product(a.begin(), a.end(), b.begin(), std::int64_t{0});

    std::cout << "Vector size: " << kVectorSize << ", runs per queue size: " << kRuns << "\n"
              << "Expected (sequential): " << expected << "\n\n"
              << std::left << std::setw(12) << "queue_size" << std::setw(14) << "avg_ms"
              << std::setw(14) << "min_ms" << "ok\n";

    std::ofstream csv("results.csv");
    csv << "queue_size,avg_ms,min_ms,correct\n";

    bool allCorrect = true;

    for (const std::size_t queueSize : kQueueSizes) {
        double total = 0.0;
        double best = 0.0;
        bool correct = true;
        for (unsigned run = 0; run < kRuns; ++run) {
            const RunResult r = scalarProduct(a, b, queueSize);
            total += r.millis;
            best = run == 0 ? r.millis : std::min(best, r.millis);
            correct = correct && r.sum == expected;
        }
        allCorrect = allCorrect && correct;
        const double avg = total / kRuns;

        std::cout << std::left << std::setw(12) << queueSize << std::fixed << std::setprecision(2)
                  << std::setw(14) << avg << std::setw(14) << best << (correct ? "yes" : "NO") << "\n";
        csv << queueSize << ',' << avg << ',' << best << ',' << (correct ? 1 : 0) << '\n';
    }

    std::cout << "\nResults written to results.csv\n";
    return allCorrect ? 0 : 1;
}
