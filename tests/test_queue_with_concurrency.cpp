#include "spsc_queue.hpp"
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace
{

// Keeps the ring pinned at its boundaries, so it is always full or empty
constexpr std::size_t kCapacity = 2;
// Leaves slack, so the cached index mostly hits instead of reloading the atomic
constexpr std::size_t kLargeCapacity = 1024;
// Tightest possible ring -- every push has to wait for a pop
constexpr std::size_t kMinCapacity = 1;

constexpr int kItemCount = 200000;
// Lower, because every item here is a heap allocation
constexpr int kMoveOnlyItemCount = 50000;

class SpscQueueConcurrentTest : public ::testing::Test
{
protected:
    using Clock = std::chrono::steady_clock;

    // Unwraps whatever the queue carries back into the int it was built from
    static int value_of(int item)
    {
        return item;
    }
    static int value_of(const std::unique_ptr<int>& item)
    {
        return item ? *item : -1;
    }

    // Pushes make_item(0)..make_item(count-1), spinning while the queue is full
    template <typename T, typename MakeItem>
    static void produce_all(SpscQueue<T>& queue, int item_count, MakeItem make_item,
                            Clock::time_point deadline)
    {
        for (int i = 0; i < item_count && Clock::now() < deadline; ++i)
        {
            T item = make_item(i);
            // Re-moving the same item on retry is only safe because a failed push leaves it be
            while (!queue.try_push(std::move(item)) && Clock::now() < deadline)
            {
                std::this_thread::yield();
            }
        }
    }

    // Pops into `consumed` until it holds `item_count` values, spinning while empty
    template <typename T>
    static void consume_all(SpscQueue<T>& queue, std::vector<int>& consumed, int item_count,
                            Clock::time_point deadline)
    {
        T item{};
        while (consumed.size() < static_cast<std::size_t>(item_count) && Clock::now() < deadline)
        {
            if (queue.try_pop(item))
            {
                consumed.push_back(value_of(item));
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    // Runs one producer against one consumer and checks every value comes back
    // exactly once, in order -- that covers loss, duplication and reordering
    template <typename MakeItem>
    void run_ordered_roundtrip(std::size_t capacity, int item_count, MakeItem make_item)
    {
        using T = decltype(make_item(0));

        SpscQueue<T> queue{capacity};

        // Only here so a regression fails the test instead of hanging CI
        const auto deadline = Clock::now() + std::chrono::seconds(30);

        std::vector<int> consumed;
        consumed.reserve(static_cast<std::size_t>(item_count));

        std::thread producer([&] { produce_all(queue, item_count, make_item, deadline); });
        std::thread consumer([&] { consume_all(queue, consumed, item_count, deadline); });

        producer.join();
        consumer.join();

        ASSERT_EQ(consumed.size(), static_cast<std::size_t>(item_count));
        for (int i = 0; i < item_count; ++i)
        {
            ASSERT_EQ(consumed[static_cast<std::size_t>(i)], i) << "wrong value at index " << i;
        }
    }
};

TEST_F(SpscQueueConcurrentTest, WhenProducedAndConsumedConcurrently_ExpectAllItemsInOrder)
{
    run_ordered_roundtrip(kCapacity, kItemCount, [](int i) { return i; });
}

TEST_F(SpscQueueConcurrentTest, WhenCapacityIsLarge_ExpectAllItemsInOrder)
{
    run_ordered_roundtrip(kLargeCapacity, kItemCount, [](int i) { return i; });
}

TEST_F(SpscQueueConcurrentTest, WhenCapacityIsOne_ExpectAllItemsInOrder)
{
    run_ordered_roundtrip(kMinCapacity, kItemCount, [](int i) { return i; });
}

// Exercises try_push(T&&) and proves a failed push never eats the caller's item
TEST_F(SpscQueueConcurrentTest, WhenItemsAreMoveOnly_ExpectAllItemsInOrder)
{
    run_ordered_roundtrip(kCapacity, kMoveOnlyItemCount,
                          [](int i) { return std::make_unique<int>(i); });
}

} // namespace
