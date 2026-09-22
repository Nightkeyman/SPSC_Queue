#include "spsc_queue.hpp"

#include <cstddef>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

namespace
{

constexpr int kItemCount = 20;

void producer_loop(SpscQueue<int>& queue, std::vector<int>& produced)
{
    for (int i = 0; i < kItemCount; ++i)
    {
        while (!queue.try_push(i))
        {
            std::this_thread::yield();
        }
        produced.push_back(i);
    }
}

void consumer_loop(SpscQueue<int>& queue, std::vector<int>& consumed)
{
    int value = 0;
    while (static_cast<int>(consumed.size()) < kItemCount)
    {
        if (queue.try_pop(value))
        {
            consumed.push_back(value);
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

} // namespace

int main()
{
    constexpr std::size_t kCapacity = 8;

    SpscQueue<int> queue{kCapacity};

    std::vector<int> produced;
    std::vector<int> consumed;
    produced.reserve(kItemCount);
    consumed.reserve(kItemCount);

    std::thread producer(producer_loop, std::ref(queue), std::ref(produced));
    std::thread consumer(consumer_loop, std::ref(queue), std::ref(consumed));

    producer.join();
    consumer.join();

    for (int i = 0; i < kItemCount; ++i)
    {
        std::cout << "produced: " << produced.at(i)
                  << "  consumed: " << consumed.at(i) << '\n';
    }

    return 0;
}
