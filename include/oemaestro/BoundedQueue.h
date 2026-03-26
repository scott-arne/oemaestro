#ifndef OEMAESTRO_BOUNDEDQUEUE_H
#define OEMAESTRO_BOUNDEDQUEUE_H

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace OEMaestro {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}

    bool Push(T item) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] { return closed_ || queue_.size() < capacity_; });
        if (closed_) return false;
        queue_.push_back(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> Pop() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, [&] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return item;
    }

    void Close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    bool IsClosed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

private:
    std::deque<T> queue_;
    size_t capacity_;
    bool closed_ = false;
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_BOUNDEDQUEUE_H
