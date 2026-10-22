#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Config.h"

struct RunResult {
    std::uint64_t seed = 0;
    double milliseconds = 0;
    std::uint64_t succeeded = 0;
    std::uint64_t unfulfilled = 0;
    std::uint64_t retries = 0;
    unsigned periodicChecks = 0;
    unsigned failedChecks = 0;
    bool finalCheckOk = false;
};

struct BenchmarkResult {
    BenchmarkConfig config;
    std::vector<RunResult> runs;
    double meanMs = 0;
    double meanUnfulfilled = 0;
    bool allChecksOk = true;
};

class Orchestrator {
public:
    explicit Orchestrator(std::string csvPath) : csvPath_(std::move(csvPath)) {}

    BenchmarkResult runBenchmark(const BenchmarkConfig& config);

    static RunResult runOnce(const BenchmarkConfig& config, std::uint64_t seed);

    static void validate(const BenchmarkConfig& config);

private:
    std::uint64_t nextSeed();
    void appendCsv(const BenchmarkResult& r) const;

    std::string csvPath_;
    std::uint64_t lastSeed_ = 0;
};
