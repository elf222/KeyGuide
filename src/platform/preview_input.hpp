// platform/preview_input.hpp
#pragma once
#include <span>
#include "platform/input_event.hpp"

namespace Platform
{
    InputBatchInfo ReadPreviewEvents(std::span<InputEvent> destination, bool function_held = false);
    PointerPosition ReadPreviewPointer();
    // Pins the preview pointer (window coordinates) and focus for authored screenshots.
    void SetPreviewPointerOverride(float x, float y);
} // namespace Platform
