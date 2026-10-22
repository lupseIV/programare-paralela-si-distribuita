#pragma once

#include <cstdint>
#include <vector>

#include "Move.h"

class Inventory {
public:
    Inventory(std::uint32_t warehouses, std::uint32_t products, std::int64_t initialQuantity)
        : W_(warehouses), P_(products),
          cells_(static_cast<std::size_t>(warehouses) * products, initialQuantity) {}

    [[nodiscard]] std::uint32_t warehouses() const { return W_; }
    [[nodiscard]] std::uint32_t products() const { return P_; }

    [[nodiscard]] std::size_t index(std::uint32_t warehouse, std::uint32_t product) const {
        return static_cast<std::size_t>(product) * W_ + warehouse;
    }

    std::int64_t& at(std::uint32_t warehouse, std::uint32_t product) {
        return cells_[index(warehouse, product)];
    }
    [[nodiscard]] std::int64_t at(std::uint32_t warehouse, std::uint32_t product) const {
        return cells_[index(warehouse, product)];
    }

    bool tryApply(const Move& m) {
        std::int64_t& src = at(m.from, m.product);
        if (src < m.quantity) return false;
        src -= m.quantity;
        at(m.to, m.product) += m.quantity;
        return true;
    }

    std::int64_t productTotal(std::uint32_t product, bool& anyNegative) const {
        const std::int64_t* row = &cells_[static_cast<std::size_t>(product) * W_];
        std::int64_t sum = 0;
        for (std::uint32_t w = 0; w < W_; ++w) {
            if (row[w] < 0) anyNegative = true;
            sum += row[w];
        }
        return sum;
    }

    [[nodiscard]] std::vector<std::int64_t> allTotals() const {
        std::vector<std::int64_t> totals(P_);
        bool neg = false;
        for (std::uint32_t p = 0; p < P_; ++p) totals[p] = productTotal(p, neg);
        return totals;
    }

private:
    std::uint32_t W_;
    std::uint32_t P_;
    std::vector<std::int64_t> cells_;
};
