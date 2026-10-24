// gameLogic/vim_state.hpp
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "platform/input_event.hpp"
#include "gameLogic/tracking_types.hpp"

enum class VimMode
{
    Normal,
    Insert,
    Visual,
    VisualLine,
    VisualBlock,
    Select,
    SelectLine,
    SelectBlock,
    Replace,
    VirtualReplace,
    Ex,
    Terminal
};

inline bool IsSelect(VimMode mode)
{
    return mode == VimMode::Select || mode == VimMode::SelectLine || mode == VimMode::SelectBlock;
}
enum class WaitingKind
{
    None,
    G,
    Z,
    Window,
    TextObject,
    BracketOpen,
    BracketClose,
    Find,
    ReplaceCharacter,
    Register,
    MarkSet,
    MarkJump,
    MacroRecord,
    MacroPlay,
    Literal,
    TerminalPrefix,
    Lsp,
    SurroundTarget,
    SurroundReplacement,
    DigraphFirst,
    DigraphSecond,
    CtrlX,
    UpperZ
};
enum class VimContext
{
    Editor,
    Search,
    Palette,
    CommandLine,
    CommandWindow
};

struct VimState
{
    Platform::AppProfile appProfile = Platform::AppProfile::Zed;
    static constexpr std::size_t PENDING_CAPACITY = 24;
    std::array<char, PENDING_CAPACITY + 1> pendingSequence{};
    std::uint8_t pendingLength = 0;
    bool pendingTruncated = false;
    VimMode mode = VimMode::Normal;
    VimMode returnMode = VimMode::Normal;
    WaitingKind waiting = WaitingKind::None;
    WaitingKind registerReturn = WaitingKind::None;
    VimContext context = VimContext::Editor;
    VimContext returnContext = VimContext::Editor;
    Confidence confidence = Confidence::Assumed;
    char32_t operatorKey = 0;
    char32_t operatorPrefix = 0;
    VimMode lastVisualMode = VimMode::Visual;
    std::uint32_t count = 0, motionCount = 0;
    bool temporaryNormal = false;
    bool exArguments = false, exOpaque = false, exBang = false, exRange = false;
    bool recording = false;
    bool escapeDown = false, escapeArmed = false;
    double lastEscape = 0;
    std::uint8_t modifiers = 0;
    bool capsLock = false;
    double escapeResetInterval = 1.0;
    const char* reason = "Normal assumed on entry";
    std::uint64_t revision = 0;
};

const char* ModeName(VimMode mode);
const char* WaitingName(const VimState& state);
