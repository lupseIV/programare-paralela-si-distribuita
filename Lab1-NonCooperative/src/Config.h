#pragma once

#include <cstdint>
#include <string>

enum class Granularity {
    PerWarehouse,
    PerProduct,
    PerCell
};

enum class ExecutionMode {
    Sequential,
    ListPerThread,
    OneMovePerThread
};

struct BenchmarkConfig {
    ExecutionMode mode = ExecutionMode::ListPerThread;
    Granularity granularity = Granularity::PerCell;
    unsigned threads = 6;

    std::uint32_t warehouses = 1000;
    std::uint32_t products = 1000;
    std::int64_t initialQuantity = 1000;
    std::int64_t maxMoveQuantity = 10;
    std::uint64_t moves = 2'000'000;

    unsigned runs = 10;
};

inline constexpr int kMaxRetries = 3;
inline constexpr unsigned kCheckCount = 10;

std::string toString(Granularity g);
std::string toString(ExecutionMode m);
