// gameLogic/vim_tracker.cpp
#include "gameLogic/vim_tracker.hpp"
#include <algorithm>
#include <string>
#include <string_view>
#include "gameLogic/vim_bindings.hpp"
#include "gameLogic/vim_ex_commands.hpp"

namespace
{
    void AppendPending(VimState& state, std::string_view text)
    {
        for (char c : text)
        {
            if (state.pendingLength == VimState::PENDING_CAPACITY)
            {
                state.pendingTruncated = true;
                break;
            }
            state.pendingSequence[state.pendingLength++] = c;
        }
        state.pendingSequence[state.pendingLength] = '\0';
    }

    void AppendPendingKey(VimState& state, const Platform::InputEvent& event)
    {
        if (event.modifiers & (Platform::CONTROL | Platform::OPTION | Platform::COMMAND))
        {
            if (state.pendingLength)
            {
                AppendPending(state, " ");
            }
            if (event.modifiers & Platform::CONTROL)
            {
                AppendPending(state, "Ctrl-");
            }
            if (event.modifiers & Platform::OPTION)
            {
                AppendPending(state, "Opt-");
            }
            if (event.modifiers & Platform::COMMAND)
            {
                AppendPending(state, "Cmd-");
            }
        }
        if (event.value.character >= 32 && event.value.character < 127)
        {
            const char character = static_cast<char>(event.value.character);
            AppendPending(state, {&character, 1});
        }
    }

    void ClearPending(VimState& state)
    {
        state.pendingSequence.fill(0);
        state.pendingLength = 0;
        state.pendingTruncated = false;
    }

    bool IsVisual(VimMode mode)
    {
        return mode == VimMode::Visual || mode == VimMode::VisualLine ||
               mode == VimMode::VisualBlock;
    }

    bool IsPrintable(const Platform::InputEvent& event)
    {
        using namespace Platform;
        return event.value.character >= 32 && event.value.character != 127 &&
               !(event.modifiers & (CONTROL | OPTION | COMMAND));
    }

    VimMode ToggleSelect(VimMode mode)
    {
        switch (mode)
        {
            case VimMode::Visual:
                return VimMode::Select;
            case VimMode::VisualLine:
                return VimMode::SelectLine;
            case VimMode::VisualBlock:
                return VimMode::SelectBlock;
            case VimMode::Select:
                return VimMode::Visual;
            case VimMode::SelectLine:
                return VimMode::VisualLine;
            case VimMode::SelectBlock:
                return VimMode::VisualBlock;
            default:
                return mode;
        }
    }

    bool IsTextEntry(VimMode mode)
    {
        return mode == VimMode::Insert || mode == VimMode::Replace ||
               mode == VimMode::VirtualReplace;
    }

    bool IsEditable(const VimState& state)
    {
        return state.context == VimContext::Editor || state.context == VimContext::CommandWindow;
    }

    bool IsIdleNormal(const VimState& state)
    {
        return state.mode == VimMode::Normal && state.context == VimContext::Editor &&
               state.waiting == WaitingKind::None && !state.operatorKey && !state.count &&
               !state.temporaryNormal;
    }

    bool IsRawKeyWait(WaitingKind waiting)
    {
        return waiting == WaitingKind::Literal || waiting == WaitingKind::DigraphFirst ||
               waiting == WaitingKind::DigraphSecond;
    }

    void LeaveVisual(VimState& state)
    {
        if (IsVisual(state.mode))
        {
            state.lastVisualMode = state.mode;
        }
        state.mode = state.returnMode == VimMode::Insert ? VimMode::Insert : VimMode::Normal;
        state.returnMode = VimMode::Normal;
    }

    enum class GhosttyAction
    {
        VimInput,
        Navigation,
        Ignore,
        Unknown
    };

