// tests/platform_stub.hpp
#pragma once
#include <vector>
#include "platform/platform.hpp"

namespace TestPlatform
{
    extern Platform::InputBatchInfo status;
    extern std::vector<Platform::InputEvent> events;
    extern Platform::PointerPosition pointer;
} // namespace TestPlatform
