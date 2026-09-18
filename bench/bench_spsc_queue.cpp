#include "spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

namespace
{

// Capacity has to leave some slack: at capacity 2 the queue is permanently
// full or empty, so the cached index would be refreshed on every operation.
constexpr std::size_t kCapacity = 1024;
constexpr int kItemCount = 10000000;
constexpr int kRepeats = 3;

// Frozen copy of the implementation before the two cache optimizations: the
// indices share one line, and every operation loads the other thread's atomic.
template <typename T>
class NaiveSpscQueue
{
public:
    explicit NaiveSpscQueue(std::size_t capacity) : buffer_(capacity + 1)
    {
    }

    bool try_push(T queue_item)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next_head = next(head);

        if (next_head == tail_.load(std::memory_order_acquire))
        {
            return false;
        }

        buffer_[head] = std::move(queue_item);

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

private:
    std::size_t next(std::size_t index) const
    {
        return index + 1 == buffer_.size() ? 0 : index + 1;
    }

    std::vector<T> buffer_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

// Keeps the consumed values observable so the loop can't be optimized away.
volatile std::uint64_t g_sink = 0;

// Spins instead of yielding: this measures the queue's own handoff cost rather
// than the OS scheduler's wakeup latency.
template <typename Queue>
double run_once(Queue& queue)
{
    const auto start = std::chrono::steady_clock::now();

    std::thread producer([&queue] {
        for (int i = 0; i < kItemCount; ++i)
        {
            while (!queue.try_push(i))
            {
            }
        }
    });

    std::uint64_t checksum = 0;
    int value = 0;
    for (int received = 0; received < kItemCount;)
    {
        if (queue.try_pop(value))
        {
            checksum += static_cast<std::uint64_t>(value);
            ++received;
        }
    }

    producer.join();
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

    constexpr std::uint64_t kExpected =
        static_cast<std::uint64_t>(kItemCount) * (static_cast<std::uint64_t>(kItemCount) - 1) / 2;
    if (checksum != kExpected)
    {
        std::cerr << "checksum mismatch -- benchmark result is meaningless\n";
    }
    g_sink = checksum;

    return elapsed.count();
}

template <typename Queue>
double best_of_repeats(const char* label)
{
    double best_seconds = std::numeric_limits<double>::max();
    for (int i = 0; i < kRepeats; ++i)
    {
        Queue queue{kCapacity};
        best_seconds = std::min(best_seconds, run_once(queue));
    }

    const double ops_per_second = kItemCount / best_seconds;
    std::cout << std::left << std::setw(26) << label << std::right << std::fixed
              << std::setprecision(3) << std::setw(8) << best_seconds << " s"
              << std::setprecision(1) << std::setw(9) << ops_per_second / 1e6 << " M ops/s"
              << std::setw(9) << 1e9 / ops_per_second << " ns/op\n";

    return best_seconds;
}

} // namespace

int main()
{
    std::cout << kItemCount << " items, capacity " << kCapacity << ", best of " << kRepeats
              << "\n\n";

    const double naive = best_of_repeats<NaiveSpscQueue<int>>("shared line, no cache");
    const double tuned = best_of_repeats<SpscQueue<int>>("padded + cached index");

    std::cout << "\nspeedup: " << std::fixed << std::setprecision(2) << naive / tuned << "x\n";

    return 0;
}
