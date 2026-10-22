#pragma once

#include <cstdint>

struct Move {
    std::uint32_t product;
    std::uint32_t from;
    std::uint32_t to;
    std::int64_t quantity;
};

struct PendingMove {
    Move move{};
    int retries = 0;
};
