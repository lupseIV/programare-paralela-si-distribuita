#pragma once

#include <cstdint>
#include <vector>

#include "Inventory.h"
#include "LockStrategy.h"

class Checker {
public:
    Checker(const Inventory& inventory, LockStrategy* locks, std::vector<std::int64_t> expected)
        : inv_(inventory), locks_(locks), expected_(std::move(expected)) {}

    bool checkAll() {
        bool ok = true;
        for (std::uint32_t p = 0; p < inv_.products(); ++p) {
            bool negative = false;
            std::int64_t total;
            if (locks_) {
                ProductLock guard(*locks_, p);
                total = inv_.productTotal(p, negative);
            } else {
                total = inv_.productTotal(p, negative);
            }
            if (negative || total != expected_[p]) ok = false;
        }
        ++checksDone_;
        if (!ok) ++checksFailed_;
        return ok;
    }

    [[nodiscard]] unsigned checksDone() const { return checksDone_; }
    [[nodiscard]] unsigned checksFailed() const { return checksFailed_; }

private:
    const Inventory& inv_;
    LockStrategy* locks_;
    std::vector<std::int64_t> expected_;
    unsigned checksDone_ = 0;
    unsigned checksFailed_ = 0;
};
