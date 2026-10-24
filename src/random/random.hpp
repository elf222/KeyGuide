// random/random.hpp

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>

#include "settings/defaults.hpp"
#include "errorHandling/asserts.hpp"

namespace Rnd
{
    enum class Stream
    {
        Gameplay,
        WorldGeneration,
        VisualEffects,

        Count
    };

    constexpr const char* ToString(Stream s)
    {
        switch (s)
        {
            case Stream::Gameplay:
                return "Gameplay";
            case Stream::WorldGeneration:
                return "WorldGeneration";
            case Stream::VisualEffects:
                return "VisualEffects";

            case Stream::Count:
                return "Count :)";
        }

        return ":(";
    }

    namespace Detail
    {
        constexpr std::size_t StreamCount = static_cast<std::size_t>(Stream::Count);

        struct RandomStream
        {
            std::mt19937 engine;
            std::uint64_t count = 0;
        };

        inline std::array<RandomStream, StreamCount> streams;

        constexpr std::size_t ToIndex(Stream stream)
        {
            return static_cast<std::size_t>(stream);
        }

        inline RandomStream& GetStream(Stream stream)
        {
            const std::size_t INDEX = ToIndex(stream);

            ASSERT(INDEX < streams.size(), "Request of non-existant stream\n",
                   " Requested: ", INDEX, ", Streams count: ", streams.size());

            return streams[INDEX];
        }

        inline std::uint32_t MakeStreamSeed(std::uint32_t base_seed, std::uint32_t stream_index)
        {
            // Mix the stream index into the base seed so that streams do not
            // begin with closely related seeds such as seed, seed + 1, seed + 2.
            std::seed_seq sequence{base_seed, stream_index, 0x9E3779B9u,
                                   base_seed ^ (stream_index * 0x85EBCA6Bu)};

            std::array<std::uint32_t, 1> result{};
            sequence.generate(result.begin(), result.end());

            return result[0];
        }
    } // namespace Detail

    inline std::uint32_t currentSeed = Default::DEBUG_RANDOMISATION_SEED;

    inline std::uint32_t MakeGameSeed()
    {
#if defined(_DEBUG)
        return Default::DEBUG_RANDOMISATION_SEED;
#else
        static std::uint32_t game_counter = 0;

        std::random_device random_device;
        const std::uint64_t TIME = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());

        std::seed_seq sequence{random_device(), random_device(), static_cast<std::uint32_t>(TIME),
                               static_cast<std::uint32_t>(TIME >> 32u), ++game_counter};

        std::array<std::uint32_t, 1> result{};
        sequence.generate(result.begin(), result.end());

        return result[0];
#endif
    }

    inline void Init(std::uint32_t base_seed)
    {
        currentSeed = base_seed;

        for (std::size_t i = 0; i < Detail::streams.size(); ++i)
        {
            Detail::RandomStream& stream = Detail::streams[i];
            stream.engine.seed(Detail::MakeStreamSeed(base_seed, static_cast<std::uint32_t>(i)));
            stream.count = 0;
        }
    }

    inline void InitForNewGame()
    {
        Init(MakeGameSeed());
    }

    inline std::uint32_t GetCurrentSeed()
    {
        return currentSeed;
    }

    // The access to generation

    inline std::uint64_t GetCount(Stream stream)
    {
        return Detail::GetStream(stream).count;
    }

    inline int Int(Stream stream, int min, int max)
    {
        ASSERT(min <= max, "Invalid random int request\n", "Stream: ", ToString(stream),
               ", rnd count: ", GetCount(stream), ", min: ", min, ", max: ", max);

        Detail::RandomStream& random_stream = Detail::GetStream(stream);
        ++random_stream.count;

        std::uniform_int_distribution<int> distribution(min, max);

        return distribution(random_stream.engine);
    }

    inline float Float(Stream stream, float min, float max)
    {
        ASSERT(min <= max, "Invalid random float request\n", "Stream: ", ToString(stream),
               ", rnd count: ", GetCount(stream), ", min: ", min, ", max: ", max);

        Detail::RandomStream& random_stream = Detail::GetStream(stream);
        ++random_stream.count;

        std::uniform_real_distribution<float> distribution(min, max);

        return distribution(random_stream.engine);
    }

    inline bool IsChance(Stream stream, float probability)
    {
        ASSERT(probability >= 0.0 && probability <= 1.0, "Invalid random int request\n",
               "Stream: ", ToString(stream), ", rnd count: ", GetCount(stream),
               ", probability: ", probability);

        Detail::RandomStream& random_stream = Detail::GetStream(stream);
        ++random_stream.count;

        std::bernoulli_distribution distribution(probability);

        return distribution(random_stream.engine);
    }

    inline void ResetCount(Stream stream)
    {
        Detail::GetStream(stream).count = 0;
    }

    inline void ResetAllCounts()
    {
        for (Detail::RandomStream& stream : Detail::streams)
        {
            stream.count = 0;
        }
    }
} // namespace Rnd
