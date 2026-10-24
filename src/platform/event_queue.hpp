// platform/event_queue.hpp
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Platform
{
    // One producer and one consumer. A full queue drops the observation, never the OS event.
    template <typename T, std::size_t Capacity> class EventQueue
    {
        static_assert(Capacity > 0);
        static_assert(std::atomic<std::size_t>::is_always_lock_free);
        std::array<T, Capacity> records{};
        alignas(64) std::atomic<std::size_t> writeIndex{0};
        alignas(64) std::atomic<std::size_t> readIndex{0};
        std::atomic<std::uint64_t> losses{0};

      public:
        bool empty() const
        {
            return readIndex.load(std::memory_order_acquire) ==
                   writeIndex.load(std::memory_order_acquire);
        }

        bool push(const T& record)
        {
            const auto write = writeIndex.load(std::memory_order_relaxed);
            if (write - readIndex.load(std::memory_order_acquire) >= Capacity)
            {
                losses.fetch_add(1, std::memory_order_release);
                return false;
            }
            records[write % Capacity] = record;
            writeIndex.store(write + 1, std::memory_order_release);
            return true;
        }

        bool pop(T& record)
        {
            const auto read = readIndex.load(std::memory_order_relaxed);
            if (read == writeIndex.load(std::memory_order_acquire))
            {
                return false;
            }
            record = records[read % Capacity];
            records[read % Capacity] = T{};
            readIndex.store(read + 1, std::memory_order_release);
            return true;
        }

        std::uint64_t lossGeneration() const
        {
            return losses.load(std::memory_order_acquire);
        }
    };
} // namespace Platform
