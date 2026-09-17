#include "spsc_queue.hpp"
#include <gtest/gtest.h>

#include <cstddef>
#include <thread>
#include <vector>

namespace {

// Deliberately tiny: the queue then swings between full and empty constantly,
// so the two threads hand off at the ring boundaries instead of the producer
// comfortably running ahead.
constexpr std::size_t kCapacity = 2;
constexpr int kItemCount = 200000;

class SpscQueueConcurrentTest : public ::testing::Test {
protected:
    SpscQueue<int> queue_{kCapacity};
};

TEST_F(SpscQueueConcurrentTest, WhenProducedAndConsumedConcurrently_ExpectAllItemsInOrder) {
    std::vector<int> consumed;
    consumed.reserve(kItemCount);

    std::thread producer([this] {
        for (int i = 0; i < kItemCount; ++i) {
            while (!queue_.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([this, &consumed] {
        int value = 0;
        while (static_cast<int>(consumed.size()) < kItemCount) {
            if (queue_.try_pop(value)) {
                consumed.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Checking the exact sequence covers loss, duplication and reordering at
    // once -- anything the queue got wrong shows up as a mismatch here.
    ASSERT_EQ(consumed.size(), static_cast<std::size_t>(kItemCount));
    for (int i = 0; i < kItemCount; ++i) {
        ASSERT_EQ(consumed[static_cast<std::size_t>(i)], i) << "order violated at index " << i;
    }
}

}  // namespace
