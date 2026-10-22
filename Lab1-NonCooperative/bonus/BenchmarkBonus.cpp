#include "BenchmarkBonus.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>

#include "ConcurrentList.h"

namespace {

using Clock = std::chrono::steady_clock;
using Node = ConcurrentList::Node;

void runThreads(unsigned n, const std::function<void(unsigned)>& body) {
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (unsigned t = 0; t < n; ++t)
        threads.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            body(t);
        });
    go.store(true, std::memory_order_release);
    for (auto& th : threads) th.join();
}

std::vector<Node*> nodesOf(ConcurrentList& list) {
    std::vector<Node*> nodes;
    for (Node* n = list.first(); n; n = list.moveNext(n)) nodes.push_back(n);
    return nodes;
}

std::vector<std::int64_t> range(std::int64_t n) {
    std::vector<std::int64_t> v(n);
    for (std::int64_t i = 0; i < n; ++i) v[i] = i;
    return v;
}

std::int64_t tag(unsigned thread, std::uint64_t k) {
    return static_cast<std::int64_t>(thread + 1) * 1'000'000'000LL + static_cast<std::int64_t>(k);
}

bool structureOk(const ConcurrentList& list, std::size_t expectedSize, std::string& detail) {
    if (!list.validate(detail)) return false;
    auto f = list.forward();
    auto b = list.backward();
    std::reverse(b.begin(), b.end());
    if (f != b) { detail = "forward and backward traversals differ"; return false; }
    if (f.size() != expectedSize) {
        detail = "size " + std::to_string(f.size()) + " != expected " + std::to_string(expectedSize);
        return false;
    }
    std::set<std::int64_t> unique(f.begin(), f.end());
    if (unique.size() != f.size()) { detail = "duplicate element"; return false; }
    return true;
}

bool originalsInOrder(const std::vector<std::int64_t>& seq, std::int64_t originals) {
    std::int64_t expected = 0;
    for (std::int64_t v : seq)
        if (v < originals) {
            if (v != expected) return false;
            ++expected;
        }
    return expected == originals;
}

bool threadOrder(const std::vector<std::int64_t>& seq, unsigned threads, bool decreasing) {
    for (unsigned t = 0; t < threads; ++t) {
        std::int64_t prev = -1;
        bool first = true;
        for (std::int64_t v : seq) {
            if (v / 1'000'000'000LL != t + 1) continue;
            if (!first && (decreasing ? v >= prev : v <= prev)) return false;
            prev = v;
            first = false;
        }
    }
    return true;
}

enum class Op { Next, Prev, InsertAfter, InsertBefore };

struct SmallOp {
    Op op;
    std::size_t target;
    std::int64_t value;
};

std::int64_t applyModel(std::vector<std::int64_t>& list, const SmallOp& o, std::int64_t targetValue) {
    auto it = std::find(list.begin(), list.end(), targetValue);
    std::size_t i = static_cast<std::size_t>(it - list.begin());
    switch (o.op) {
        case Op::Next: return i + 1 < list.size() ? list[i + 1] : 0;
        case Op::Prev: return i > 0 ? list[i - 1] : 0;
        case Op::InsertAfter: list.insert(list.begin() + static_cast<std::ptrdiff_t>(i) + 1, o.value); return 0;
        case Op::InsertBefore: list.insert(list.begin() + static_cast<std::ptrdiff_t>(i), o.value); return 0;
    }
    return 0;
}

bool someInterleavingMatches(const std::vector<std::vector<SmallOp>>& programs,
                             const std::vector<std::vector<std::int64_t>>& observed,
                             const std::vector<std::int64_t>& initial,
                             const std::vector<std::int64_t>& finalList) {
    std::vector<std::size_t> pos(programs.size(), 0);
    std::vector<std::int64_t> model = initial;
    std::function<bool()> search = [&]() -> bool {
        bool done = true;
        for (std::size_t t = 0; t < programs.size(); ++t) {
            if (pos[t] == programs[t].size()) continue;
            done = false;
            const SmallOp& o = programs[t][pos[t]];
            std::vector<std::int64_t> saved = model;
            std::int64_t r = applyModel(model, o, initial[o.target]);
            if (r == observed[t][pos[t]]) {
                ++pos[t];
                if (search()) return true;
                --pos[t];
            }
            model = std::move(saved);
        }
        return done && model == finalList;
    };
    return search();
}

}  // namespace

