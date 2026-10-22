#pragma once

#include <cstdint>
#include <string>
#include <vector>

class BenchmarkBonus {
public:
    bool runAll();

    bool testInsertAfter();
    bool testInsertBefore();
    bool testInsertSameGap();
    bool testMovesDuringInserts();
    bool testLinearizability(unsigned trials = 2000);
    void benchmark(std::uint32_t initialSize = 100'000, std::uint64_t operations = 2'000'000,
                   const std::vector<unsigned>& threadCounts = {3, 6, 12, 24}, unsigned runs = 10);

private:
    bool report(const std::string& name, bool ok, const std::string& detail);
    std::uint64_t nextSeed();
    std::uint64_t lastSeed_ = 0;
};
