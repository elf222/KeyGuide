// gameSkeleton/game_state.hpp
#pragma once
// App lifecycle stays separate from the inferred editor state in Tracked_State.
enum class GameState
{
    Loading,
    Observing,
    Preview,
    Paused,
    Ending,
    Count
};