    GhosttyAction ClassifyGhosttyInput(const Platform::InputEvent& event)
    {
        using namespace Platform;
        const auto mods = event.modifiers;
        const auto key = event.key;
        const auto special = event.value.special;
        const auto character = event.value.character;
        const bool bracket =
            key == KeyId::LeftBracket || key == KeyId::RightBracket ||
            std::u32string_view(U"[]{}").find(character) != std::u32string_view::npos;
        const bool arrow = special == SpecialKey::Up || special == SpecialKey::Down ||
                           special == SpecialKey::Left || special == SpecialKey::Right;
        if (((mods == CONTROL || mods == (CONTROL | SHIFT)) && special == SpecialKey::Tab) ||
            ((mods == COMMAND || mods == (COMMAND | SHIFT)) && bracket) ||
            (mods == (COMMAND | OPTION) && arrow) ||
            (mods == COMMAND && key >= KeyId::One && key <= KeyId::Nine) ||
            ((mods == COMMAND || mods == (COMMAND | SHIFT)) &&
             (key == KeyId::T || key == KeyId::D || key == KeyId::Z)) ||
            (mods == COMMAND && key == KeyId::N) ||
            ((mods & COMMAND) && !(mods & CONTROL) && key == KeyId::W))
        {
            return GhosttyAction::Navigation;
        }
        if (!(mods & COMMAND))
        {
            return GhosttyAction::VimInput;
        }
        if ((mods == COMMAND && (key == KeyId::C || key == KeyId::A || key == KeyId::Zero ||
                                 key == KeyId::Minus || key == KeyId::Equal)) ||
            (mods == (COMMAND | SHIFT) && key == KeyId::Equal) ||
            ((mods == COMMAND || mods == (COMMAND | SHIFT)) && special == SpecialKey::Enter) ||
            (mods == (COMMAND | CONTROL) && key == KeyId::F) ||
            ((mods == COMMAND || mods == (COMMAND | SHIFT)) &&
             (special == SpecialKey::Up || special == SpecialKey::Down)) ||
            (mods == COMMAND && (special == SpecialKey::Home || special == SpecialKey::End ||
                                 special == SpecialKey::PageUp || special == SpecialKey::PageDown)))
        {
            return GhosttyAction::Ignore;
        }
        // Paste, terminal search/palette and unknown shortcuts have hidden effects.
        return GhosttyAction::Unknown;
    }

    void ClearContinuation(VimState& state)
    {
        state.waiting = WaitingKind::None;
        state.registerReturn = WaitingKind::None;
        state.operatorKey = 0;
        state.operatorPrefix = 0;
        state.count = 0;
        state.motionCount = 0;
        state.exArguments = false;
        state.exOpaque = false;
        state.exBang = false;
        state.exRange = false;
        ClearPending(state);
    }

    void Complete(VimState& state)
    {
        const bool change = OperatorIs(state, "c");
        ClearContinuation(state);
        if (change)
        {
            state.mode = VimMode::Insert;
        }
        if (state.temporaryNormal)
        {
            state.mode = VimMode::Insert;
            state.temporaryNormal = false;
        }
        state.confidence = Confidence::Inferred;
    }

    // The key before a new operator key: an operator already pending (cs, cx, ys) or the g of a
    // g-operator.
    char32_t OperatorPrefix(const VimState& state, WaitingKind waiting)
    {
        if (state.operatorKey)
        {
            return state.operatorKey;
        }
        return waiting == WaitingKind::G ? U'g' : 0;
    }

    // A motion or text object ends the operator, except ys which still needs its surround
    // character.
    void CompleteOperatorTarget(VimState& state)
    {
        if (OperatorIs(state, "ys"))
        {
            state.waiting = WaitingKind::SurroundReplacement;
            return;
        }
        Complete(state);
    }

    void ReturnFromOverlay(VimState& state)
    {
        state.context = state.returnContext;
        state.mode = state.returnMode;
        ClearContinuation(state);
        if (state.mode == VimMode::Ex)
        {
            state.context = VimContext::CommandLine;
            AppendPending(state, ":");
        }
        if (state.appProfile == Platform::AppProfile::Neovim && state.temporaryNormal)
        {
            Complete(state);
        }
    }

    void OpenCommandWindow(VimState& state)
    {
        ClearContinuation(state);
        state.temporaryNormal = false;
        state.returnContext = VimContext::Editor;
        state.context = VimContext::CommandWindow;
        state.mode = VimMode::Normal;
    }

    void LeaveCommandWindow(VimState& state)
    {
        state.returnContext = VimContext::Editor;
        ReturnFromOverlay(state);
    }

    void Escape(VimState& state)
    {
        // Escape belongs to the terminal program; Neovim stays in Terminal mode.
        if (state.mode == VimMode::Terminal)
        {
            ClearContinuation(state);
            return;
        }
        if (IsRawKeyWait(state.waiting))
        {
            state.waiting = WaitingKind::None;
            ClearPending(state);
            return;
        }
        if (!IsEditable(state))
        {
            ReturnFromOverlay(state);
            return;
        }
        ClearContinuation(state);
        if (IsVisual(state.mode))
        {
            LeaveVisual(state);
        }
        else
        {
            state.mode = state.temporaryNormal ? VimMode::Insert : VimMode::Normal;
        }
        state.temporaryNormal = false;
        if (state.confidence != Confidence::Uncertain)
        {
            state.confidence = Confidence::Inferred;
        }
    }

