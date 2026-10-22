#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "Config.h"
#include "Move.h"

struct alignas(64) PaddedMutex {
    std::mutex m;
};

class LockStrategy {
public:
    virtual ~LockStrategy() = default;
    [[nodiscard]] virtual Granularity granularity() const = 0;

    virtual void lockMove(const Move& m) = 0;
    virtual void unlockMove(const Move& m) = 0;

    virtual void lockProduct(std::uint32_t p) = 0;
    virtual void unlockProduct(std::uint32_t p) = 0;
};

class PerWarehouseLocks : public LockStrategy {
public:
    explicit PerWarehouseLocks(std::uint32_t warehouses) : locks_(warehouses) {}
    [[nodiscard]] Granularity granularity() const override { return Granularity::PerWarehouse; }

    void lockMove(const Move& move) override {
        const std::uint32_t lo = std::min(move.from, move.to);
        const std::uint32_t hi = std::max(move.from, move.to);
        locks_[lo].m.lock();
        locks_[hi].m.lock();
    }
    void unlockMove(const Move& move) override {
        locks_[move.from].m.unlock();
        locks_[move.to].m.unlock();
    }
    void lockProduct(std::uint32_t) override {
        for (auto& l : locks_) l.m.lock();
    }
    void unlockProduct(std::uint32_t) override {
        for (auto& l : locks_) l.m.unlock();
    }

private:
    std::vector<PaddedMutex> locks_;
};

class PerProductLocks : public LockStrategy {
public:
    explicit PerProductLocks(std::uint32_t products) : locks_(products) {}
    [[nodiscard]] Granularity granularity() const override { return Granularity::PerProduct; }

    void lockMove(const Move& move) override { locks_[move.product].m.lock(); }
    void unlockMove(const Move& move) override { locks_[move.product].m.unlock(); }
    void lockProduct(std::uint32_t p) override { locks_[p].m.lock(); }
    void unlockProduct(std::uint32_t p) override { locks_[p].m.unlock(); }

private:
    std::vector<PaddedMutex> locks_;
};

class PerCellLocks : public LockStrategy {
public:
    PerCellLocks(std::uint32_t warehouses, std::uint32_t products)
        : W_(warehouses),
          locks_(std::make_unique<std::mutex[]>(static_cast<std::size_t>(warehouses) * products)) {}
    [[nodiscard]] Granularity granularity() const override { return Granularity::PerCell; }

    void lockMove(const Move& m) override {
        const std::size_t a = cell(m.from, m.product);
        const std::size_t b = cell(m.to, m.product);
        locks_[std::min(a, b)].lock();
        locks_[std::max(a, b)].lock();
    }
    void unlockMove(const Move& m) override {
        locks_[cell(m.from, m.product)].unlock();
        locks_[cell(m.to, m.product)].unlock();
    }
    void lockProduct(std::uint32_t p) override {
        for (std::uint32_t w = 0; w < W_; ++w) locks_[cell(w, p)].lock();
    }
    void unlockProduct(std::uint32_t p) override {
        for (std::uint32_t w = 0; w < W_; ++w) locks_[cell(w, p)].unlock();
    }

private:
    [[nodiscard]] std::size_t cell(std::uint32_t w, std::uint32_t p) const {
        return static_cast<std::size_t>(p) * W_ + w;
    }
    std::uint32_t W_;
    std::unique_ptr<std::mutex[]> locks_;
};

class MoveLock {
public:
    MoveLock(LockStrategy& s, const Move& m) : s_(s), m_(m) { s_.lockMove(m_); }
    ~MoveLock() { s_.unlockMove(m_); }
    MoveLock(const MoveLock&) = delete;
    MoveLock& operator=(const MoveLock&) = delete;
private:
    LockStrategy& s_;
    const Move& m_;
};

class ProductLock {
public:
    ProductLock(LockStrategy& s, std::uint32_t p) : s_(s), p_(p) { s_.lockProduct(p_); }
    ~ProductLock() { s_.unlockProduct(p_); }
    ProductLock(const ProductLock&) = delete;
    ProductLock& operator=(const ProductLock&) = delete;
private:
    LockStrategy& s_;
    std::uint32_t p_;
};

std::unique_ptr<LockStrategy> makeLockStrategy(Granularity g, std::uint32_t warehouses,
                                               std::uint32_t products);
