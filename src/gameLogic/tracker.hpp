// gameLogic/tracker.hpp
#pragma once
#include "gameLogic/tracked_state.hpp"

void TrackInput(TrackedState& state, const Platform::InputEvent& event);
void SetAppProfile(TrackedState& state, Platform::AppProfile profile);
void TrackFocusChange(TrackedState& state, bool foreground);
void InvalidateTrackedState(TrackedState& state, const char* reason);
