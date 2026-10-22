#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    void push(T value) {
        {
            std::unique_lock lock(mutex_);
            notFull_.wait(lock, [this] { return items_.size() < capacity_; });
            items_.push(std::move(value));
        }
        notEmpty_.notify_one();
    }

    std::optional<T> pop() {
        std::optional<T> value;
        {
            std::unique_lock lock(mutex_);
            notEmpty_.wait(lock, [this] { return !items_.empty() || done_; });
            if (items_.empty()) return std::nullopt;  // woke because done_ and nothing is left
            value = std::move(items_.front());
            items_.pop();
        }
        notFull_.notify_one();
        return value;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            done_ = true;
        }
        notEmpty_.notify_one();
    }

private:
    const std::size_t capacity_;
    std::queue<T> items_;
    bool done_ = false;

    std::mutex mutex_;
    std::condition_variable notFull_;   // producer waits here
    std::condition_variable notEmpty_;  // consumer waits here
};