bool BenchmarkBonus::report(const std::string& name, bool ok, const std::string& detail) {
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << name;
    if (!detail.empty()) std::cout << " - " << detail;
    std::cout << "\n";
    return ok;
}

std::uint64_t BenchmarkBonus::nextSeed() {
    auto ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    lastSeed_ = std::max(ms, lastSeed_ + 1);
    return lastSeed_;
}

bool BenchmarkBonus::testInsertAfter() {
    const unsigned T = 8;
    const std::uint64_t K = 20'000;
    ConcurrentList list(range(10));
    auto nodes = nodesOf(list);
    runThreads(T, [&](unsigned t) {
        Node* anchor = (t % 2 == 0) ? nodes[0] : nodes[t];
        for (std::uint64_t k = 0; k < K; ++k) list.insertAfter(anchor, tag(t, k));
    });
    std::string detail;
    bool ok = structureOk(list, 10 + T * K, detail);
    auto seq = list.forward();
    if (ok && !originalsInOrder(seq, 10)) { ok = false; detail = "original elements reordered"; }
    if (ok && !threadOrder(seq, T, true)) { ok = false; detail = "a thread's inserts are out of order"; }
    return report("insertAfter: 8 threads x 20000 inserts (4 on the same node)", ok, detail);
}

bool BenchmarkBonus::testInsertBefore() {
    const unsigned T = 8;
    const std::uint64_t K = 20'000;
    ConcurrentList list(range(10));
    auto nodes = nodesOf(list);
    runThreads(T, [&](unsigned t) {
        Node* anchor = (t % 2 == 0) ? nodes[9] : nodes[t];
        for (std::uint64_t k = 0; k < K; ++k) list.insertBefore(anchor, tag(t, k));
    });
    std::string detail;
    bool ok = structureOk(list, 10 + T * K, detail);
    auto seq = list.forward();
    if (ok && !originalsInOrder(seq, 10)) { ok = false; detail = "original elements reordered"; }
    if (ok && !threadOrder(seq, T, false)) { ok = false; detail = "a thread's inserts are out of order"; }
    return report("insertBefore: 8 threads x 20000 inserts (4 on the same node)", ok, detail);
}

