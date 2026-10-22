#include "Orchestrator.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "Checker.h"
#include "DataGenerator.h"
#include "Inventory.h"
#include "LockStrategy.h"
#include "Worker.h"

namespace {
using Clock = std::chrono::steady_clock;

double elapsedMs(Clock::time_point from) {
    return std::chrono::duration<double, std::milli>(Clock::now() - from).count();
}

struct CheckSchedule {
    std::uint64_t step;
    unsigned next = 1;
    explicit CheckSchedule(std::uint64_t totalMoves) : step(std::max<std::uint64_t>(1, totalMoves / kCheckCount)) {}
    unsigned crossed(std::uint64_t successes) {
        unsigned n = 0;
        while (next < kCheckCount && successes >= next * step) { ++next; ++n; }
        return n;
    }
};

void checkInThread(Checker& checker) {
    std::thread t([&checker] { checker.checkAll(); });
    t.join();
}

std::deque<PendingMove> toQueue(const std::vector<Move>& moves) {
    std::deque<PendingMove> q;
    for (const Move& m : moves) q.push_back(PendingMove{m, 0});
    return q;
}

}

void Orchestrator::validate(const BenchmarkConfig& c) {
    if (c.warehouses < 2) throw std::invalid_argument("warehouses must be >= 2");
    if (c.products < 1) throw std::invalid_argument("products must be >= 1");
    if (c.moves < 1) throw std::invalid_argument("moves must be >= 1");
    if (c.maxMoveQuantity < 1) throw std::invalid_argument("maxMoveQuantity must be >= 1");
    if (c.initialQuantity < 0) throw std::invalid_argument("initialQuantity must be >= 0");
    if (c.runs < 1) throw std::invalid_argument("runs must be >= 1");
    if (c.mode == ExecutionMode::Sequential && c.threads != 1)
        throw std::invalid_argument("sequential mode requires threads == 1");
    if (c.mode != ExecutionMode::Sequential && c.threads < 1)
        throw std::invalid_argument("threads must be >= 1");
}

std::uint64_t Orchestrator::nextSeed() {
    auto ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    lastSeed_ = std::max(ms, lastSeed_ + 1);
    return lastSeed_;
}

RunResult Orchestrator::runOnce(const BenchmarkConfig& c, std::uint64_t seed) {
    RunResult result;
    result.seed = seed;
    std::mt19937_64 rng(seed);

    Inventory inv(c.warehouses, c.products, c.initialQuantity);
    const std::vector<std::int64_t> expected = inv.allTotals();
    const std::vector<Move> moves =
        generateMoves(c.moves, c.warehouses, c.products, c.maxMoveQuantity, rng);

    std::unique_ptr<LockStrategy> locks;
    if (c.mode != ExecutionMode::Sequential)
        locks = makeLockStrategy(c.granularity, c.warehouses, c.products);

    Checker checker(inv, locks.get(), expected);
    CheckSchedule schedule(c.moves);

    if (c.mode == ExecutionMode::Sequential) {
        std::deque<PendingMove> queue = toQueue(moves);
        WorkerStats stats;
        auto start = Clock::now();
        runWorker<false>(inv, nullptr, queue, stats, [&](std::uint64_t successes) {
            for (unsigned n = schedule.crossed(successes); n > 0; --n) checker.checkAll();
        });
        result.finalCheckOk = checker.checkAll();
        result.milliseconds = elapsedMs(start);
        result.succeeded = stats.succeeded.load();
        result.unfulfilled = stats.unfulfilled;
        result.retries = stats.retries;
    } else if (c.mode == ExecutionMode::ListPerThread) {
        const unsigned n = c.threads;
        std::vector<std::deque<PendingMove>> queues;
        for (auto& list : distributeMoves(moves, n, rng)) queues.push_back(toQueue(list));
        std::vector<WorkerStats> stats(n);
        std::atomic<unsigned> finished{0};

        auto start = Clock::now();
        std::vector<std::thread> workers;
        workers.reserve(n);
        for (unsigned t = 0; t < n; ++t) {
            workers.emplace_back([&, t] {
                runWorker<true>(inv, locks.get(), queues[t], stats[t], [](std::uint64_t) {});
                finished.fetch_add(1, std::memory_order_release);
            });
        }

        while (finished.load(std::memory_order_acquire) < n) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::uint64_t total = 0;
            for (const auto& s : stats) total += s.succeeded.load(std::memory_order_relaxed);
            for (unsigned k = schedule.crossed(total); k > 0; --k) checkInThread(checker);
        }
        for (auto& w : workers) w.join();
        std::uint64_t total = 0;
        for (const auto& s : stats) total += s.succeeded.load();
        for (unsigned k = schedule.crossed(total); k > 0; --k) checkInThread(checker);
        result.finalCheckOk = checker.checkAll();
        result.milliseconds = elapsedMs(start);

        for (const auto& s : stats) {
            result.succeeded += s.succeeded.load();
            result.unfulfilled += s.unfulfilled;
            result.retries += s.retries;
        }
    } else {
        const unsigned n = c.threads;
        std::deque<PendingMove> pool = toQueue(moves);
        std::vector<PendingMove> batch;
        std::vector<char> ok;
        std::vector<std::thread> threads;

        auto start = Clock::now();
        while (!pool.empty()) {
            const std::size_t k = std::min<std::size_t>(n, pool.size());
            batch.assign(pool.begin(), pool.begin() + static_cast<std::ptrdiff_t>(k));
            pool.erase(pool.begin(), pool.begin() + static_cast<std::ptrdiff_t>(k));
            ok.assign(k, 0);
            threads.clear();
            for (std::size_t i = 0; i < k; ++i)
                threads.emplace_back([&, i] { ok[i] = attemptMove<true>(inv, locks.get(), batch[i].move); });
            for (auto& t : threads) t.join();
            for (std::size_t i = 0; i < k; ++i) {
                if (ok[i]) {
                    ++result.succeeded;
                } else if (batch[i].retries < kMaxRetries) {
                    ++batch[i].retries;
                    ++result.retries;
                    pool.push_back(batch[i]);
                } else {
                    ++result.unfulfilled;
                }
            }
            for (unsigned x = schedule.crossed(result.succeeded); x > 0; --x) checkInThread(checker);
        }
        result.finalCheckOk = checker.checkAll();
        result.milliseconds = elapsedMs(start);
    }

    result.periodicChecks = checker.checksDone() - 1;
    result.failedChecks = checker.checksFailed();
    return result;
}