    void OpaqueExInput(VimState& state)
    {
        state.exOpaque = true;
        state.exBang = false;
        ClearPending(state);
        AppendPending(state, ":");
    }

    void TrackCommandLine(VimState& state, const Platform::InputEvent& event)
    {
        using namespace Platform;
        if (state.waiting == WaitingKind::Literal)
        {
            state.waiting = WaitingKind::None;
            return;
        }
        const auto character = event.value.character;
        const bool control = event.modifiers == CONTROL;
        const bool execute = (event.modifiers == 0 && event.value.special == SpecialKey::Enter) ||
                             (control && (character == U'm' || character == U'j'));
        if (execute)
        {
            const auto prefix = ExPrefix(state);
            const auto* command = FindExCommand(prefix);
            const bool empty =
                prefix.empty() && !state.exOpaque && !state.exArguments && !state.exBang;
            const bool known = command && !state.exOpaque && !state.exArguments &&
                               command->result != ExResult::Unknown &&
                               (!state.exRange || command->result == ExResult::Complete);
            if (command && command->name == "quit" && !state.exOpaque && !state.exArguments &&
                state.returnContext == VimContext::CommandWindow)
            {
                LeaveCommandWindow(state);
                state.confidence = Confidence::Inferred;
                return;
            }
            if (!empty && !known)
            {
                state.context = VimContext::Editor;
                state.mode = VimMode::Normal;
                InvalidateTrackedState(state,
                                       "Ex command result is unknown; focus Neovim and resync");
                return;
            }
            state.context = state.returnContext;
            state.mode = state.returnMode;
            if (command && command->result == ExResult::LeaveEx)
            {
                state.mode = VimMode::Normal;
            }
            if (command && command->result == ExResult::Normal)
            {
                state.temporaryNormal = false;
            }
            if (command && command->result == ExResult::Insert)
            {
                state.mode = VimMode::Insert;
                state.temporaryNormal = false;
            }
            if (command && command->result == ExResult::Replace)
            {
                state.mode = VimMode::Replace;
                state.temporaryNormal = false;
            }
            if (command && command->result == ExResult::Terminal)
            {
                state.mode = VimMode::Terminal;
                state.temporaryNormal = false;
            }
            Complete(state);
            if (state.mode == VimMode::Ex)
            {
                state.context = VimContext::CommandLine;
                AppendPending(state, ":");
            }
            return;
        }
        if (control && character == U'v')
        {
            OpaqueExInput(state);
            state.waiting = WaitingKind::Literal;
            return;
        }
        if (control && character == U'f')
        {
            OpenCommandWindow(state);
            return;
        }
        if (control && character == U'u' && !state.exOpaque)
        {
            ClearContinuation(state);
            AppendPending(state, ":");
            return;
        }
        const bool backspace =
            (event.modifiers == 0 && event.value.special == SpecialKey::Backspace) ||
            (control && character == U'h');
        if (backspace && !state.exOpaque && !state.exArguments)
        {
            if (state.exBang)
            {
                state.exBang = false;
            }
            else if (state.pendingLength <= 1)
            {
                if (state.exRange)
                {
                    OpaqueExInput(state);
                    return;
                }
                Escape(state);
                return;
            }
            state.pendingSequence[--state.pendingLength] = '\0';
            return;
        }
        if (control && character == U'w' && !state.exOpaque && !state.exArguments &&
            !state.exRange && !state.exBang)
        {
            ClearContinuation(state);
            AppendPending(state, ":");
            return;
        }
        if (event.modifiers & (CONTROL | OPTION | COMMAND) ||
            event.value.special != SpecialKey::None)
        {
            OpaqueExInput(state);
            return;
        }
        if (state.exOpaque)
        {
            return;
        }
        if (state.exArguments)
        {
            return;
        }
        if (character == U'!' && !state.exBang && FindExCommand(ExPrefix(state)))
        {
            state.exBang = true;
            AppendPending(state, "!");
            return;
        }
        if (character >= U'a' && character <= U'z' && !state.exBang)
        {
            auto prefix = std::string(ExPrefix(state));
            prefix += static_cast<char>(character);
            if (IsExPrefix(prefix))
            {
                AppendPending(state, prefix.substr(prefix.size() - 1));
            }
            else
            {
                OpaqueExInput(state);
            }
            return;
        }
        if (state.pendingLength == 1 &&
            (std::u32string_view(U"0123456789%.,$").find(character) != std::u32string_view::npos))
        {
            state.exRange = true;
            return;
        }
        // Arguments may contain filenames, inserted text, scripts or chained commands.
        if (FindExCommand(ExPrefix(state)))
        {
            state.exArguments = true;
        }
        else
        {
            OpaqueExInput(state);
        }
    }

