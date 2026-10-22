#include "DataGenerator.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

std::vector<Move> generateMoves(std::uint64_t count, std::uint32_t warehouses,
                                std::uint32_t products, std::int64_t maxQuantity,
                                std::mt19937_64& rng) {
    if (warehouses < 2) throw std::invalid_argument("need at least 2 warehouses");
    std::uniform_int_distribution<std::uint32_t> product(0, products - 1);
    std::uniform_int_distribution<std::uint32_t> warehouse(0, warehouses - 1);
    std::uniform_int_distribution<std::uint32_t> other(0, warehouses - 2);
    std::uniform_int_distribution<std::int64_t> quantity(1, maxQuantity);

    std::vector<Move> moves;
    moves.reserve(count);
    for (std::uint64_t i = 0; i < count; ++i) {
        Move m{};
        m.product = product(rng);
        m.from = warehouse(rng);
        const std::uint32_t to = other(rng);
        m.to = (to >= m.from) ? to + 1 : to;
        m.quantity = quantity(rng);
        moves.push_back(m);
    }
    return moves;
}

std::vector<std::vector<Move>> distributeMoves(const std::vector<Move>& moves, unsigned parts,
                                               std::mt19937_64& rng) {
    std::vector<std::size_t> order(moves.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::shuffle(order, rng);

    std::vector<std::vector<Move>> lists(parts);
    for (auto& l : lists) l.reserve(moves.size() / parts + 1);
    for (std::size_t i = 0; i < order.size(); ++i) lists[i % parts].push_back(moves[order[i]]);
    return lists;
}