BenchmarkResult Orchestrator::runBenchmark(const BenchmarkConfig& c) {
    validate(c);
    BenchmarkResult br;
    br.config = c;

    const std::string gran = c.mode == ExecutionMode::Sequential ? "none" : toString(c.granularity);
    std::cout << "\n== " << toString(c.mode) << " | " << gran << " | threads=" << c.threads
              << " | W=" << c.warehouses << " P=" << c.products << " | moves=" << c.moves
              << " | stock=" << c.initialQuantity << " | q<=" << c.maxMoveQuantity << "\n";

    for (unsigned r = 0; r < c.runs; ++r) {
        RunResult rr = runOnce(c, nextSeed());
        std::cout << "  run " << std::setw(2) << r + 1 << "  seed=" << rr.seed << "  "
                  << std::fixed << std::setprecision(2) << std::setw(9) << rr.milliseconds << " ms"
                  << "  ok=" << rr.succeeded << "  unfulfilled=" << rr.unfulfilled
                  << "  retries=" << rr.retries << "  checks=" << rr.periodicChecks << "+1"
                  << (rr.failedChecks == 0 && rr.finalCheckOk ? "  INVARIANTS OK" : "  INVARIANT VIOLATED")
                  << "\n";
        if (rr.failedChecks != 0 || !rr.finalCheckOk) br.allChecksOk = false;
        br.runs.push_back(rr);
    }

    double sum = 0, sumUnf = 0;
    for (const auto& r : br.runs) {
        sum += r.milliseconds;
        sumUnf += static_cast<double>(r.unfulfilled);
    }
    br.meanMs = sum / static_cast<double>(br.runs.size());
    br.meanUnfulfilled = sumUnf / static_cast<double>(br.runs.size());

    std::cout << "  mean " << std::fixed << std::setprecision(2) << br.meanMs << " ms  mean unfulfilled "
              << br.meanUnfulfilled << "\n";
    appendCsv(br);
    return br;
}

void Orchestrator::appendCsv(const BenchmarkResult& r) const {
    bool writeHeader;
    {
        std::ifstream probe(csvPath_);
        writeHeader = !probe.good() || probe.peek() == std::ifstream::traits_type::eof();
    }
    std::ofstream out(csvPath_, std::ios::app);
    if (!out) {
        std::cerr << "cannot write " << csvPath_ << "\n";
        return;
    }
    if (writeHeader)
        out << "mode,granularity,threads,warehouses,products,initial_quantity,max_move_quantity,moves,"
               "runs,mean_ms,mean_unfulfilled,all_checks_ok,seeds\n";
    const auto& c = r.config;
    std::ostringstream seeds;
    for (std::size_t i = 0; i < r.runs.size(); ++i) seeds << (i ? ";" : "") << r.runs[i].seed;
    out << toString(c.mode) << ','
        << (c.mode == ExecutionMode::Sequential ? "none" : toString(c.granularity)) << ','
        << c.threads << ',' << c.warehouses << ',' << c.products << ',' << c.initialQuantity << ','
        << c.maxMoveQuantity << ',' << c.moves << ',' << c.runs << ','
        << std::fixed << std::setprecision(3) << r.meanMs << ','
        << r.meanUnfulfilled << ',' << (r.allChecksOk ? "yes" : "no") << ','
        << seeds.str() << '\n';
}
