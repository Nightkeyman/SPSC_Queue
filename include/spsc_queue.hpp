#ifndef SPSC_QUEUE
#define SPSC_QUEUE

#include <atomic>
#include <cstddef>
#include <vector>

// Template class for Single Producer - Single Consumer Queue
// Lock-free, based on acqu/rel mechanisms
template <typename T> 
class SpscQueue
{
public:
    explicit SpscQueue(std::size_t capacity) : buffer_(capacity + 1)
    {
    }


    bool try_push(T item)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = next(head);

        if (next_head == tail_.load(std::memory_order_acquire))
        {
            return false;
        }

        buffer_[head] = std::move(item);

        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire))
        {
            return false;
        }

        out = std::move(buffer_[tail]);

        tail_.store(next(tail), std::memory_order_release);
        return true;
    }

    bool empty();

    bool full();

    std::size_t capacity() const
    {
        return buffer_.size() - 1;
    }

private:
    std::size_t next(std::size_t index) const
    {
        return index + 1 == buffer_.size() ? 0 : index + 1;
    }

    std::vector<T> buffer_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

#endif
