#ifndef SPSC_QUEUE
#define SPSC_QUEUE

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::atomic<std::size_t>::is_always_lock_free,
              "SpscQueue needs a lock-free std::atomic<std::size_t> on this platform");


/// @brief Bounded lock-free ring buffer for exactly one producer and one consumer thread
///
/// Nothing blocks -- try_push fails when full, try_pop when empty, ordered by acquire/release
template <typename T>
class SpscQueue
{
    static_assert(!std::is_same_v<T, bool>,
                  "SpscQueue<bool> would be a data race: std::vector<bool> packs bits, so the "
                  "producer and consumer can end up writing the same word");

public:
    /// @brief Creates a queue that holds up to `capacity` items, throws if `capacity` is 0
    explicit SpscQueue(std::size_t capacity) : buffer_(capacity + 1)
    {
        // A zero-capacity ring would report itself full forever and drop every push
        if (capacity == 0)
        {
            throw std::invalid_argument("SpscQueue needs a capacity of at least 1");
        }
    }

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    /// @brief Copies an item in, fails if the queue is full
    bool try_push(const T& queue_item)
    {
        return try_push_impl(queue_item);
    }

    /// @brief Moves an item in, fails if the queue is full
    bool try_push(T&& queue_item)
    {
        return try_push_impl(std::move(queue_item));
    }

    /// @brief Moves the oldest item into `out`, fails if the queue is empty
    bool try_pop(T& out)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        // A stale head is fine -- the producer only adds, so a mismatch means one is waiting
        if (tail == head_cache_)
        {
            head_cache_ = head_.load(std::memory_order_acquire);
            if (tail == head_cache_)
            {
                return false;
            }
        }

        out = std::move(buffer_[tail]);

        tail_.store(next(tail), std::memory_order_release);
        return true;
    }

    /// @brief Rough emptiness check for metrics -- not safe for control flow
    bool empty() const
    {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    /// @brief Rough fullness check for metrics -- not safe for control flow
    bool full() const
    {
        return next(head_.load(std::memory_order_relaxed)) == tail_.load(std::memory_order_relaxed);
    }

    /// @brief How many items the queue can hold
    std::size_t capacity() const
    {
        return buffer_.size() - 1;
    }

private:
    /// @brief Next index in the ring, wrapping back to 0 at the end of the buffer
    std::size_t next(std::size_t index) const
    {
        return index + 1 == buffer_.size() ? 0 : index + 1;
    }

    /// @brief Only consumes the item once there's room, so a failed push leaves it untouched
    template <typename U>
    bool try_push_impl(U&& queue_item)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = next(head);

        // A stale tail is fine -- the consumer only frees slots, so a mismatch means room
        if (next_head == tail_cache_)
        {
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (next_head == tail_cache_)
            {
                return false;
            }
        }

        buffer_[head] = std::forward<U>(queue_item);

        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // not std::hardware_destructive_interference_size, as GCC9 warns on every use of that one
    static constexpr std::size_t kCacheLineSize = 64;

    // Each index shares its line only with its own thread's cached copy of the other one
    std::vector<T> buffer_;
    alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
    std::size_t tail_cache_{0};
    alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};
    std::size_t head_cache_{0};
};

#endif
