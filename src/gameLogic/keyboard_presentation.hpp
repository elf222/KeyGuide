// gameLogic/keyboard_presentation.hpp
#pragma once
#include <string>
#include "gameLogic/tracked_state.hpp"

struct KeyboardPresentation
{
    TrackedState tracked{};
    Platform::InputBatchInfo input{};
    std::string description;
    bool initialised = false, hoverLocked = false;

    void capture(const TrackedState& state, const Platform::InputBatchInfo& status)
    {
        tracked = state;
        input = status;
        description = status.description ? status.description : "";
        initialised = true;
    }

    Platform::InputBatchInfo inputInfo() const
    {
        auto status = input;
        status.description = description.c_str();
        return status;
    }
};
