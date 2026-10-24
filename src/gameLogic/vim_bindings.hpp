// gameLogic/vim_bindings.hpp
#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include "gameLogic/vim_state.hpp"

enum class BindingScope
{
    Motion,
    Normal,
    NormalVisual,
    Visual,
    VisualBlock,
    Select,
    Insert,
    Replace,
    Operator,
    G,
    NormalG,
    VisualG,
    Lsp,
    Z,
    Window,
    TextObject,
    BracketOpen,
    BracketClose,
    Argument,
    LiteralArgument,
    Editor,
    Ex,
    ExControl,
    Cursor,
    Terminal,
    TerminalPrefix,
    VisualTextObject,
    CtrlX,
    UpperZ,
    NormalBracketOpen,
    NormalBracketClose,
    VisualBracketOpen,
    VisualBracketClose
};
enum class BindingAction
{
    Motion,
    Insert,
    Replace,
    Visual,
    VisualLine,
    VisualBlock,
    Select,
    SelectLine,
    SelectBlock,
    SelectToggle,
    Operator,
    Find,
    ReplaceCharacter,
    PrefixG,
    PrefixLsp,
    VirtualReplace,
    ExMode,
    ForcedMotion,
    PrefixZ,
    PrefixWindow,
    PrefixBracketOpen,
    PrefixBracketClose,
    TextObject,
    Register,
    MarkSet,
    MarkJump,
    MacroRecord,
    MacroPlay,
    Unknown,
    Search,
    Palette,
    Escape,
    TemporaryNormal,
    Literal,
    Count,
    Complete,
    Navigation,
    SelectMultiple,
    SelectMatch,
    KeepVisual,
    ExInput,
    TerminalPrefix,
    LeaveTerminal,
    RestoreVisual,
    Digraph,
    PrefixCtrlX,
    SelectAll,
    PrefixUpperZ,
    SelectNode
};

struct VimBinding
{
    Platform::KeyValue value{};
    std::uint8_t modifiers = 0;
    BindingScope scope = BindingScope::Normal;
    BindingAction action = BindingAction::Complete;
    std::string_view shortHint, fullHint, chord;
    char32_t mnemonic = 0;
    std::optional<Platform::AppProfile> profile = {};
    std::string_view commandPrefix = {};
    std::string_view pendingOperator = {};
};

std::span<const VimBinding> VimBindings();
bool OperatorIs(const VimState& state, std::string_view name);
bool BindingAvailable(const VimBinding& binding, const VimState& state);
HintId FindBinding(const VimState& state, Platform::KeyValue value, std::uint8_t modifiers);
