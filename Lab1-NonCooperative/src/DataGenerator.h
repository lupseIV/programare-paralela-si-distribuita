#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "Move.h"

std::vector<Move> generateMoves(std::uint64_t count, std::uint32_t warehouses,
                                std::uint32_t products, std::int64_t maxQuantity,
                                std::mt19937_64& rng);

std::vector<std::vector<Move>> distributeMoves(const std::vector<Move>& moves, unsigned parts,
                                               std::mt19937_64& rng);