    std::uint32_t AppendCount(std::uint32_t count, char32_t digit)
    {
        return std::min<std::uint32_t>(999999, count * 10 + static_cast<unsigned>(digit - U'0'));
    }
} // namespace

void ResetTrackedState(VimState& state, VimMode mode, const char* reason)
{
    const auto revision = state.revision + 1;
    const auto modifiers = state.modifiers;
    const auto interval = state.escapeResetInterval;
    const auto caps_lock = state.capsLock;
    const auto recording = state.recording;
    const auto profile = state.appProfile;
    state = VimState{};
    state.mode = mode;
    state.reason = reason;
    state.revision = revision;
    state.modifiers = modifiers;
    state.escapeResetInterval = interval;
    state.capsLock = caps_lock;
    state.recording = recording;
    state.appProfile = profile;
}

void InvalidateTrackedState(VimState& state, const char* reason)
{
    ClearContinuation(state);
    state.temporaryNormal = false;
    state.confidence = Confidence::Uncertain;
    state.reason = reason;
    state.escapeArmed = false;
    state.escapeDown = false;
    ++state.revision;
}

void TrackInput(VimState& state, const Platform::InputEvent& event)
{
    using namespace Platform;
    state.modifiers = event.modifiers;
    state.capsLock = event.capsLock;
    ++state.revision;
    switch (event.type)
    {
        case EventType::NormalBaseline:
            ResetTrackedState(state, VimMode::Normal,
                              event.baselineReason ? event.baselineReason
                                                   : "Manual Normal baseline");
            return;
        case EventType::InsertBaseline:
            ResetTrackedState(state, VimMode::Insert, "Manual Insert baseline");
            return;
        case EventType::VisualBaseline:
            ResetTrackedState(state, VimMode::Visual, "Manual Visual baseline");
            return;
        case EventType::ReplaceBaseline:
            ResetTrackedState(state, VimMode::Replace, "Manual Replace baseline");
            return;
        case EventType::MouseAction:
            InvalidateTrackedState(state, "Mouse action; focus editor and press Esc twice");
            return;
        case EventType::Unsupported:
            InvalidateTrackedState(state, "Unsupported keyboard layout or input");
            return;
        case EventType::KeyUp:
            if (event.key == KeyId::Escape)
            {
                state.escapeDown = false;
            }
            return;
        case EventType::Modifiers:
            return;
        default:
            break;
    }
    const bool escape = event.value.special == SpecialKey::Escape && event.modifiers == 0;
    if (escape)
    {
        const bool raw_key = IsRawKeyWait(state.waiting);
        const bool double_escape = !event.repeat && !state.escapeDown && state.escapeArmed &&
                                   event.timestamp >= state.lastEscape &&
                                   event.timestamp - state.lastEscape <= state.escapeResetInterval;
        Escape(state);
        // A tracked Terminal mode keeps Escape for the shell; an uncertain state still resyncs
        // here.
        const bool terminal_owns_escape =
            state.mode == VimMode::Terminal && state.confidence != Confidence::Uncertain;
        if (double_escape && !raw_key && state.context == VimContext::Editor &&
            !terminal_owns_escape)
        {
            ResetTrackedState(state, VimMode::Normal, "Normal assumed after two Escape presses");
        }
        if (!event.repeat && !state.escapeDown)
        {
            state.escapeArmed = !raw_key;
            state.lastEscape = event.timestamp;
        }
        state.escapeDown = true;
        return;
    }
    state.escapeArmed = false;
    const bool neovim = state.appProfile == AppProfile::Neovim;
    if (neovim)
    {
        switch (ClassifyGhosttyInput(event))
        {
            case GhosttyAction::Navigation:
                ResetTrackedState(state, VimMode::Normal,
                                  "Normal assumed after Ghostty tab / split / window navigation");
                return;
            case GhosttyAction::Ignore:
                return;
            case GhosttyAction::Unknown:
                InvalidateTrackedState(
                    state, "Ghostty UI, paste or unsupported shortcut; focus Neovim and resync");
                return;
            case GhosttyAction::VimInput:
                break;
        }
    }
    if (neovim && state.context == VimContext::CommandWindow && event.modifiers == CONTROL &&
        event.value.character == U'c')
    {
        LeaveCommandWindow(state);
        return;
    }
    // Ctrl-x Ctrl-c is an Insert command (Zed edit prediction, Neovim CTRL-X mode), not a cancel.
    const bool ctrl_x_command = state.waiting == WaitingKind::CtrlX;
    // Zed's Ctrl-w Ctrl-c closes the pane; Neovim's CTRL-W CTRL-C is a cancelled command.
    const bool zed_pane_command = !neovim && state.waiting == WaitingKind::Window;
    const bool control_cancel =
        event.modifiers == CONTROL &&
        (event.value.character == U'[' ||
         (event.value.character == U'c' && !ctrl_x_command && !zed_pane_command &&
          (state.mode != VimMode::Normal || state.operatorKey ||
           state.waiting != WaitingKind::None || state.context != VimContext::Editor)));
    if (control_cancel && state.waiting != WaitingKind::Literal)
    {
        Escape(state);
        return;
    }
    if (event.modifiers == CONTROL && event.value.character == U'c' && IsIdleNormal(state))
    {
        return;
    }
    if (state.appProfile == AppProfile::Zed && event.modifiers == COMMAND &&
        event.value.character == U'w')
    {
        ResetTrackedState(state, VimMode::Normal, "Normal assumed after closing a tab");
        return;
    }
    // Known file/pane shortcuts are the user's Normal-on-navigation convention.
    if (!neovim && ((event.modifiers == CONTROL && (event.value.special == SpecialKey::PageUp ||
                                                    event.value.special == SpecialKey::PageDown)) ||
                    (event.modifiers == (COMMAND | SHIFT) &&
                     (event.value.character == U'{' || event.value.character == U'}')) ||
                    (event.modifiers == COMMAND && event.value.character >= U'1' &&
                     event.value.character <= U'9')))
    {
        ResetTrackedState(state, VimMode::Normal, "Normal assumed after file navigation");
        return;
    }
    if (state.context == VimContext::CommandLine)
    {
        if (state.confidence != Confidence::Uncertain)
        {
            TrackCommandLine(state, event);
        }
        return;
    }
    if (neovim && state.context == VimContext::CommandWindow && state.mode == VimMode::Normal &&
        state.waiting == WaitingKind::None && !state.operatorKey && !state.count &&
        event.modifiers == 0 && event.value.special == SpecialKey::Enter)
    {
        LeaveCommandWindow(state);
        InvalidateTrackedState(state, "Command-line window line executed; resync");
        return;
    }
    if (!IsEditable(state))
    {
        if (event.value.special == SpecialKey::Enter ||
            (neovim && event.modifiers == CONTROL &&
             (event.value.character == U'm' || event.value.character == U'j')))
        {
            if (state.context == VimContext::Palette)
            {
                state.context = VimContext::Editor;
                InvalidateTrackedState(state, "Command executed; resync after returning to editor");
            }
            else
            {
                state.context = state.returnContext;
                state.mode = state.returnMode;
                Complete(state);
            }
        }
        return;
    }
    if (state.confidence == Confidence::Uncertain)
    {
        return;
    }
    if (IsSelect(state.mode))
    {
        // Select mode replaces the selection with typed text, so printable keys never act as
        // motions.
        if (IsPrintable(event))
        {
            state.mode = VimMode::Insert;
            Complete(state);
            return;
        }
        if (event.value.special == SpecialKey::Enter || event.value.special == SpecialKey::Tab ||
            event.value.special == SpecialKey::Backspace)
        {
            InvalidateTrackedState(state, "Select-mode edit key; focus Neovim and resync");
            return;
        }
    }
    if (neovim && event.modifiers == CONTROL &&
        (event.value.special == SpecialKey::PageUp || event.value.special == SpecialKey::PageDown))
    {
        if (state.operatorKey || state.waiting != WaitingKind::None || IsVisual(state.mode) ||
            IsSelect(state.mode))
        {
            InvalidateTrackedState(state, "Tab navigation during pending / Visual command; resync");
        }
        else
        {
            Complete(state);
        }
        return;
    }
    const auto character = event.value.character;
    const auto waiting = state.waiting;
    if (waiting == WaitingKind::Literal)
    {
        state.waiting = WaitingKind::None;
        ClearPending(state);
        return;
    }
    if (waiting == WaitingKind::DigraphFirst)
    {
        state.waiting = WaitingKind::DigraphSecond;
        return;
    }
    if (waiting == WaitingKind::DigraphSecond)
    {
        state.waiting = WaitingKind::None;
        ClearPending(state);
        return;
    }
    if (waiting == WaitingKind::MacroRecord && neovim &&
        !(event.modifiers & (CONTROL | OPTION | COMMAND)) &&
        (character == U':' || character == U'/' || character == U'?'))
    {
        state.returnMode = VimMode::Normal;
        OpenCommandWindow(state);
        return;
    }
    if (waiting == WaitingKind::Find || waiting == WaitingKind::ReplaceCharacter ||
        waiting == WaitingKind::Register || waiting == WaitingKind::MarkSet ||
        waiting == WaitingKind::MarkJump || waiting == WaitingKind::MacroRecord ||
        waiting == WaitingKind::MacroPlay)
    {
        if (!character || (event.modifiers & (CONTROL | OPTION | COMMAND)))
        {
            InvalidateTrackedState(state, "Unsupported command argument");
            return;
        }
        if (waiting == WaitingKind::Register)
        {
            if (std::u32string_view(
                    U"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\"_-+*/.%:#")
                    .find(character) == std::u32string_view::npos)
            {
                InvalidateTrackedState(state, "Unsupported register argument");
                return;
            }
            AppendPendingKey(state, event);
            state.waiting = state.registerReturn;
            state.registerReturn = WaitingKind::None;
            if (IsTextEntry(state.mode))
            {
                Complete(state);
            }
            return;
        }
        if (waiting == WaitingKind::MacroPlay)
        {
            InvalidateTrackedState(state, "Macro execution can change hidden state");
            return;
        }
        if (waiting == WaitingKind::MacroRecord)
        {
            state.recording = true;
        }
        if (waiting == WaitingKind::ReplaceCharacter && IsVisual(state.mode))
        {
            LeaveVisual(state);
        }
        CompleteOperatorTarget(state);
        return;
    }
    if (waiting == WaitingKind::SurroundTarget || waiting == WaitingKind::SurroundReplacement)
    {
        if (!character || (event.modifiers & (CONTROL | OPTION | COMMAND)))
        {
            InvalidateTrackedState(state, "Unsupported surround character");
            return;
        }
        AppendPendingKey(state, event);
        if (waiting == WaitingKind::SurroundTarget && OperatorIs(state, "cs"))
        {
            state.waiting = WaitingKind::SurroundReplacement;
            return;
        }
        Complete(state);
        return;
    }
    if (character == U'0' && event.modifiers == 0 && waiting == WaitingKind::None &&
        (state.mode == VimMode::Normal || IsVisual(state.mode)) &&
        (state.operatorKey ? state.motionCount : state.count))
    {
        auto& count = state.operatorKey ? state.motionCount : state.count;
        count = AppendCount(count, character);
        AppendPendingKey(state, event);
        return;
    }
    auto binding_modifiers = event.modifiers;
    if ((character >= U'A' && character <= U'Z') || (character >= U'a' && character <= U'z'))
    {
        binding_modifiers = static_cast<std::uint8_t>((binding_modifiers & ~SHIFT) |
                                                      ((character <= U'Z') ? SHIFT : 0));
    }
    const auto id = FindBinding(state, event.value, binding_modifiers);
    if (id == NO_HINT)
    {
        if (state.mode == VimMode::Terminal)
        {
            ClearContinuation(state);
            return;
        }
        if (state.waiting == WaitingKind::CtrlX)
        {
            // Neovim's CTRL-X mode takes Ctrl keys as completion/scroll commands; any other key
            // ends it and is inserted normally.
            if (neovim && ((event.modifiers & CONTROL) || !(event.modifiers & (OPTION | COMMAND))))
            {
                Complete(state);
                return;
            }
            InvalidateTrackedState(state, "Unsupported Ctrl-x command; focus editor and resync");
            return;
        }
        if (IsTextEntry(state.mode) && !(event.modifiers & (CONTROL | OPTION | COMMAND)))
        {
            return;
        }
        InvalidateTrackedState(state, "Unsupported command; focus editor and press Esc twice");
        return;
    }
    const auto& binding = VimBindings()[id];
    AppendPendingKey(state, event);
    using A = BindingAction;
    switch (binding.action)
    {
        case A::Escape:
            Escape(state);
            break;
        case A::Count:
        {
            auto& count = state.operatorKey ? state.motionCount : state.count;
            count = AppendCount(count, character);
            break;
        }
        case A::Insert:
            if (IsVisual(state.mode))
            {
                LeaveVisual(state);
            }
            state.mode = VimMode::Insert;
            Complete(state);
            break;
        case A::SelectMultiple:
            if (state.mode == VimMode::Normal)
            {
                state.mode = VimMode::Visual;
            }
            Complete(state);
            break;
        case A::SelectMatch:
            if (!state.operatorKey)
            {
                state.mode = VimMode::Visual;
            }
            Complete(state);
            break;
        case A::KeepVisual:
            Complete(state);
            break;
        case A::SelectAll:
            if (state.mode == VimMode::Normal || IsVisual(state.mode))
            {
                state.mode = VimMode::Visual;
            }
            Complete(state);
            break;
        case A::Digraph:
            state.waiting = WaitingKind::DigraphFirst;
            break;
        case A::PrefixCtrlX:
            state.waiting = WaitingKind::CtrlX;
            break;
        case A::ExInput:
            break;
        case A::Replace:
            state.mode = VimMode::Replace;
            Complete(state);
            break;
        case A::VirtualReplace:
            state.mode = VimMode::VirtualReplace;
            Complete(state);
            break;
        case A::ExMode:
            state.returnContext = VimContext::Editor;
            state.returnMode = VimMode::Ex;
            state.context = VimContext::CommandLine;
            state.mode = VimMode::Ex;
            ClearContinuation(state);
            AppendPending(state, ":");
            break;
        case A::SelectNode:
            if (state.mode == VimMode::Normal && !state.operatorKey)
            {
                state.mode = VimMode::Visual;
            }
            Complete(state);
            break;
        case A::Visual:
        case A::VisualLine:
        case A::VisualBlock:
        {
            const auto mode = binding.action == A::Visual       ? VimMode::Visual
                              : binding.action == A::VisualLine ? VimMode::VisualLine
                                                                : VimMode::VisualBlock;
            if (neovim && state.temporaryNormal)
            {
                state.returnMode = VimMode::Insert;
                state.temporaryNormal = false;
                state.mode = mode;
                ClearContinuation(state);
                break;
            }
            if (state.mode == mode)
            {
                LeaveVisual(state);
            }
            else
            {
                state.mode = mode;
            }
            Complete(state);
            break;
        }
        case A::Select:
        case A::SelectLine:
        case A::SelectBlock:
        {
            const auto mode = binding.action == A::Select       ? VimMode::Select
                              : binding.action == A::SelectLine ? VimMode::SelectLine
                                                                : VimMode::SelectBlock;
            state.mode = mode;
            Complete(state);
            break;
        }
        case A::SelectToggle:
            state.mode = ToggleSelect(state.mode);
            Complete(state);
            break;
        case A::TerminalPrefix:
            state.waiting = WaitingKind::TerminalPrefix;
            break;
        case A::LeaveTerminal:
            state.mode = VimMode::Normal;
            Complete(state);
            break;
        case A::Operator:
            if (IsVisual(state.mode))
            {
                LeaveVisual(state);
                if (character == U'c' && waiting != WaitingKind::G)
                {
                    state.mode = VimMode::Insert;
                }
                Complete(state);
            }
            else
            {
                state.operatorPrefix = OperatorPrefix(state, waiting);
                state.operatorKey = character;
                state.waiting = OperatorIs(state, "ds") || OperatorIs(state, "cs")
                                    ? WaitingKind::SurroundTarget
                                    : WaitingKind::None;
            }
            break;
        case A::ForcedMotion:
            break;
        case A::Motion:
            CompleteOperatorTarget(state);
            break;
        case A::RestoreVisual:
            state.mode = state.lastVisualMode;
            Complete(state);
            break;
        case A::Complete:
            if (IsVisual(state.mode))
            {
                LeaveVisual(state);
            }
            Complete(state);
            break;
        case A::Find:
            state.waiting = WaitingKind::Find;
            break;
        case A::ReplaceCharacter:
            state.waiting = WaitingKind::ReplaceCharacter;
            break;
        case A::PrefixG:
            state.waiting = WaitingKind::G;
            break;
        case A::PrefixLsp:
            state.waiting = WaitingKind::Lsp;
            break;
        case A::PrefixZ:
            state.waiting = WaitingKind::Z;
            break;
        case A::PrefixWindow:
            state.waiting = WaitingKind::Window;
            break;
        case A::PrefixUpperZ:
            state.waiting = WaitingKind::UpperZ;
            break;
        case A::PrefixBracketOpen:
            state.waiting = WaitingKind::BracketOpen;
            break;
        case A::PrefixBracketClose:
            state.waiting = WaitingKind::BracketClose;
            break;
        case A::TextObject:
            state.waiting = WaitingKind::TextObject;
            break;
        case A::Register:
            state.registerReturn = state.waiting;
            state.waiting = WaitingKind::Register;
            break;
        case A::MarkSet:
            state.waiting = WaitingKind::MarkSet;
            break;
        case A::MarkJump:
            state.waiting = WaitingKind::MarkJump;
            break;
        case A::MacroRecord:
            if (state.recording)
            {
                state.recording = false;
                Complete(state);
            }
            else
            {
                state.waiting = WaitingKind::MacroRecord;
            }
            break;
        case A::MacroPlay:
            state.waiting = WaitingKind::MacroPlay;
            break;
        case A::Search:
            state.returnMode = state.mode;
            state.returnContext = state.context;
            state.context = VimContext::Search;
            break;
        case A::Palette:
            state.returnMode = neovim ? VimMode::Normal : state.mode;
            state.returnContext = state.context;
            state.context = neovim ? VimContext::CommandLine : VimContext::Palette;
            if (neovim)
            {
                const bool range = IsVisual(state.mode) || state.count;
                ClearContinuation(state);
                state.exRange = range;
                AppendPending(state, ":");
            }
            break;
        case A::TemporaryNormal:
            state.mode = VimMode::Normal;
            state.temporaryNormal = true;
            break;
        case A::Literal:
            state.waiting = WaitingKind::Literal;
            break;
        case A::Navigation:
            if (neovim)
            {
                Complete(state);
            }
            else
            {
                ResetTrackedState(state, VimMode::Normal, "Normal assumed after pane navigation");
            }
            break;
        case A::Unknown:
            InvalidateTrackedState(state, "Command can change hidden state; resync");
            break;
    }
    if (state.confidence != Confidence::Uncertain && binding.action != A::Navigation)
    {
        state.confidence = Confidence::Inferred;
        state.reason = "Inferred from observed keys";
    }
}

