/**
 * @file message_queue.hpp
 * @brief Thread-safe message queue with blocking and non-blocking pop.
 */

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace cortex {

/**
 * @brief Thread-safe FIFO message queue.
 * @tparam T The element type stored in the queue.
 *
 * Supports non-blocking try_pop(), blocking wait_pop(), and
 * push() with condition-variable notification.
 */
template <typename T>
class MessageQueue {
public:
    /**
     * @brief Push an item onto the queue and notify one waiting consumer.
     * @param item The item to enqueue (moved).
     */
    void push(T item) {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /**
     * @brief Try to pop an item without blocking.
     * @return The front item, or std::nullopt if the queue is empty.
     */
    std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Block until an item is available, then pop and return it.
     * @return The front item from the queue.
     */
    T wait_pop() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Check whether the queue is empty.
     * @return True if the queue contains no items.
     */
    bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;           ///< Underlying FIFO storage.
    mutable std::mutex mutex_;      ///< Mutex guarding all queue operations.
    std::condition_variable cv_;    ///< Condition variable for wait_pop() blocking.
};

} // namespace cortex
