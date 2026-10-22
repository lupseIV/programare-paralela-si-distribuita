#include <iostream>
#include <thread>
#include <vector>

#include "src/Config.h"
#include "src/Orchestrator.h"
#include "bonus/BenchmarkBonus.h"

namespace {
struct Level {
    std::uint32_t warehouses;
    std::uint32_t products;
    std::uint64_t moves;
};

constexpr bool kQuickRun = false;
constexpr bool RUN_PROBLEM1 = true;
constexpr bool RUN_BONUS = true;

const std::vector<unsigned> kThreadCounts = {3, 6, 12, 24};
const std::vector<Granularity> kGranularities = {
                                                 Granularity::PerProduct,
                                                 };
constexpr std::uint32_t kWarehouses = 100;
constexpr std::uint32_t kProducts = 100;
constexpr std::uint64_t kMoves = 50'000;

constexpr bool SCALING_STEPS = true;
constexpr unsigned NR_STEPS = 2;
constexpr std::uint32_t GRID_SCALE = 2;
constexpr std::uint64_t MOVES_SCALE = 5;

constexpr std::int64_t kInitialQuantity = 1000;
constexpr std::int64_t kMaxMoveQuantity = 10;
constexpr unsigned kRuns = 10;

constexpr bool kRunRoundsExperiment = true;
constexpr std::uint64_t kRoundsMoves = 50'000;

}

int main() {
    std::cout << "Hardware threads reported: " << std::thread::hardware_concurrency() << "\n";

    Orchestrator orchestrator("results.csv");

    std::vector<Level> levels;
    const unsigned steps = SCALING_STEPS ? NR_STEPS : 1;
    Level step{kWarehouses, kProducts, kMoves};
    for (unsigned i = 0; i < steps; ++i) {
        levels.push_back(step);
        step.warehouses *= GRID_SCALE;
        step.products *= GRID_SCALE;
        step.moves *= MOVES_SCALE;
    }
    if (kQuickRun) levels.resize(1);
    if (!RUN_PROBLEM1) levels.clear();
    const unsigned runs = kQuickRun ? 2 : kRuns;

    for (const Level& level : levels) {
        {
            BenchmarkConfig base;
            base.warehouses = level.warehouses;
            base.products = level.products;
            base.initialQuantity = kInitialQuantity;
            base.maxMoveQuantity = kMaxMoveQuantity;
            base.moves = level.moves;
            base.runs = runs;

            BenchmarkConfig seq = base;
            seq.mode = ExecutionMode::Sequential;
            seq.threads = 1;
            orchestrator.runBenchmark(seq);

            for (Granularity g : kGranularities) {
                for (unsigned t : kThreadCounts) {
                    BenchmarkConfig cfg = base;
                    cfg.mode = ExecutionMode::ListPerThread;
                    cfg.granularity = g;
                    cfg.threads = t;
                    orchestrator.runBenchmark(cfg);
                }
            }
        }
    }

    if (RUN_PROBLEM1 && kRunRoundsExperiment) {
        BenchmarkConfig base;
        base.warehouses = 100;
        base.products = 100;
        base.initialQuantity = kInitialQuantity;
        base.maxMoveQuantity = kMaxMoveQuantity;
        base.moves = kQuickRun ? 5'000 : kRoundsMoves;
        base.granularity = Granularity::PerCell;
        base.runs = runs;

        BenchmarkConfig seq = base;
        seq.mode = ExecutionMode::Sequential;
        seq.threads = 1;
        orchestrator.runBenchmark(seq);

        for (unsigned t : kThreadCounts) {
            for (ExecutionMode m : {ExecutionMode::ListPerThread, ExecutionMode::OneMovePerThread}) {
                BenchmarkConfig cfg = base;
                cfg.mode = m;
                cfg.threads = t;
                orchestrator.runBenchmark(cfg);
            }
        }
    }

    if (RUN_BONUS) {
        BenchmarkBonus bonus;
        bonus.runAll();
    }

    std::cout << "\nResults appended to results.csv / results_bonus.csv (in the working directory).\n";
    return 0;
}
