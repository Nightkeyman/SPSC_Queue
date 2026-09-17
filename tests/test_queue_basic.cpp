#include "spsc_queue.hpp"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>

namespace {

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
    EXPECT_EQ(out, 1);
    ASSERT_TRUE(queue_.try_pop(out));
    EXPECT_EQ(out, 2);
    ASSERT_TRUE(queue_.try_pop(out));
    EXPECT_EQ(out, 3);
}

TEST_F(SpscQueueBasicTest, WhenFull_ExpectPushFailureUntilPopFreesSlot)
{
    for (std::size_t i = 0; i < kCapacity; ++i) {
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
    // Hold a couple of items back so the queue straddles the wrap point rather
    // than emptying every iteration -- that way head and tail cross the end of
    // the buffer while it still has content.
    constexpr int kResident = 2;
    for (int i = 0; i < kResident; ++i) {
        ASSERT_TRUE(queue_.try_push(i));
    }

    for (int i = kResident; i < 10 * static_cast<int>(kCapacity); ++i) {
        ASSERT_TRUE(queue_.try_push(i));

        int out = -1;
        ASSERT_TRUE(queue_.try_pop(out));
        EXPECT_EQ(out, i - kResident);
    }
}

}  // namespace
