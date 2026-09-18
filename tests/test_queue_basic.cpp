#include "spsc_queue.hpp"
#include <array>
#include <cstddef>
#include <gtest/gtest.h>

namespace
{

constexpr std::size_t kCapacity = 4;

class SpscQueueBasicTest : public ::testing::Test
{
protected:
    SpscQueue<int> queue_{kCapacity};
};

TEST_F(SpscQueueBasicTest, WhenDeclaredCapacity_ExpectSameCapacity)
{
    EXPECT_EQ(queue_.capacity(), kCapacity);
}

TEST_F(SpscQueueBasicTest, WhenQueueEmpty_ExpectPopFailure)
{
    int out = -1;
    EXPECT_FALSE(queue_.try_pop(out));
    EXPECT_EQ(out, -1);
}

TEST_F(SpscQueueBasicTest, WhenAddedInOrder_ExpectPreservedFifoOrder)
{
    std::array<int, 3> ordered_numbers = {1, 2, 3};

    ASSERT_TRUE(queue_.try_push(ordered_numbers.at(0)));
    ASSERT_TRUE(queue_.try_push(ordered_numbers.at(1)));
    ASSERT_TRUE(queue_.try_push(ordered_numbers.at(2)));

    int out = 0;
    ASSERT_TRUE(queue_.try_pop(out));
    EXPECT_EQ(out, ordered_numbers.at(0));
    ASSERT_TRUE(queue_.try_pop(out));
    EXPECT_EQ(out, ordered_numbers.at(1));
    ASSERT_TRUE(queue_.try_pop(out));
    EXPECT_EQ(out, ordered_numbers.at(2));
}

TEST_F(SpscQueueBasicTest, WhenFull_ExpectPushFailureUntilPopFreesSlot)
{
    for (std::size_t i = 0; i < kCapacity; ++i)
    {
        ASSERT_TRUE(queue_.try_push(static_cast<int>(i)));
    }
    ASSERT_FALSE(queue_.try_push(999));

    int out = -1;
    ASSERT_TRUE(queue_.try_pop(out));
    EXPECT_EQ(out, 0);

    EXPECT_TRUE(queue_.try_push(999));
}

TEST_F(SpscQueueBasicTest, WhenWrappingPastBufferEnd_ExpectPreservedFifoOrder)
{
    // Proves that FIFO order survives head/tail wrapping around the ring
    // buffer, not only a single fill-then-drain pass.
    constexpr int kResidentItems = 2;
    constexpr int kTotalPushes = 10 * static_cast<int>(kCapacity);

    // Seed the backlog: these values aren't popped until the main loop below.
    for (int pushed = 0; pushed < kResidentItems; ++pushed)
    {
        ASSERT_TRUE(queue_.try_push(pushed));
    }

    // Each iteration pushes a new value and pops the oldest
    // so the value popped always trails the value just
    // pushed by exactly `kResidentItems`.
    for (int pushed = kResidentItems; pushed < kTotalPushes; ++pushed)
    {
        ASSERT_TRUE(queue_.try_push(pushed));

        int popped = -1;
        ASSERT_TRUE(queue_.try_pop(popped));
        EXPECT_EQ(popped, pushed - kResidentItems);
    }
}

} // namespace
