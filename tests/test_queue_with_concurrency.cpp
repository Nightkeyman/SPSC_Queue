#include "spsc_queue.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

namespace
{

// small capacity for stress testing, to wrap around easily
constexpr std::size_t kCapacity = 2;
constexpr int kItemCount = 200000;

class SpscQueueConcurrentTest : public ::testing::Test
{
protected:
    using Clock = std::chrono::steady_clock;

    SpscQueue<int> queue_{kCapacity};

    // Pushes `count` values starting at `tag * count`, so different `tag`s
    // push disjoint ranges and any collision on the reading side is unambiguous.
    void push_range(int tag, int count, Clock::time_point deadline)
    {
        const int base = tag * count;
        for (int i = 0; i < count && Clock::now() < deadline; ++i)
        {
            while (!queue_.try_push(base + i) && Clock::now() < deadline)
            {
                std::this_thread::yield();
            }
        }
    }

    // Pops into `out` until it holds `target` items or `deadline` passes,
    // whichever comes first.
    void pop_until_count_or_deadline(std::vector<int>& out, std::size_t target,
                                     Clock::time_point deadline)
    {
        int value = 0;
        while (out.size() < target && Clock::now() < deadline)
        {
            if (queue_.try_pop(value))
            {
                out.push_back(value);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    // Pops into `out` for the whole window regardless of count -- for cases
    // where stopping early could starve a producer that's still running.
    void pop_until_deadline(std::vector<int>& out, Clock::time_point deadline)
    {
        int value = 0;
        while (Clock::now() < deadline)
        {
            if (queue_.try_pop(value))
            {
                out.push_back(value);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
};

/* Test Similar to demonstration file (main). Checks the values with assert EQ. */

TEST_F(SpscQueueConcurrentTest, WhenProducedAndConsumedConcurrently_ExpectAllItemsInOrder)
{
    const auto deadline = Clock::now() + std::chrono::seconds(30);

    std::thread producer([this, deadline] { push_range(0, kItemCount, deadline); });

    std::vector<int> consumed;
    consumed.reserve(kItemCount);
    std::thread consumer([this, &consumed, deadline] {
        pop_until_count_or_deadline(consumed, static_cast<std::size_t>(kItemCount), deadline);
    });

    producer.join();
    consumer.join();

    // Checking the exact sequence covers loss, duplication and reordering at
    // once -- anything the queue got wrong shows up as a mismatch here.
    ASSERT_EQ(consumed.size(), static_cast<std::size_t>(kItemCount));
    for (int i = 0; i < kItemCount; ++i)
    {
        ASSERT_EQ(consumed[static_cast<std::size_t>(i)], i) << "order violated at index " << i;
    }
}

/* Test violation of SP-SC contract (two writers) - data race */

TEST_F(SpscQueueConcurrentTest, WhenTwoProducersRaceConcurrently_ExpectDataCorruption)
{
    constexpr int kItemsPerProducer = 20000;
    constexpr int kTotalItems = kItemsPerProducer * 2;
    const auto deadline = Clock::now() + std::chrono::seconds(5);

    std::thread producer_a(
        [this, deadline, kItemsPerProducer] { push_range(0, kItemsPerProducer, deadline); });
    std::thread producer_b(
        [this, deadline, kItemsPerProducer] { push_range(1, kItemsPerProducer, deadline); });

    std::vector<int> consumed;
    consumed.reserve(kTotalItems);
    pop_until_count_or_deadline(consumed, static_cast<std::size_t>(kTotalItems), deadline);

    producer_a.join();
    producer_b.join();

    std::vector<int> seen_count(kTotalItems, 0);
    for (int v : consumed)
    {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, kTotalItems);
        ++seen_count[static_cast<std::size_t>(v)];
    }

    // A correct queue delivers every value exactly once; expect that to break
    // under the race (duplicate and/or missing values).
    const bool any_value_wrong =
        std::any_of(seen_count.begin(), seen_count.end(), [](int c) { return c != 1; });
    EXPECT_TRUE(consumed.size() != static_cast<std::size_t>(kTotalItems) || any_value_wrong)
        << "expected the two-producer race to corrupt delivery, but every value arrived exactly "
           "once";
}

/* Test violation of SP-SC contract (two readers) - data race */

TEST_F(SpscQueueConcurrentTest, WhenTwoConsumersRaceConcurrently_ExpectDataCorruption)
{
    constexpr int kItemCount = 20000;
    const auto deadline = Clock::now() + std::chrono::seconds(2);

    std::thread producer([this, deadline, kItemCount] { push_range(0, kItemCount, deadline); });

    // Runs for the whole window instead of a count target: a duplicated read
    // could hit that target early and leave the producer stuck on a full queue.
    std::vector<int> consumed_a;
    std::vector<int> consumed_b;
    consumed_a.reserve(kItemCount);
    consumed_b.reserve(kItemCount);
    std::thread consumer_a(
        [this, &consumed_a, deadline] { pop_until_deadline(consumed_a, deadline); });
    std::thread consumer_b(
        [this, &consumed_b, deadline] { pop_until_deadline(consumed_b, deadline); });

    producer.join();
    consumer_a.join();
    consumer_b.join();

    std::vector<int> seen_count(kItemCount, 0);
    for (int v : consumed_a)
    {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, kItemCount);
        ++seen_count[static_cast<std::size_t>(v)];
    }
    for (int v : consumed_b)
    {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, kItemCount);
        ++seen_count[static_cast<std::size_t>(v)];
    }

    // The two consumers should not have been able to agree on who owns which
    // slot -- expect at least one value delivered to both.
    const bool any_duplicated =
        std::any_of(seen_count.begin(), seen_count.end(), [](int c) { return c > 1; });
    EXPECT_TRUE(any_duplicated)
        << "expected the two-consumer race to duplicate a delivered value, but none were";
}

} // namespace
