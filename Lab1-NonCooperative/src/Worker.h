#pragma once

#include <atomic>
#include <cstdint>
#include <deque>

#include "Config.h"
#include "Inventory.h"
#include "LockStrategy.h"
#include "Move.h"

struct alignas(64) WorkerStats {
    std::atomic<std::uint64_t> succeeded{0};
    std::uint64_t unfulfilled = 0;
    std::uint64_t retries = 0;
};

template <bool Locked>
inline bool attemptMove(Inventory& inv, LockStrategy* locks, const Move& m) {
    if constexpr (Locked) {
        MoveLock guard(*locks, m);
        return inv.tryApply(m);
    } else {
        return inv.tryApply(m);
    }
}

template <bool Locked, typename OnSuccess>
void runWorker(Inventory& inv, LockStrategy* locks, std::deque<PendingMove>& queue,
               WorkerStats& stats, OnSuccess&& onSuccess) {
    std::uint64_t succeeded = 0;
    while (!queue.empty()) {
        PendingMove pm = queue.front();
        queue.pop_front();
        if (attemptMove<Locked>(inv, locks, pm.move)) {
            ++succeeded;
            stats.succeeded.store(succeeded, std::memory_order_relaxed);
            onSuccess(succeeded);
        } else if (pm.retries < kMaxRetries) {
            ++pm.retries;
            ++stats.retries;
            queue.push_back(pm);
        } else {
            ++stats.unfulfilled;
        }
    }
}
