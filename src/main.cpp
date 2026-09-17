#include "spsc_queue.hpp"

#include <cstddef>
#include <functional>
#include <iostream>
#include <thread>

namespace
{

constexpr int kItemCount = 20;

void producer_loop(SpscQueue<int>& queue)
{
    for (int i = 0; i < kItemCount; ++i)
    {
        while (!queue.try_push(i))
        {
            std::this_thread::yield();
        }
    }
}

void consumer_loop(SpscQueue<int>& queue)
{
    int received = 0;
    int value = 0;
    while (received < kItemCount)
    {
        if (queue.try_pop(value))
        {
            std::cout << "consumed " << value << '\n';
            ++received;
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

    std::thread producer(producer_loop, std::ref(queue));
    std::thread consumer(consumer_loop, std::ref(queue));

    producer.join();
    consumer.join();

    return 0;
}