const char* ModeName(VimMode mode)
{
    switch (mode)
    {
        case VimMode::Normal:
            return "NORMAL";
        case VimMode::Insert:
            return "INSERT";
        case VimMode::Visual:
            return "VISUAL";
        case VimMode::VisualLine:
            return "VISUAL LINE";
        case VimMode::VisualBlock:
            return "VISUAL BLOCK";
        case VimMode::Select:
            return "SELECT";
        case VimMode::SelectLine:
            return "SELECT LINE";
        case VimMode::SelectBlock:
            return "SELECT BLOCK";
        case VimMode::Replace:
            return "REPLACE";
        case VimMode::Terminal:
            return "TERMINAL";
        case VimMode::VirtualReplace:
            return "VIRTUAL REPLACE";
        case VimMode::Ex:
            return "EX";
    }
    return "UNKNOWN";
}

const char* WaitingName(const VimState& state)
{
    if (state.context == VimContext::Palette)
    {
        return "command palette";
    }
    if (state.context == VimContext::CommandWindow)
    {
        return "command-line window";
    }
    if (state.context == VimContext::CommandLine)
    {
        return state.mode == VimMode::Ex ? "Ex command" : "command line";
    }
    if (state.context == VimContext::Search)
    {
        return "search";
    }
    switch (state.waiting)
    {
        case WaitingKind::G:
            return "g command";
        case WaitingKind::Lsp:
            return "LSP command";
        case WaitingKind::Z:
            return "z command";
        case WaitingKind::Window:
            return "pane command";
        case WaitingKind::UpperZ:
            return "Z command";
        case WaitingKind::TextObject:
            return "text object";
        case WaitingKind::Find:
            return "find character";
        case WaitingKind::ReplaceCharacter:
            return "replacement character";
        case WaitingKind::BracketOpen:
            return "[ motion";
        case WaitingKind::BracketClose:
            return "] motion";
        case WaitingKind::Register:
            return "register";
        case WaitingKind::MarkSet:
            return "mark";
        case WaitingKind::MarkJump:
            return "mark";
        case WaitingKind::MacroRecord:
            return "macro register";
        case WaitingKind::MacroPlay:
            return "macro register";
        case WaitingKind::Literal:
            return "literal key";
        case WaitingKind::TerminalPrefix:
            return "terminal prefix";
        case WaitingKind::SurroundTarget:
            return "surround target";
        case WaitingKind::SurroundReplacement:
            return "surround replacement";
        case WaitingKind::DigraphFirst:
        case WaitingKind::DigraphSecond:
            return "digraph";
        case WaitingKind::CtrlX:
            return "Ctrl-x command";
        default:
            return state.operatorKey ? "operator motion" : "";
    }
}
