// tests/input_queue_tests.cpp
#include "test_helpers.hpp"
#include <thread>
#include "platform/event_queue.hpp"

int main()
{
    Platform::EventQueue<int, 3> queue;
    CHECK(queue.empty());
    int value = 0;
    CHECK(!queue.pop(value));
    CHECK(queue.push(1));
    CHECK(!queue.empty());
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK(!queue.push(4));
    CHECK(queue.lossGeneration() == 1);
    CHECK(queue.pop(value) && value == 1);
    CHECK(queue.push(5));
    CHECK(queue.pop(value) && value == 2);
    CHECK(queue.pop(value) && value == 3);
    CHECK(queue.pop(value) && value == 5);
    CHECK(!queue.pop(value));
    CHECK(queue.empty());
    Platform::EventQueue<unsigned, 64> concurrent;
    constexpr unsigned COUNT = 100000;
    std::thread producer(
        [&]
        {
            for (unsigned i = 0; i < COUNT; ++i)
            {
                while (!concurrent.push(i))
                {
                    std::this_thread::yield();
                }
            }
        });
    for (unsigned i = 0; i < COUNT; ++i)
    {
        unsigned record = COUNT;
        while (!concurrent.pop(record))
        {
            std::this_thread::yield();
        }
        CHECK(record == i);
    }
    producer.join();
    unsigned final_record = 0;
    CHECK(!concurrent.pop(final_record));
    std::cout << "Queue capacity, loss, wrap and SPSC order checks passed\n";
}