bool BenchmarkBonus::testInsertSameGap() {
    const unsigned T = 8;
    const std::uint64_t K = 20'000;
    ConcurrentList list(range(2));
    auto nodes = nodesOf(list);
    runThreads(T, [&](unsigned t) {
        for (std::uint64_t k = 0; k < K; ++k) {
            if (t % 2 == 0) list.insertAfter(nodes[0], tag(t, k));
            else list.insertBefore(nodes[1], tag(t, k));
        }
    });
    std::string detail;
    bool ok = structureOk(list, 2 + T * K, detail);
    auto seq = list.forward();
    if (ok && (seq.front() != 0 || seq.back() != 1)) { ok = false; detail = "inserts escaped the gap"; }
    if (ok) {
        for (unsigned t = 0; t < T && ok; ++t) {
            std::int64_t prev = -1;
            bool first = true;
            for (std::int64_t v : seq) {
                if (v / 1'000'000'000LL != t + 1) continue;
                bool bad = !first && (t % 2 == 0 ? v >= prev : v <= prev);
                if (bad) { ok = false; detail = "a thread's inserts are out of order"; break; }
                prev = v;
                first = false;
            }
        }
    }
    return report("insertAfter(A) and insertBefore(B) racing on the same gap A-B", ok, detail);
}

bool BenchmarkBonus::testMovesDuringInserts() {
    const std::int64_t N = 1000;
    const unsigned writers = 4, readers = 4;
    const std::uint64_t K = 20'000;
    const unsigned walks = 30;
    ConcurrentList list(range(N));
    auto nodes = nodesOf(list);
    std::atomic<bool> failed{false};
    std::string failure;
    std::mutex failureMutex;
    auto fail = [&](const std::string& why) {
        std::lock_guard<std::mutex> lk(failureMutex);
        if (!failed.exchange(true)) failure = why;
    };
    runThreads(writers + readers, [&](unsigned t) {
        if (t < writers) {
            std::mt19937_64 rng(t);
            std::uniform_int_distribution<std::size_t> pick(0, nodes.size() - 1);
            for (std::uint64_t k = 0; k < K; ++k) {
                Node* n = nodes[pick(rng)];
                if (k % 2) list.insertAfter(n, tag(t, k));
                else list.insertBefore(n, tag(t, k));
            }
            return;
        }
        for (unsigned w = 0; w < walks && !failed; ++w) {
            std::vector<std::int64_t> seq;
            for (Node* n = list.first(); n; n = list.moveNext(n)) seq.push_back(n->value);
            if (!originalsInOrder(seq, N)) fail("forward walk saw original elements out of order");
            seq.clear();
            for (Node* n = list.last(); n; n = list.movePrev(n)) seq.push_back(n->value);
            std::reverse(seq.begin(), seq.end());
            if (!originalsInOrder(seq, N)) fail("backward walk saw original elements out of order");
        }
    });
    std::string detail;
    bool ok = !failed && structureOk(list, N + writers * K, detail);
    if (failed) detail = failure;
    return report("moveNext/movePrev walks while 4 threads insert (4 readers x 30 walks each way)", ok, detail);
}

bool BenchmarkBonus::testLinearizability(unsigned trials) {
    const std::vector<std::int64_t> initial = {1, 2, 3};
    const unsigned T = 3, opsPerThread = 3;
    std::mt19937_64 rng(nextSeed());
    std::uniform_int_distribution<int> pickOp(0, 3);
    std::uniform_int_distribution<std::size_t> pickNode(0, initial.size() - 1);
    unsigned passed = 0;
    for (unsigned trial = 0; trial < trials; ++trial) {
        std::vector<std::vector<SmallOp>> programs(T);
        for (unsigned t = 0; t < T; ++t)
            for (unsigned k = 0; k < opsPerThread; ++k)
                programs[t].push_back({static_cast<Op>(pickOp(rng)), pickNode(rng),
                                       static_cast<std::int64_t>(100 + t * 10 + k)});
        ConcurrentList list(initial);
        auto nodes = nodesOf(list);
        std::vector<std::vector<std::int64_t>> observed(T, std::vector<std::int64_t>(opsPerThread, 0));
        runThreads(T, [&](unsigned t) {
            for (unsigned k = 0; k < opsPerThread; ++k) {
                const SmallOp& o = programs[t][k];
                Node* n = nodes[o.target];
                Node* r = nullptr;
                switch (o.op) {
                    case Op::Next: r = list.moveNext(n); break;
                    case Op::Prev: r = list.movePrev(n); break;
                    case Op::InsertAfter: list.insertAfter(n, o.value); break;
                    case Op::InsertBefore: list.insertBefore(n, o.value); break;
                }
                observed[t][k] = r ? r->value : 0;
            }
        });
        std::string detail;
        if (structureOk(list, initial.size() + [&] {
                std::size_t inserts = 0;
                for (auto& p : programs)
                    for (auto& o : p) inserts += (o.op == Op::InsertAfter || o.op == Op::InsertBefore);
                return inserts;
            }(), detail) &&
            someInterleavingMatches(programs, observed, initial, list.forward()))
            ++passed;
    }
    return report("linearizability: " + std::to_string(trials) +
                      " random 3-thread programs matched against all sequential orders",
                  passed == trials, std::to_string(passed) + "/" + std::to_string(trials) + " matched");
}

void BenchmarkBonus::benchmark(std::uint32_t initialSize, std::uint64_t operations,
                               const std::vector<unsigned>& threadCounts, unsigned runs) {
    std::cout << "\nBonus benchmark: list of " << initialSize << " elements, " << operations
              << " operations (40% moveNext, 40% movePrev, 10% insertAfter, 10% insertBefore), "
              << runs << " runs each\n";
    std::vector<unsigned> counts = {1};
    counts.insert(counts.end(), threadCounts.begin(), threadCounts.end());
    std::ofstream csv("results_bonus.csv", std::ios::app);
    csv << "threads,initial_size,operations,runs,mean_ms,all_valid,seeds\n";

    for (unsigned T : counts) {
        std::vector<double> times;
        std::vector<std::uint64_t> seeds;
        bool allValid = true;
        for (unsigned r = 0; r < runs; ++r) {
            const std::uint64_t seed = nextSeed();
            seeds.push_back(seed);
            std::mt19937_64 rng(seed);
            ConcurrentList list(range(initialSize));
            auto nodes = nodesOf(list);
            std::vector<std::vector<std::uint8_t>> ops(T);
            std::uniform_int_distribution<int> d(0, 9);
            std::uint64_t inserts = 0;
            for (unsigned t = 0; t < T; ++t) {
                const std::uint64_t count = operations / T + (t < operations % T ? 1 : 0);
                ops[t].resize(count);
                for (auto& o : ops[t]) {
                    int x = d(rng);
                    o = static_cast<std::uint8_t>(x < 4 ? 0 : x < 8 ? 1 : x < 9 ? 2 : 3);
                    inserts += (o >= 2);
                }
            }
            auto start = Clock::now();
            runThreads(T, [&](unsigned t) {
                Node* cursor = nodes[static_cast<std::size_t>(t) * initialSize / T];
                std::uint64_t k = 0;
                for (std::uint8_t o : ops[t]) {
                    Node* moved = nullptr;
                    switch (o) {
                        case 0: moved = list.moveNext(cursor); break;
                        case 1: moved = list.movePrev(cursor); break;
                        case 2: list.insertAfter(cursor, tag(t, k++)); break;
                        default: list.insertBefore(cursor, tag(t, k++)); break;
                    }
                    if (moved) cursor = moved;
                }
            });
            times.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
            std::string detail;
            if (!structureOk(list, initialSize + inserts, detail)) allValid = false;
        }
        double mean = 0;
        for (double x : times) mean += x;
        mean /= static_cast<double>(times.size());
        std::cout << "  " << (T == 1 ? "sequential (1 thread)" : std::to_string(T) + " threads")
                  << std::string(T == 1 ? 1 : (T < 10 ? 13 : 12), ' ') << std::fixed << std::setprecision(2)
                  << "mean " << mean << " ms  " << (allValid ? "LIST VALID" : "LIST CORRUPTED") << "\n";
        csv << T << ',' << initialSize << ',' << operations << ',' << runs << ',' << std::fixed
            << std::setprecision(3) << mean << ','
            << (allValid ? "yes" : "no") << ',';
        for (std::size_t i = 0; i < seeds.size(); ++i) csv << (i ? ";" : "") << seeds[i];
        csv << '\n';
    }
}

bool BenchmarkBonus::runAll() {
    std::cout << "\n=== Bonus: concurrent doubly linked list - thread-safety tests ===\n";
    bool ok = true;
    ok &= testInsertAfter();
    ok &= testInsertBefore();
    ok &= testInsertSameGap();
    ok &= testMovesDuringInserts();
    ok &= testLinearizability();
    std::cout << (ok ? "All bonus tests passed.\n" : "SOME BONUS TESTS FAILED.\n");
    benchmark();
    return ok;
}
