// tests/vim_tracker_tests.cpp
#include "test_helpers.hpp"
#include "gameLogic/vim_tracker.hpp"
#include "gameLogic/vim_bindings.hpp"
#include "gameLogic/keyboard_status.hpp"

// ====================================================================================================
// Plain motions, Q / K / C / Ctrl-c / Ctrl-a / text objects / Neovim defaults
// ====================================================================================================
namespace
{
    struct Stroke
    {
        char32_t character = 0;
        Platform::SpecialKey special = Platform::SpecialKey::None;
        std::uint8_t modifiers = 0;
    };

    void CheckPlainMotionsAndKeys()
    {
        using P = Platform::AppProfile;
        using SK = Platform::SpecialKey;
        VimState state{};
        const auto start = [&](P profile, VimMode mode)
        {
            state = VimState{};
            state.appProfile = profile;
            ResetTrackedState(state, mode, "test");
        };
        const auto send = [&](const Stroke& stroke)
        {
            if (stroke.special != SK::None)
            {
                TrackInput(state, {.value = {0, stroke.special}, .modifiers = stroke.modifiers});
            }
            else
            {
                TrackInput(state, Key(stroke.character, 0, stroke.modifiers));
            }
        };
        const auto press = [&](char32_t c, std::uint8_t mods = 0)
        {
            send({c, SK::None, mods});
        };
        const Stroke motions[] = {
            {U' '},
            {0, SK::Enter},
            {U'm', SK::None, Platform::CONTROL},
            {U'+'},
            {0, SK::Backspace},
            {U'-'},
            {U'_'},
            {U'|'},
            {U'('},
            {U')'},
            {U'H'},
            {U'M'},
            {U'L'},
            {U'j', SK::None, Platform::CONTROL},
            {0, SK::Home},
            {0, SK::End},
            {0, SK::PageUp},
            {0, SK::PageDown},
        };

        for (const auto profile : {P::Zed, P::Neovim})
        {
            for (const auto mode : {VimMode::Normal, VimMode::Visual})
            {
                for (const auto& motion : motions)
                {
                    start(profile, mode);
                    send(motion);
                    CHECK(state.mode == mode && state.confidence == Confidence::Inferred);
                }
            }
            for (const auto& motion : motions)
            {
                start(profile, VimMode::Normal);
                press(U'd');
                send(motion);
                CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
                      state.confidence == Confidence::Inferred);
            }
        }

        start(P::Zed, VimMode::Normal);
        send({0, SK::Tab});
        CHECK(state.confidence == Confidence::Uncertain);
        start(P::Neovim, VimMode::Normal);
        send({0, SK::Tab});
        CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
        for (const auto profile : {P::Zed, P::Neovim})
        {
            start(profile, VimMode::Insert);
            send({0, SK::Enter});
            send({0, SK::Backspace});
            CHECK(state.mode == VimMode::Insert && state.confidence != Confidence::Uncertain);
        }

        for (const auto profile : {P::Zed, P::Neovim})
        {
            start(profile, VimMode::Normal);
            const auto hint = FindBinding(state, {U'Q'}, Platform::SHIFT);
            CHECK(hint != NO_HINT &&
                  VimBindings()[hint].fullHint == "replay last macro (resync needed)");
            press(U'Q');
            CHECK(state.mode == VimMode::Normal && state.context == VimContext::Editor &&
                  state.confidence == Confidence::Uncertain);
            start(profile, VimMode::Visual);
            press(U'Q');
            CHECK(state.confidence == Confidence::Uncertain);
            start(profile, VimMode::Visual);
            press(U'@');
            CHECK(state.waiting == WaitingKind::MacroPlay);
            press(U'a');
            CHECK(state.confidence == Confidence::Uncertain);
        }
        start(P::Neovim, VimMode::Normal);
        press(U'g');
        press(U'Q');
        CHECK(state.mode == VimMode::Ex && state.context == VimContext::CommandLine);

        start(P::Neovim, VimMode::Visual);
        press(U'C');
        CHECK(state.mode == VimMode::Insert && state.confidence != Confidence::Uncertain);
        start(P::Zed, VimMode::Visual);
        press(U'C');
        CHECK(state.confidence == Confidence::Uncertain);
        for (const auto profile : {P::Zed, P::Neovim})
        {
            start(profile, VimMode::Normal);
            press(U'C');
            CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred);
        }

        for (const auto profile : {P::Zed, P::Neovim})
        {
            start(profile, VimMode::Normal);
            press(U'c', Platform::CONTROL);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed &&
                  state.pendingLength == 0);
            start(profile, VimMode::Normal);
            press(U'd');
            press(U'c', Platform::CONTROL);
            CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
                  state.confidence == Confidence::Inferred);
            start(profile, VimMode::Visual);
            press(U'c', Platform::CONTROL);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
            start(profile, VimMode::Insert);
            press(U'c', Platform::CONTROL);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
        }

        for (const auto profile : {P::Zed, P::Neovim})
        {
            start(profile, VimMode::Normal);
            press(U'K', Platform::SHIFT);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
            start(profile, VimMode::Visual);
            press(U'K', Platform::SHIFT);
            CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
            start(profile, VimMode::VisualLine);
            press(U'K', Platform::SHIFT);
            CHECK(state.mode == VimMode::VisualLine && state.confidence == Confidence::Inferred);
            start(profile, VimMode::Normal);
            press(U'a', Platform::CONTROL);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
            start(profile, VimMode::Normal);
            press(U'x', Platform::CONTROL);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
        }

        for (const auto profile : {P::Zed, P::Neovim})
        {
            for (const char32_t object : std::u32string_view(U"tB"))
            {
                start(profile, VimMode::Normal);
                press(U'd');
                press(U'i');
                press(object);
                CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
                      state.confidence == Confidence::Inferred);
                start(profile, VimMode::Normal);
                press(U'c');
                press(U'a');
                press(object);
                CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred);
                start(profile, VimMode::Visual);
                press(U'i');
                press(object);
                CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
            }
        }
        for (const char32_t object : std::u32string_view(U"q|raiIfce"))
        {
            start(P::Zed, VimMode::Normal);
            press(U'd');
            press(U'i');
            press(object);
            CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
                  state.confidence == Confidence::Inferred);
            start(P::Neovim, VimMode::Normal);
            press(U'd');
            press(U'i');
            press(object);
            CHECK(state.confidence == Confidence::Uncertain);
        }
        start(P::Neovim, VimMode::Visual);
        press(U'i');
        press(U'n');
        CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
        start(P::Neovim, VimMode::Visual);
        press(U'a');
        press(U'n');
        CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
        start(P::Neovim, VimMode::Normal);
        press(U'd');
        press(U'i');
        press(U'n');
        CHECK(state.confidence == Confidence::Uncertain);
        start(P::Zed, VimMode::Visual);
        press(U'i');
        press(U'n');
        CHECK(state.confidence == Confidence::Uncertain);

        start(P::Neovim, VimMode::Normal);
        press(U'&');
        CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
        start(P::Neovim, VimMode::Normal);
        press(U'l', Platform::CONTROL);
        CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
        start(P::Zed, VimMode::Normal);
        press(U'&');
        CHECK(state.confidence == Confidence::Uncertain);
        start(P::Zed, VimMode::Normal);
        press(U'l', Platform::CONTROL);
        CHECK(state.confidence == Confidence::Uncertain);
    }
} // namespace

int main()
{
    VimState state{};
    const auto baseline = [&]
    {
        ResetTrackedState(state, VimMode::Normal, "test");
    };
    const auto key = [&](char32_t c, std::uint8_t mods = 0)
    {
        TrackInput(state, Key(c, 0, mods));
    };
    key(U'i');
    CHECK(state.mode == VimMode::Insert);
    key(U'd');
    CHECK(state.mode == VimMode::Insert && !state.operatorKey);
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'R');
    CHECK(state.mode == VimMode::Replace);
    key(U'd');
    CHECK(state.mode == VimMode::Replace);
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'v');
    CHECK(state.mode == VimMode::Visual);
    key(U'd');
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'V');
    CHECK(state.mode == VimMode::VisualLine);
    baseline();
    key(U'v', Platform::CONTROL);
    CHECK(state.mode == VimMode::VisualBlock);
    baseline();
    key(U'2');
    key(U'd');
    CHECK(state.operatorKey == U'd' && state.count == 2);
    key(U'3');
    CHECK(state.motionCount == 3);
    key(U'w');
    CHECK(!state.operatorKey && state.count == 0 && state.motionCount == 0);
    baseline();
    key(U'1');
    key(U'0');
    CHECK(state.count == 10);
    key(U'w');
    CHECK(!state.count);
    baseline();
    key(U'2');
    key(U'd');
    key(U'0');
    CHECK(!state.operatorKey && !state.count && state.mode == VimMode::Normal);
    baseline();
    key(U'c');
    key(U'i');
    CHECK(state.operatorKey == U'c' && state.waiting == WaitingKind::TextObject);
    key(U'w');
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None &&
          !state.operatorKey);
    baseline();
    key(U'd');
    key(U'f');
    CHECK(state.waiting == WaitingKind::Find && state.operatorKey == U'd');
    key(U'a');
    CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
          state.waiting == WaitingKind::None);
    baseline();
    key(U'r');
    CHECK(state.waiting == WaitingKind::ReplaceCharacter);
    key(U'i');
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'g');
    CHECK(state.waiting == WaitingKind::G);
    key(U'g');
    CHECK(state.waiting == WaitingKind::None);
    baseline();
    key(U'd');
    key(U'd');
    CHECK(state.mode == VimMode::Normal && !state.operatorKey);
    baseline();
    key(U'c');
    key(U'c');
    CHECK(state.mode == VimMode::Insert);
    key(U'o', Platform::CONTROL);
    CHECK(state.mode == VimMode::Normal && state.temporaryNormal);
    key(U'd');
    key(U'f');
    CHECK(state.mode == VimMode::Normal && state.temporaryNormal);
    key(U'a');
    CHECK(state.mode == VimMode::Insert && !state.temporaryNormal);
    key(U'v', Platform::CONTROL);
    CHECK(state.waiting == WaitingKind::Literal);
    TrackInput(state, Esc(2));
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None);
    baseline();
    key(U':');
    CHECK(state.context == VimContext::Palette);
    key(U'i');
    CHECK(state.mode == VimMode::Normal);
    TrackInput(state, Esc(1));
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal);
    key(U':');
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'/');
    key(U'i');
    CHECK(state.context == VimContext::Search);
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal);
    baseline();
    key(U'@');
    key(U'a');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'.');
    CHECK(state.confidence == Confidence::Uncertain);
    key(U'i');
    CHECK(state.mode == VimMode::Normal);
    TrackInput(state, Esc(1));
    TrackInput(state, Esc(1.1, false, true));
    CHECK(state.confidence == Confidence::Uncertain);
    TrackInput(state, Esc(1.2, true));
    TrackInput(state, Esc(1.3));
    CHECK(state.confidence == Confidence::Assumed && state.mode == VimMode::Normal);
    InvalidateTrackedState(state, "test");
    TrackInput(state, Esc(2));
    TrackInput(state, Esc(2.1, true));
    key(U'i');
    TrackInput(state, Esc(2.2));
    CHECK(state.confidence == Confidence::Uncertain);
    TrackInput(state, Esc(2.3, true));
    TrackInput(state, Esc(4));
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'i');
    TrackInput(state,
               {.value = {0, Platform::SpecialKey::PageDown}, .modifiers = Platform::CONTROL});
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    key(U'c');
    TrackInput(state, {.type = Platform::EventType::MouseAction});
    CHECK(state.confidence == Confidence::Uncertain && !state.operatorKey);
    baseline();
    key(U'"');
    CHECK(state.waiting == WaitingKind::Register);
    key(U'a');
    key(U'd');
    key(U'w');
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'q');
    key(U'a');
    CHECK(state.recording);
    key(U'i');
    key(U'x');
    TrackInput(state, Esc(1));
    key(U'q');
    CHECK(!state.recording);
    baseline();
    key(U'd');
    key(U'f');
    key(U'[', Platform::CONTROL);
    CHECK(state.mode == VimMode::Normal && state.waiting == WaitingKind::None &&
          !state.operatorKey && state.confidence != Confidence::Uncertain);
    baseline();
    key(U'i');
    key(U'v', Platform::CONTROL);
    key(U'i');
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None);
    baseline();
    key(U'i');
    key(U'k', Platform::CONTROL);
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::DigraphFirst);
    baseline();
    key(U']');
    CHECK(state.waiting == WaitingKind::BracketClose);
    key(U'b');
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    baseline();
    key(U'c');
    key(U'[');
    key(U'[');
    CHECK(state.mode == VimMode::Insert && !state.operatorKey);
    baseline();
    auto caps = Key(U'R');
    caps.modifiers = 0;
    caps.capsLock = true;
    TrackInput(state, caps);
    CHECK(state.mode == VimMode::Replace && state.modifiers == 0 && state.capsLock);
    baseline();
    key(U'v');
    key(U'U');
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'd');
    key(U'g');
    key(U'u');
    CHECK(state.confidence == Confidence::Uncertain);
    const auto pending = [&]
    {
        return std::string_view(state.pendingSequence.data(), state.pendingLength);
    };
    baseline();
    key(U'2');
    key(U'd');
    key(U'3');
    CHECK(pending() == "2d3");
    key(U'w');
    CHECK(pending().empty());
    baseline();
    key(U'd');
    key(U'2');
    key(U'0');
    CHECK(pending() == "d20");
    TrackInput(state, Esc(1));
    CHECK(pending().empty());
    baseline();
    key(U'c');
    key(U'i');
    CHECK(pending() == "ci");
    key(U'w');
    CHECK(pending().empty());
    baseline();
    key(U'd');
    key(U'F');
    CHECK(pending() == "dF");
    key(U'a');
    CHECK(pending().empty());
    baseline();
    key(U'g');
    CHECK(pending() == "g");
    key(U'g');
    CHECK(pending().empty());
    baseline();
    key(U'w', Platform::CONTROL);
    CHECK(pending() == "Ctrl-w");
    key(U'h');
    CHECK(pending().empty());
    baseline();
    key(U'"');
    key(U'a');
    key(U'd');
    CHECK(pending() == "\"ad");
    key(U'w');
    CHECK(pending().empty());
    baseline();
    key(U'i');
    key(U'o', Platform::CONTROL);
    key(U'd');
    CHECK(pending() == "Ctrl-od");
    key(U'w');
    CHECK(pending().empty() && state.mode == VimMode::Insert);
    key(U'v', Platform::CONTROL);
    CHECK(pending() == "Ctrl-v");
    key(U'x');
    CHECK(pending().empty());
    key(U'v', Platform::CONTROL);
    TrackInput(state, Esc(2));
    CHECK(pending().empty());
    key(U'p');
    key(U'r');
    key(U'i');
    key(U'v');
    key(U'a');
    key(U't');
    key(U'e');
    CHECK(pending().empty());
    baseline();
    key(U'/');
    key(U'p');
    key(U'r');
    key(U'i');
    CHECK(pending() == "/");
    TrackInput(state, Esc(1));
    CHECK(pending().empty());
    baseline();
    key(U':');
    key(U'p');
    key(U'r');
    key(U'i');
    CHECK(pending() == ":");
    TrackInput(state, Esc(1));
    CHECK(pending().empty());
    baseline();
    for (int i = 0; i < 40; ++i)
    {
        key(U'9');
    }
    CHECK(pending().size() == VimState::PENDING_CAPACITY && state.pendingTruncated);
    CHECK(state.pendingSequence[state.pendingLength] == '\0');
    key(U'w');
    CHECK(pending().empty() && !state.pendingTruncated);
    key(U'd');
    InvalidateTrackedState(state, "test");
    CHECK(pending().empty());
    baseline();
    key(U'd');
    ResetTrackedState(state, VimMode::Normal, "focus");
    CHECK(pending().empty());
    key(U'd');
    key(U'g');
    key(U'u');
    CHECK(pending().empty() && state.confidence == Confidence::Uncertain);
    Platform::InputBatchInfo input{
        .foreground = true, .available = true, .description = "Observing Zed"};
    baseline();
    key(U'd');
    key(U'2');
    auto status = BuildKeyboardStatus(state, input);
    CHECK(status.kind == StatusKind::Mode && status.label == "NORMAL" && status.pending == "d2");
    CHECK(status.detail.find("operator motion") != std::string::npos);
    InvalidateTrackedState(state, "Focus editor and press Esc twice");
    status = BuildKeyboardStatus(state, input);
    CHECK(status.kind == StatusKind::Resync && status.pending.empty());
    CHECK(status.detail.find("Esc twice") != std::string::npos);
    input.available = false;
    CHECK(BuildKeyboardStatus(state, input).label == "INPUT OFF");
    input.foreground = false;
    CHECK(BuildKeyboardStatus(state, input).label == "INACTIVE");
    input.paused = true;
    CHECK(BuildKeyboardStatus(state, input).label == "PAUSED");
    input.paused = false;
    input.preview = true;
    CHECK(BuildKeyboardStatus(state, input).label == "INACTIVE");
    input.foreground = true;
    ResetTrackedState(state, VimMode::VisualBlock, "test");
    CHECK(BuildKeyboardStatus(state, input).label == "VISUAL BLOCK");
    ResetTrackedState(state, VimMode::Normal, "test");
    key(U'q');
    key(U'a');
    CHECK(BuildKeyboardStatus(state, input).pending == "REC");
    key(U'q');
    state.appProfile = Platform::AppProfile::Neovim;
    baseline();
    CHECK(state.appProfile == Platform::AppProfile::Neovim);
    auto hint = FindBinding(state, {U'Y'}, Platform::SHIFT);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "yank to line end");
    key(U'2');
    key(U'd');
    key(U'3');
    key(U'w');
    CHECK(state.mode == VimMode::Normal && !state.operatorKey && !state.count &&
          state.pendingLength == 0);
    key(U'c');
    key(U'i');
    key(U'w');
    CHECK(state.mode == VimMode::Insert && !state.operatorKey);
    CHECK(FindBinding(state, {U'b'}, Platform::CONTROL) == NO_HINT);
    CHECK(FindBinding(state, {U'd'}, Platform::CONTROL) != NO_HINT);
    key(U'd', Platform::CONTROL);
    key(U't', Platform::CONTROL);
    key(U'j', Platform::CONTROL);
    CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred &&
          state.pendingLength == 0);
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    TrackInput(state, {.value = {0, Platform::SpecialKey::Tab}});
    TrackInput(state, {.value = {0, Platform::SpecialKey::Left}});
    CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred);
    key(U'r', Platform::CONTROL);
    key(U'a');
    CHECK(state.waiting == WaitingKind::None && state.pendingLength == 0 &&
          state.mode == VimMode::Insert);
    key(U'o', Platform::CONTROL);
    key(U'w', Platform::CONTROL);
    key(U'h');
    CHECK(state.mode == VimMode::Insert && !state.temporaryNormal && state.pendingLength == 0);
    key(U'o', Platform::CONTROL);
    key(U']');
    key(U'b');
    CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred &&
          !state.temporaryNormal);
    TrackInput(state,
               {.value = {0, Platform::SpecialKey::PageDown}, .modifiers = Platform::CONTROL});
    CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred);
    baseline();
    key(U'v');
    key(U'[');
    CHECK(FindBinding(state, {U'b'}, 0) == NO_HINT);
    key(U'b');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    CHECK(FindBinding(state, {U'='}, 0) == NO_HINT);
    key(U'=');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U':');
    key(U's');
    key(U't');
    key(U'a');
    key(U'r');
    key(U't');
    key(U'i');
    CHECK(state.context == VimContext::CommandLine && state.mode == VimMode::Normal &&
          pending() == ":starti");
    CHECK(BuildKeyboardStatus(state, input).label == "COMMAND");
    CHECK(std::string_view(WaitingName(state)) == "command line");
    TrackInput(state, Esc(1));
    CHECK(state.context == VimContext::Editor && state.pendingLength == 0);
    key(U'v');
    key(U':');
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Normal);
    key(U':');
    key(U'c', Platform::CONTROL);
    CHECK(state.context == VimContext::Editor);
    key(U'/');
    key(U'm', Platform::CONTROL);
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal);
    key(U'i');
    key(U'o', Platform::CONTROL);
    key(U'/');
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Insert && !state.temporaryNormal);
    baseline();
    key(U':');
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    CHECK(state.confidence == Confidence::Inferred && state.context == VimContext::Editor);
    key(U':');
    key(U'l');
    key(U'u');
    key(U'a');
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    CHECK(state.confidence == Confidence::Uncertain); // Script execution still has hidden effects.
    TrackInput(state, Esc(2));
    TrackInput(state, Esc(2.1, true));
    TrackInput(state, Esc(2.2));
    CHECK(state.confidence == Confidence::Assumed &&
          state.appProfile == Platform::AppProfile::Neovim);
    key(U'R');
    key(U'e', Platform::CONTROL);
    CHECK(state.mode == VimMode::Replace);
    key(U'w', Platform::CONTROL);
    key(U'r', Platform::CONTROL);
    key(U'a');
    CHECK(state.mode == VimMode::Replace && state.waiting == WaitingKind::None &&
          state.pendingLength == 0);
    TrackInput(state, Esc(3));
    CHECK(state.mode == VimMode::Normal);
    key(U'v', Platform::CONTROL);
    key(U'c');
    CHECK(state.mode == VimMode::Insert);
    // Ghostty consumes these shortcuts before the terminal application sees them.
    for (const auto event :
         {Key(U'2', 0, Platform::COMMAND), Key(U't', 0, Platform::COMMAND),
          Key(U'd', 0, Platform::COMMAND),
          Platform::InputEvent{.key = Platform::KeyId::Two, .modifiers = Platform::COMMAND},
          Platform::InputEvent{.key = Platform::KeyId::LeftBracket,
                               .modifiers = Platform::COMMAND | Platform::SHIFT},
          Platform::InputEvent{.value = {0, Platform::SpecialKey::Tab},
                               .modifiers = Platform::CONTROL},
          Platform::InputEvent{.value = {0, Platform::SpecialKey::Tab},
                               .modifiers = Platform::CONTROL | Platform::SHIFT},
          Platform::InputEvent{.value = {0, Platform::SpecialKey::Up},
                               .modifiers = Platform::COMMAND | Platform::OPTION}})
    {
        baseline();
        key(U'i');
        TrackInput(state, event);
        CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed &&
              state.pendingLength == 0);
    }
    baseline();
    key(U'd');
    key(U'c', Platform::COMMAND);
    key(U'=', Platform::COMMAND);
    CHECK(state.operatorKey == U'd' && pending() == "d");
    key(U'w');
    CHECK(!state.operatorKey);
    key(U'i');
    key(U'v', Platform::COMMAND);
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'P', Platform::COMMAND);
    CHECK(state.confidence == Confidence::Uncertain);
    input.preview = false;
    input.foreground = false;
    input.appProfile = Platform::AppProfile::Neovim;
    CHECK(BuildKeyboardStatus(state, input).label == "INACTIVE");
    state.appProfile = Platform::AppProfile::Zed;
    baseline();
    hint = FindBinding(state, {U'Y'}, Platform::SHIFT);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "yank line");
    key(U'i');
    CHECK(FindBinding(state, {U'b'}, Platform::CONTROL) != NO_HINT);
    // Selections remain selections through endpoint swaps; I/A enter Insert in both editors.
    for (auto profile : {Platform::AppProfile::Zed, Platform::AppProfile::Neovim})
    {
        state.appProfile = profile;
        for (auto insert : {U'I', U'A'})
        {
            baseline();
            key(U'v', Platform::CONTROL);
            key(U'j');
            key(U'j');
            key(U'j');
            CHECK(state.mode == VimMode::VisualBlock && state.confidence == Confidence::Inferred);
            hint = FindBinding(state, {insert}, Platform::SHIFT);
            CHECK(hint != NO_HINT &&
                  VimBindings()[hint].fullHint.find("block row") != std::string_view::npos);
            key(insert);
            CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None &&
                  pending().empty());
            key(U'x');
            TrackInput(state, Esc(10));
            CHECK(state.mode == VimMode::Normal && state.confidence != Confidence::Uncertain);
            baseline();
            key(U'V');
            key(insert);
            CHECK(state.mode == VimMode::Insert);
        }
        baseline();
        key(U'v');
        key(U'o');
        key(U'O');
        CHECK(state.mode == VimMode::Visual && pending().empty());
        key(U'g');
        key(U'n');
        CHECK(state.mode == VimMode::Visual);
        baseline();
        key(U'c');
        key(U'g');
        key(U'n');
        CHECK(state.mode == VimMode::Insert && !state.operatorKey);
        baseline();
        key(U'd');
        key(U'g');
        key(U'N');
        CHECK(state.mode == VimMode::Normal && !state.operatorKey);
    }
    state.appProfile = Platform::AppProfile::Zed;
    for (auto selection : {U'l', U'L', U'a', U'>', U'<'})
    {
        baseline();
        key(U'g');
        CHECK(FindBinding(state, {selection}, Key(selection).modifiers) != NO_HINT);
        key(selection);
        CHECK(state.mode == VimMode::Visual && pending().empty() &&
              state.confidence == Confidence::Inferred);
        key(U'c');
        CHECK(state.mode == VimMode::Insert);
    }
    for (auto insert : {U'I', U'A'})
    {
        baseline();
        key(U'V');
        key(U'j');
        key(U'g');
        CHECK(FindBinding(state, {insert}, Platform::SHIFT) != NO_HINT);
        key(insert);
        CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None &&
              pending().empty());
        baseline();
        key(U'g');
        const auto normal_hint = FindBinding(state, {insert}, Platform::SHIFT);
        CHECK(normal_hint == NO_HINT || VimBindings()[normal_hint].action != BindingAction::Insert);
    }
    baseline();
    key(U'd');
    key(U'g');
    CHECK(FindBinding(state, {U'l'}, 0) == NO_HINT);
    baseline();
    key(U'd', Platform::COMMAND);
    CHECK(state.mode == VimMode::Visual);
    key(U'I');
    key(U'p', Platform::COMMAND | Platform::CONTROL);
    CHECK(state.mode == VimMode::Insert && state.confidence == Confidence::Inferred);
    baseline();
    TrackInput(state, {.value = {0, Platform::SpecialKey::Up},
                       .modifiers = Platform::COMMAND | Platform::OPTION});
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
    state.appProfile = Platform::AppProfile::Neovim;
    baseline();
    key(U'g');
    CHECK(FindBinding(state, {U'l'}, 0) == NO_HINT);
    key(U'l');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'V');
    key(U'g');
    CHECK(FindBinding(state, {U'A'}, Platform::SHIFT) == NO_HINT);

    const auto text = [&](std::u32string_view sequence)
    {
        for (auto c : sequence)
        {
            key(c);
        }
    };
    const auto enter = [&]
    {
        TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    };
    const auto backspace = [&]
    {
        TrackInput(state, {.value = {0, Platform::SpecialKey::Backspace}});
    };
    baseline();
    key(U':');
    CHECK(FindBinding(state, {U'w'}, 0) != NO_HINT && FindBinding(state, {U'q'}, 0) != NO_HINT);
    CHECK(FindBinding(state, {U'i'}, 0) ==
          NO_HINT); // Editor Insert bindings are hidden in the command line.
    key(U'w');
    hint = FindBinding(state, {U'q'}, 0);
    CHECK(pending() == ":w" && hint != NO_HINT && VimBindings()[hint].chord == ":wq");
    CHECK(FindBinding(state, {U'e'}, 0) == NO_HINT);
    key(U'q');
    CHECK(pending() == ":wq" && FindBinding(state, {U'a'}, 0) != NO_HINT);
    enter();
    CHECK(state.context == VimContext::Editor && state.confidence == Confidence::Uncertain &&
          pending().empty());
    for (auto command : {U":w", U":write!", U":noh", U":bn", U":sp", U":vs", U":%w"})
    {
        baseline();
        text(command);
        enter();
        CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred &&
              pending().empty());
    }
    baseline();
    text(U":w!");
    backspace();
    CHECK(pending() == ":w" && !state.exBang);
    backspace();
    CHECK(pending() == ":");
    backspace();
    CHECK(state.context == VimContext::Editor && pending().empty());
    baseline();
    text(U":wq");
    key(U'u', Platform::CONTROL);
    CHECK(pending() == ":");
    text(U"write");
    key(U'w', Platform::CONTROL);
    CHECK(pending() == ":");
    text(U"w");
    enter();
    CHECK(state.confidence == Confidence::Inferred);
    baseline();
    text(U":startinsert");
    enter();
    CHECK(state.mode == VimMode::Insert);
    key(U'o', Platform::CONTROL);
    text(U":startreplace");
    enter();
    CHECK(state.mode == VimMode::Replace && !state.temporaryNormal);
    baseline();
    key(U'i');
    key(U'o', Platform::CONTROL);
    text(U":stopinsert");
    enter();
    CHECK(state.mode == VimMode::Normal && !state.temporaryNormal);
    key(U'i');
    key(U'o', Platform::CONTROL);
    text(U":w");
    enter();
    CHECK(state.mode == VimMode::Insert && !state.temporaryNormal);
    baseline();
    text(U":write private_filename.txt");
    CHECK(pending() == ":write" && state.exArguments && FindBinding(state, {U'q'}, 0) == NO_HINT);
    key(U'u', Platform::CONTROL);
    CHECK(pending() == ":" && !state.exArguments);
    for (auto command : {U":w|startinsert", U":lua private_code", U":unknown",
                         U":edit +startinsert file", U":wq!"})
    {
        baseline();
        text(command);
        CHECK(pending().size() <= VimState::PENDING_CAPACITY);
        enter();
        CHECK(state.confidence == Confidence::Uncertain && pending().empty());
    }
    baseline();
    text(U":w");
    TrackInput(state, {.value = {0, Platform::SpecialKey::Left}});
    CHECK(state.context == VimContext::CommandLine && state.exOpaque &&
          FindBinding(state, {U'q'}, 0) == NO_HINT);
    key(U'u', Platform::CONTROL);
    enter();
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    text(U":w");
    key(U'v', Platform::CONTROL);
    TrackInput(state, Esc(20));
    CHECK(state.context == VimContext::CommandLine && state.waiting == WaitingKind::None);
    key(U'w');
    enter();
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    text(U":wq");
    TrackInput(state, Esc(21));
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal &&
          pending().empty() && !state.exBang && !state.exOpaque);
    baseline();
    text(U":w");
    TrackInput(state, {.type = Platform::EventType::Unsupported});
    key(U'u', Platform::CONTROL);
    enter();
    CHECK(state.confidence == Confidence::Uncertain && pending().empty());
    state.appProfile = Platform::AppProfile::Zed;
    baseline();
    text(U":wq");
    CHECK(state.context == VimContext::Palette && pending() == ":");
    CHECK(FindBinding(state, {U'q'}, 0) == NO_HINT);
    enter();
    CHECK(state.confidence == Confidence::Uncertain);

    state.appProfile = Platform::AppProfile::Neovim;
    CHECK(std::string_view(ModeName(VimMode::Select)) == "SELECT");
    CHECK(std::string_view(ModeName(VimMode::SelectLine)) == "SELECT LINE");
    CHECK(std::string_view(ModeName(VimMode::SelectBlock)) == "SELECT BLOCK");
    CHECK(std::string_view(ModeName(VimMode::Terminal)) == "TERMINAL");

    baseline();
    key(U'g');
    key(U'h');
    CHECK(state.mode == VimMode::Select && state.confidence == Confidence::Inferred &&
          pending().empty());
    baseline();
    key(U'g');
    key(U'H');
    CHECK(state.mode == VimMode::SelectLine && pending().empty());
    baseline();
    key(U'g');
    key(U'h', Platform::CONTROL);
    CHECK(state.mode == VimMode::SelectBlock && pending().empty());
    baseline();
    key(U'v');
    key(U'g');
    key(U'h');
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Uncertain);
    state.appProfile = Platform::AppProfile::Zed;
    baseline();
    key(U'g');
    key(U'h');
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
    baseline();
    key(U'v');
    key(U'g', Platform::CONTROL);
    CHECK(state.confidence == Confidence::Uncertain);
    state.appProfile = Platform::AppProfile::Neovim;

    baseline();
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Uncertain);
    baseline();
    key(U'v');
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::Select && state.confidence == Confidence::Inferred);
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
    baseline();
    key(U'V');
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::SelectLine);
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::VisualLine);
    baseline();
    key(U'v', Platform::CONTROL);
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::SelectBlock);
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::VisualBlock);

    baseline();
    key(U'g');
    key(U'h');
    TrackInput(state, {.value = {0, Platform::SpecialKey::Right}});
    CHECK(state.mode == VimMode::Select && state.confidence == Confidence::Inferred);
    key(U'd', Platform::CONTROL);
    CHECK(state.mode == VimMode::Select && state.confidence == Confidence::Inferred);
    key(U'x');
    CHECK(state.mode == VimMode::Insert && !state.operatorKey && pending().empty());
    baseline();
    key(U'g');
    key(U'H');
    key(U'd');
    CHECK(state.mode == VimMode::Insert && !state.operatorKey);
    baseline();
    key(U'g');
    key(U'h', Platform::CONTROL);
    key(U'A');
    CHECK(state.mode == VimMode::Insert && pending().empty());
    baseline();
    key(U'v');
    key(U'g', Platform::CONTROL);
    key(U'2');
    CHECK(state.mode == VimMode::Insert && state.count == 0);
    baseline();
    key(U'g');
    key(U'h');
    TrackInput(state, Esc(80));
    CHECK(state.mode == VimMode::Normal && state.confidence != Confidence::Uncertain);
    TrackInput(state, Esc(80.1, true));
    baseline();
    key(U'v');
    key(U'g', Platform::CONTROL);
    TrackInput(state, Esc(81));
    CHECK(state.mode == VimMode::Normal);
    baseline();
    key(U'g');
    key(U'h');
    state.appProfile = Platform::AppProfile::Neovim;
    const Platform::InputBatchInfo ready{
        .foreground = true, .available = true, .description = "Observing Neovim"};
    baseline();
    key(U'g');
    key(U'R');
    CHECK(state.mode == VimMode::VirtualReplace && state.confidence == Confidence::Inferred &&
          pending().empty());
    status = BuildKeyboardStatus(state, ready);
    CHECK(status.label == "VIRTUAL REPLACE");
    hint = FindBinding(state, {U'd'}, Platform::CONTROL);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "decrease indent");
    key(U'd');
    CHECK(state.mode == VimMode::VirtualReplace && pending().empty());
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Inferred &&
          pending().empty());
    baseline();
    key(U'v');
    key(U'g');
    CHECK(FindBinding(state, {U'R'}, Platform::SHIFT) == NO_HINT);
    baseline();
    key(U'g');
    key(U'R');
    CHECK(FindBinding(state, {U'R'}, Platform::SHIFT) == NO_HINT);

    baseline();
    key(U'g');
    key(U'r');
    CHECK(state.waiting == WaitingKind::Lsp && pending() == "gr" &&
          state.confidence == Confidence::Inferred);
    CHECK(std::string_view(WaitingName(state)) == "LSP command");
    hint = FindBinding(state, {U'r'}, 0);
    CHECK(hint != NO_HINT && VimBindings()[hint].chord == "grr");
    key(U'n');
    CHECK(state.waiting == WaitingKind::None && pending().empty() &&
          state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
    baseline();
    key(U'g');
    key(U'r');
    key(U'z');
    CHECK(state.confidence == Confidence::Uncertain);

    baseline();
    key(U'g');
    key(U'Q');
    CHECK(state.mode == VimMode::Ex && state.context == VimContext::CommandLine &&
          pending() == ":");
    status = BuildKeyboardStatus(state, ready);
    CHECK(status.label == "EX" && status.detail.find("Ex command") != std::string::npos);
    key(U'v');
    key(U'i');
    hint = FindBinding(state, {U's'}, 0);
    CHECK(pending() == ":vi" && hint != NO_HINT && VimBindings()[hint].chord == ":visual");
    enter();
    CHECK(state.mode == VimMode::Normal && state.context == VimContext::Editor &&
          pending().empty() && state.confidence == Confidence::Inferred);
    baseline();
    key(U'g');
    key(U'Q');
    text(U"w");
    enter();
    CHECK(state.mode == VimMode::Ex && state.context == VimContext::CommandLine &&
          pending() == ":" && state.confidence == Confidence::Inferred);
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Ex && state.context == VimContext::CommandLine &&
          pending() == ":");
    baseline();
    key(U'g');
    key(U'Q');
    CHECK(state.mode == VimMode::Ex && state.context == VimContext::CommandLine);
    text(U"visual");
    enter();
    CHECK(state.mode == VimMode::Normal && state.context == VimContext::Editor &&
          state.confidence == Confidence::Inferred);

    baseline();
    key(U'q');
    key(U':');
    CHECK(state.context == VimContext::CommandWindow && state.mode == VimMode::Normal &&
          state.waiting == WaitingKind::None && pending().empty());
    status = BuildKeyboardStatus(state, ready);
    CHECK(status.label == "NORMAL" &&
          status.detail.find("command-line window") != std::string::npos);
    key(U'i');
    CHECK(state.context == VimContext::CommandWindow && state.mode == VimMode::Insert);
    TrackInput(state, Esc(1));
    CHECK(state.context == VimContext::CommandWindow && state.mode == VimMode::Normal);
    key(U'c', Platform::CONTROL);
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal);
    baseline();
    key(U'q');
    key(U'/');
    CHECK(state.context == VimContext::CommandWindow && state.mode == VimMode::Normal);
    baseline();
    key(U'q');
    key(U'?');
    CHECK(state.context == VimContext::CommandWindow && state.mode == VimMode::Normal);
    baseline();
    key(U'q');
    key(U':');
    key(U':');
    key(U'q');
    CHECK(state.context == VimContext::CommandLine && pending() == ":q");
    enter();
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal &&
          state.confidence == Confidence::Inferred && pending().empty());
    baseline();
    key(U':');
    key(U'w');
    key(U'f', Platform::CONTROL);
    CHECK(state.context == VimContext::CommandWindow && state.mode == VimMode::Normal &&
          pending().empty());
    key(U'c', Platform::CONTROL);
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal);
    baseline();
    key(U'q');
    key(U':');
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    CHECK(state.confidence == Confidence::Uncertain);

    baseline();
    key(U'g');
    hint = FindBinding(state, {U'h'}, 0);
    CHECK(hint != NO_HINT && VimBindings()[hint].chord == "gh");
    hint = FindBinding(state, {U'H'}, Platform::SHIFT);
    CHECK(hint != NO_HINT && VimBindings()[hint].chord == "gH");
    hint = FindBinding(state, {U'h'}, Platform::CONTROL);
    CHECK(hint != NO_HINT && VimBindings()[hint].chord == "g Ctrl-h");
    baseline();
    key(U'v');
    hint = FindBinding(state, {U'g'}, Platform::CONTROL);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "switch to Select mode");
    key(U'g', Platform::CONTROL);
    hint = FindBinding(state, {U'g'}, Platform::CONTROL);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "switch to Visual mode");

    baseline();
    text(U":terminal");
    enter();
    CHECK(state.mode == VimMode::Terminal && state.context == VimContext::Editor &&
          state.confidence == Confidence::Inferred && pending().empty());
    baseline();
    text(U":term");
    enter();
    CHECK(state.mode == VimMode::Terminal && state.confidence == Confidence::Inferred);
    baseline();
    text(U":ter");
    enter();
    CHECK(state.mode == VimMode::Terminal);

    baseline();
    text(U":term");
    enter();
    key(U'i');
    key(U'v');
    key(U'g', Platform::CONTROL);
    CHECK(state.mode == VimMode::Terminal && pending().empty() &&
          state.confidence == Confidence::Inferred);
    CHECK(FindBinding(state, {U'i'}, 0) == NO_HINT && FindBinding(state, {U'v'}, 0) == NO_HINT);
    CHECK(FindBinding(state, {0, Platform::SpecialKey::Escape}, 0) == NO_HINT);
    TrackInput(state, Esc(90));
    TrackInput(state, Esc(90.1, true));
    TrackInput(state, Esc(90.2));
    CHECK(state.mode == VimMode::Terminal && state.confidence == Confidence::Inferred);
    key(U'[', Platform::CONTROL);
    key(U'c', Platform::CONTROL);
    CHECK(state.mode == VimMode::Terminal && state.confidence == Confidence::Inferred);
    hint = FindBinding(state, {U'\\'}, Platform::CONTROL);
    CHECK(hint != NO_HINT && VimBindings()[hint].chord == "Ctrl-\\ Ctrl-n");

    key(U'\\', Platform::CONTROL);
    CHECK(state.mode == VimMode::Terminal && state.waiting == WaitingKind::TerminalPrefix &&
          pending() == "Ctrl-\\");
    CHECK(std::string_view(WaitingName(state)) == "terminal prefix");
    key(U'n', Platform::CONTROL);
    CHECK(state.mode == VimMode::Normal && state.waiting == WaitingKind::None &&
          state.confidence == Confidence::Inferred && pending().empty());
    baseline();
    text(U":term");
    enter();
    key(U'\\', Platform::CONTROL);
    key(U'x');
    CHECK(state.mode == VimMode::Terminal && state.waiting == WaitingKind::None &&
          pending().empty());
    key(U'\\', Platform::CONTROL);
    TrackInput(state, Esc(100));
    CHECK(state.mode == VimMode::Terminal && state.waiting == WaitingKind::None);
    key(U'n', Platform::CONTROL);
    CHECK(state.mode == VimMode::Terminal && state.confidence == Confidence::Inferred);
    baseline();
    text(U":term");
    enter();
    key(U'\\', Platform::CONTROL);
    key(U'o', Platform::CONTROL);
    CHECK(state.mode == VimMode::Terminal && state.confidence == Confidence::Uncertain);
    TrackInput(state, Esc(110));
    TrackInput(state, Esc(110.1, true));
    TrackInput(state, Esc(110.2));
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    key(U'd');
    key(U'v');
    CHECK(state.operatorKey == U'd' && state.waiting == WaitingKind::None && pending() == "dv" &&
          state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
    key(U'w');
    CHECK(!state.operatorKey && pending().empty() && state.confidence == Confidence::Inferred);
    baseline();
    key(U'd');
    hint = FindBinding(state, {U'V'}, Platform::SHIFT);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "force the motion linewise");
    key(U'V');
    CHECK(state.operatorKey == U'd' && pending() == "dV" && state.mode == VimMode::Normal);
    key(U'j');
    CHECK(!state.operatorKey && pending().empty());
    baseline();
    key(U'c');
    hint = FindBinding(state, {U'v'}, Platform::CONTROL);
    CHECK(hint != NO_HINT && VimBindings()[hint].fullHint == "force the motion blockwise");
    key(U'v', Platform::CONTROL);
    CHECK(state.operatorKey == U'c' && state.mode == VimMode::Normal);
    key(U'i');
    key(U'w');
    CHECK(state.mode == VimMode::Insert && !state.operatorKey);

    baseline();
    key(U'i');
    key(U'o', Platform::CONTROL);
    key(U'v');
    CHECK(state.mode == VimMode::Visual && state.returnMode == VimMode::Insert &&
          !state.temporaryNormal);
    key(U'l');
    CHECK(state.mode == VimMode::Visual && state.returnMode == VimMode::Insert);
    key(U'V');
    CHECK(state.mode == VimMode::VisualLine && state.returnMode == VimMode::Insert);
    key(U'o');
    CHECK(state.mode == VimMode::VisualLine);
    key(U'v');
    CHECK(state.mode == VimMode::Visual && state.returnMode == VimMode::Insert);
    key(U'v');
    CHECK(state.mode == VimMode::Insert && state.returnMode == VimMode::Normal);
    baseline();
    key(U'i');
    key(U'o', Platform::CONTROL);
    key(U'v');
    key(U'l');
    key(U'd');
    CHECK(state.mode == VimMode::Insert && !state.temporaryNormal &&
          state.returnMode == VimMode::Normal);
    baseline();
    key(U'i');
    key(U'o', Platform::CONTROL);
    key(U'v');
    TrackInput(state, Esc(1));
    CHECK(state.mode == VimMode::Insert && !state.temporaryNormal &&
          state.returnMode == VimMode::Normal);
    baseline();
    key(U'v');
    key(U'l');
    key(U'd');
    CHECK(state.mode == VimMode::Normal && state.returnMode == VimMode::Normal);

    state.appProfile = Platform::AppProfile::Zed;
    baseline();
    key(U'q');
    key(U':');
    CHECK(state.context == VimContext::Editor && state.recording);
    state.recording = false;
    baseline();
    key(U'd');
    key(U'V');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'g');
    key(U'r');
    key(U'x');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'g');
    key(U'Q');
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Uncertain);
    baseline();
    key(U'Q');
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Uncertain);

    // ================================================================================================
    // g prefix and operator extras
    // ================================================================================================
    {
        const auto zed = Platform::AppProfile::Zed;
        const auto neovim = Platform::AppProfile::Neovim;
        const auto typed = [&](Platform::AppProfile profile, std::u32string_view sequence)
        {
            state.appProfile = profile;
            ResetTrackedState(state, VimMode::Normal, "g prefix test");
            for (char32_t c : sequence)
            {
                key(c);
            }
        };
        const auto settled = [&](VimMode mode)
        {
            return state.mode == mode && state.confidence != Confidence::Uncertain &&
                   !state.operatorKey && state.waiting == WaitingKind::None;
        };

        for (const auto profile : {zed, neovim})
        {
            for (const auto sequence :
                 {U"guu", U"gugu", U"gUU", U"gUgU", U"g~~", U"g~g~", U"g??", U"g?g?", U"gqq",
                  U"gqgq", U"gww", U"gwgw", U"gcc", U"gcgc", U"gcw", U"guw", U"guiw"})
            {
                typed(profile, sequence);
                CHECK(settled(VimMode::Normal));
            }
            typed(profile, U"gc");
            CHECK(state.operatorKey == U'c' && state.operatorPrefix == U'g' && pending() == "gc");
            typed(profile, U"cw");
            CHECK(settled(VimMode::Insert));
            typed(profile, U"dd");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"cc");
            CHECK(settled(VimMode::Insert));
        }

        typed(zed, U"gv");
        CHECK(settled(VimMode::Visual));
        typed(zed, U"vl");
        TrackInput(state, Esc(1));
        key(U'g');
        key(U'v');
        CHECK(settled(VimMode::Visual));
        typed(neovim, U"Vj");
        TrackInput(state, Esc(1));
        key(U'g');
        key(U'v');
        CHECK(settled(VimMode::VisualLine));
        typed(zed, U"vd");
        key(U'g');
        key(U'v');
        CHECK(settled(VimMode::Visual));
        typed(neovim, U"v");
        key(U'v', Platform::CONTROL);
        TrackInput(state, Esc(1));
        key(U'g');
        key(U'v');
        CHECK(settled(VimMode::VisualBlock));

        for (const auto profile : {zed, neovim})
        {
            typed(profile, U"gi");
            CHECK(settled(VimMode::Insert));
            typed(profile, U"gJ");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vgc");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vg?");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vgw");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vgq");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vgu");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vc");
            CHECK(settled(VimMode::Insert));
            for (const auto sequence :
                 {U"g_", U"g0", U"g$", U"g^", U"gM", U"g*", U"g#", U"g,", U"g;"})
            {
                typed(profile, sequence);
                CHECK(settled(VimMode::Normal));
            }
            typed(profile, U"dg_");
            CHECK(settled(VimMode::Normal));
            typed(profile, U"vg$");
            CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
        }
        typed(neovim, U"gI");
        CHECK(settled(VimMode::Insert));
        typed(zed, U"gI");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"gA");
        CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
        typed(zed, U"vgR");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"vgh");
        CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
        typed(neovim, U"vgb");
        CHECK(state.confidence == Confidence::Uncertain);

        typed(neovim, U"g?");
        CHECK(state.operatorKey == U'?' && state.operatorPrefix == U'g');
        auto operator_hint = FindBinding(state, {U'?'}, Platform::SHIFT);
        CHECK(operator_hint != NO_HINT && VimBindings()[operator_hint].chord == "g??");
        typed(neovim, U"d?");
        CHECK(state.context == VimContext::Search);
        typed(zed, U"g?");
        operator_hint = FindBinding(state, {U'?'}, Platform::SHIFT);
        CHECK(operator_hint != NO_HINT && VimBindings()[operator_hint].chord == "g??");

        typed(neovim, U"g");
        operator_hint = FindBinding(state, {U'O'}, Platform::SHIFT);
        CHECK(operator_hint != NO_HINT && VimBindings()[operator_hint].chord == "gO" &&
              VimBindings()[operator_hint].fullHint.find("location list") !=
                  std::string_view::npos);
        typed(neovim, U"gO");
        CHECK(state.confidence == Confidence::Uncertain);

        for (const auto sequence : {U"gs", U"gS", U"gO", U"g.", U"g/"})
        {
            typed(zed, sequence);
            CHECK(state.context == VimContext::Palette && state.mode == VimMode::Normal);
            TrackInput(state, Esc(1));
            CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal);
        }
        for (const auto sequence : {U"gh", U"gB", U"gD", U"gy", U"gx", U"g[", U"g]"})
        {
            typed(zed, sequence);
            CHECK(settled(VimMode::Normal));
        }
        typed(neovim, U"gh");
        CHECK(state.mode == VimMode::Select);
        for (const auto sequence : {U"gf", U"g ", U"gt", U"gT", U"gA"})
        {
            typed(zed, sequence);
            CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
        }

        for (const auto sequence : {U"grn", U"grr", U"gri", U"gra"})
        {
            typed(zed, sequence);
            CHECK(settled(VimMode::Normal));
        }
        typed(zed, U"gr");
        CHECK(state.waiting == WaitingKind::Lsp);
        typed(zed, U"grt");
        CHECK(state.confidence == Confidence::Uncertain);
        typed(neovim, U"grt");
        CHECK(settled(VimMode::Normal));

        typed(zed, U"gR");
        CHECK(state.operatorKey == U'R' && state.operatorPrefix == U'g' &&
              state.mode == VimMode::Normal && state.confidence == Confidence::Inferred);
        for (const auto sequence : {U"gRr", U"gRR", U"gRw"})
        {
            typed(zed, sequence);
            CHECK(settled(VimMode::Normal));
        }
        typed(zed, U"gbc");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"gb");
        CHECK(state.operatorKey == U'b' && state.operatorPrefix == U'g');
        typed(neovim, U"gb");
        CHECK(state.confidence == Confidence::Uncertain);

        typed(zed, U"cs");
        CHECK(state.waiting == WaitingKind::SurroundTarget &&
              std::string_view(WaitingName(state)) == "surround target");
        typed(zed, U"cs\"");
        CHECK(state.waiting == WaitingKind::SurroundReplacement && pending() == "cs\"");
        typed(zed, U"cs\"'");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"ds\"");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"ds");
        CHECK(state.waiting == WaitingKind::SurroundTarget);
        typed(zed, U"ysiw\"");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"ysiw");
        CHECK(state.waiting == WaitingKind::SurroundReplacement);
        typed(zed, U"yss)");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"ysw'");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"ysf)\"");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"ys");
        CHECK(state.operatorKey == U's' && state.operatorPrefix == U'y' &&
              state.waiting == WaitingKind::None);
        typed(zed, U"cs");
        TrackInput(state, Esc(1));
        CHECK(settled(VimMode::Normal));
        typed(neovim, U"ds\"");
        CHECK(state.confidence == Confidence::Uncertain);
        typed(neovim, U"cs\"");
        CHECK(state.confidence == Confidence::Uncertain);

        typed(zed, U"cx");
        CHECK(state.operatorKey == U'x' && state.operatorPrefix == U'c' &&
              state.mode == VimMode::Normal);
        for (const auto sequence : {U"cxw", U"cxx", U"cxc"})
        {
            typed(zed, sequence);
            CHECK(settled(VimMode::Normal));
        }
        typed(neovim, U"cx");
        CHECK(state.confidence == Confidence::Uncertain);

        typed(zed, U"cd");
        CHECK(state.context == VimContext::Palette);
        typed(neovim, U"cd");
        CHECK(state.confidence == Confidence::Uncertain);

        typed(zed, U"dvw");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"yvw");
        CHECK(settled(VimMode::Normal));
        typed(zed, U"dv");
        CHECK(state.operatorKey == U'd' && state.mode == VimMode::Normal &&
              state.confidence == Confidence::Inferred);
        typed(zed, U"dV");
        CHECK(state.confidence == Confidence::Uncertain);

        for (const auto sequence : {U"do", U"dO", U"dp", U"du", U"dU"})
        {
            typed(zed, sequence);
            CHECK(settled(VimMode::Normal));
        }
        typed(neovim, U"do");
        CHECK(state.confidence == Confidence::Uncertain);
    }
    CheckPlainMotionsAndKeys();
    // ====================================================================================================
    // Harmless Cmd shortcuts and Insert/Replace Ctrl keys
    // ====================================================================================================
    double now = 100;
    const auto press_escape = [&]
    {
        now += 10;
        TrackInput(state, Esc(now));
        TrackInput(state, Esc(now + 1, true));
    };
    const auto enter_mode = [&](VimMode mode)
    {
        baseline();
        if (mode == VimMode::Visual)
        {
            key(U'v');
        }
        else if (mode == VimMode::VisualLine)
        {
            key(U'V');
        }
        else if (mode == VimMode::Insert)
        {
            key(U'i');
        }
        else if (mode == VimMode::Replace)
        {
            key(U'R');
        }
    };
    const auto tracked_in = [&](VimMode mode)
    {
        return state.mode == mode && state.confidence != Confidence::Uncertain;
    };
    const auto full_hint = [&](char32_t character, std::uint8_t modifiers)
    {
        const auto id = FindBinding(state, {character}, modifiers);
        return id == NO_HINT ? std::string_view{} : VimBindings()[id].fullHint;
    };
    const auto cmd = Platform::COMMAND;
    const auto ctrl = Platform::CONTROL;

    state.appProfile = Platform::AppProfile::Zed;
    for (const auto mode : {VimMode::Normal, VimMode::Visual, VimMode::Insert, VimMode::Replace})
    {
        enter_mode(mode);
        key(U's', cmd);
        CHECK(tracked_in(mode));
        enter_mode(mode);
        key(U's', cmd | Platform::OPTION);
        CHECK(tracked_in(mode));
        enter_mode(mode);
        key(U'c', cmd);
        CHECK(tracked_in(mode));
        enter_mode(mode);
        key(U'/', cmd);
        CHECK(tracked_in(mode));
    }
    for (const auto mode : {VimMode::Normal, VimMode::Insert, VimMode::Replace})
    {
        enter_mode(mode);
        key(U'z', cmd);
        CHECK(tracked_in(mode));
        enter_mode(mode);
        key(U'Z', cmd);
        CHECK(tracked_in(mode));
        enter_mode(mode);
        key(U'x', cmd);
        CHECK(tracked_in(mode));
        enter_mode(mode);
        key(U'v', cmd);
        CHECK(tracked_in(mode));
    }
    enter_mode(VimMode::Visual);
    key(U'z', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Visual);
    key(U'Z', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Visual);
    key(U'x', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Visual);
    key(U'v', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Normal);
    key(U'd');
    key(U's', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Normal);
    key(U'd');
    key(U'v', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Normal);
    key(U'g');
    key(U's', cmd);
    CHECK(state.confidence == Confidence::Uncertain);

    enter_mode(VimMode::Normal);
    key(U'a', cmd);
    CHECK(tracked_in(VimMode::Visual));
    enter_mode(VimMode::VisualLine);
    key(U'a', cmd);
    CHECK(tracked_in(VimMode::Visual));
    enter_mode(VimMode::Visual);
    key(U'a', cmd);
    CHECK(tracked_in(VimMode::Visual));
    enter_mode(VimMode::Insert);
    key(U'a', cmd);
    CHECK(tracked_in(VimMode::Insert));
    enter_mode(VimMode::Replace);
    key(U'a', cmd);
    CHECK(state.confidence == Confidence::Uncertain);

    enter_mode(VimMode::Normal);
    key(U'f', cmd);
    CHECK(state.context == VimContext::Search && state.confidence != Confidence::Uncertain);
    TrackInput(state, {.value = {0, Platform::SpecialKey::Enter}});
    CHECK(state.context == VimContext::Editor && tracked_in(VimMode::Normal));
    enter_mode(VimMode::Visual);
    key(U'f', cmd);
    CHECK(state.context == VimContext::Search);
    press_escape();
    CHECK(state.context == VimContext::Editor && tracked_in(VimMode::Visual));
    enter_mode(VimMode::Insert);
    key(U'f', cmd);
    CHECK(state.confidence == Confidence::Uncertain);

    enter_mode(VimMode::Normal);
    key(U'p', cmd);
    CHECK(state.context == VimContext::Palette && state.confidence != Confidence::Uncertain);
    press_escape();
    CHECK(state.context == VimContext::Editor && tracked_in(VimMode::Normal));
    enter_mode(VimMode::Visual);
    key(U'P', cmd);
    CHECK(state.context == VimContext::Palette && state.confidence != Confidence::Uncertain);
    press_escape();
    CHECK(state.context == VimContext::Editor && tracked_in(VimMode::Visual));
    enter_mode(VimMode::Insert);
    key(U'p', cmd);
    CHECK(state.confidence == Confidence::Uncertain);

    enter_mode(VimMode::Normal);
    key(U'd');
    key(U'w', cmd);
    CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
          state.confidence == Confidence::Assumed);
    enter_mode(VimMode::Visual);
    key(U'w', cmd);
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    enter_mode(VimMode::Insert);
    key(U'w', cmd);
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);

    enter_mode(VimMode::Normal);
    CHECK(full_hint(U's', cmd) == "save file");
    CHECK(full_hint(U's', cmd | Platform::OPTION) == "save all files");
    CHECK(full_hint(U'Z', cmd | Platform::SHIFT) == "redo");
    CHECK(full_hint(U'P', cmd | Platform::SHIFT) == "command palette");
    CHECK(full_hint(U'w', cmd) == "close tab; assume Normal");
    CHECK(full_hint(U'v', cmd) == "paste");
    enter_mode(VimMode::Visual);
    CHECK(full_hint(U'c', cmd) == "copy selection");
    CHECK(full_hint(U'v', cmd).empty());
    enter_mode(VimMode::Insert);
    CHECK(full_hint(U'z', cmd) == "undo");
    CHECK(full_hint(U'a', cmd) == "select all; stays Insert");
    enter_mode(VimMode::Normal);
    key(U'd');
    CHECK(full_hint(U's', cmd).empty());

    for (const char32_t character : std::u32string_view(U"ntdyeafsl"))
    {
        enter_mode(VimMode::Insert);
        key(character, ctrl);
        CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    }
    enter_mode(VimMode::Insert);
    CHECK(full_hint(U't', ctrl) == "increase indent");
    CHECK(full_hint(U'n', ctrl) == "next word completion");

    enter_mode(VimMode::Insert);
    key(U'q', ctrl);
    CHECK(state.waiting == WaitingKind::Literal);
    key(U'x');
    CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Insert);
    key(U'q', ctrl);
    press_escape();
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None);

    enter_mode(VimMode::Insert);
    key(U'k', ctrl);
    CHECK(state.waiting == WaitingKind::DigraphFirst);
    CHECK(std::string_view(WaitingName(state)) == "digraph");
    key(U'e');
    CHECK(state.waiting == WaitingKind::DigraphSecond);
    key(U':');
    CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Insert);
    key(U'k', ctrl);
    press_escape();
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Insert);
    key(U'k', ctrl);
    key(U'e');
    press_escape();
    CHECK(state.mode == VimMode::Insert && state.waiting == WaitingKind::None);

    enter_mode(VimMode::Replace);
    key(U'q', ctrl);
    CHECK(state.waiting == WaitingKind::Literal);
    key(U'x');
    CHECK(tracked_in(VimMode::Replace) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Replace);
    key(U'k', ctrl);
    key(U'a');
    key(U'b');
    CHECK(tracked_in(VimMode::Replace) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Replace);
    key(U'n', ctrl);
    CHECK(state.confidence == Confidence::Uncertain);

    for (const char32_t character : std::u32string_view(U"oclzey"))
    {
        enter_mode(VimMode::Insert);
        key(U'x', ctrl);
        CHECK(state.waiting == WaitingKind::CtrlX && state.mode == VimMode::Insert);
        CHECK(std::string_view(WaitingName(state)) == "Ctrl-x command");
        key(character, ctrl);
        CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    }
    enter_mode(VimMode::Insert);
    key(U'x', ctrl);
    key(U'a', ctrl);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Insert);
    key(U'x', ctrl);
    key(U'q', ctrl);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Insert);
    key(U'x', ctrl);
    key(U'a');
    CHECK(state.confidence == Confidence::Uncertain);

    state.appProfile = Platform::AppProfile::Neovim;
    for (const char32_t character : std::u32string_view(U"nps"))
    {
        enter_mode(VimMode::Insert);
        key(character, ctrl);
        CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    }
    enter_mode(VimMode::Replace);
    key(U'n', ctrl);
    CHECK(tracked_in(VimMode::Replace) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Insert);
    key(U'k', ctrl);
    key(U'e');
    key(U':');
    CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Replace);
    key(U'k', ctrl);
    key(U'e');
    key(U':');
    CHECK(tracked_in(VimMode::Replace) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Insert);
    key(U'q', ctrl);
    CHECK(state.waiting == WaitingKind::Literal);
    key(U'x');
    CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    for (const char32_t character : std::u32string_view(U"oclaeynwsz"))
    {
        enter_mode(VimMode::Insert);
        key(U'x', ctrl);
        key(character, ctrl);
        CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    }
    enter_mode(VimMode::Replace);
    key(U'x', ctrl);
    key(U'o', ctrl);
    CHECK(tracked_in(VimMode::Replace) && state.waiting == WaitingKind::None);
    // A plain key ends Neovim's CTRL-X mode and is inserted.
    enter_mode(VimMode::Insert);
    key(U'x', ctrl);
    key(U'a');
    CHECK(tracked_in(VimMode::Insert) && state.waiting == WaitingKind::None);
    enter_mode(VimMode::Insert);
    key(U'x', ctrl);
    key(U'[', ctrl);
    CHECK(state.mode == VimMode::Normal);

    for (const char32_t character : std::u32string_view(U"sxvfp"))
    {
        enter_mode(VimMode::Normal);
        key(character, cmd);
        CHECK(state.confidence == Confidence::Uncertain);
    }
    enter_mode(VimMode::Insert);
    key(U'z', cmd);
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    enter_mode(VimMode::Normal);
    key(U'P', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Normal);
    key(U'/', cmd);
    CHECK(state.confidence == Confidence::Uncertain);
    enter_mode(VimMode::Normal);
    key(U'c', cmd);
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    enter_mode(VimMode::Visual);
    key(U'a', cmd);
    CHECK(tracked_in(VimMode::Visual));
    enter_mode(VimMode::Normal);
    key(U'w', cmd);
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    state.appProfile = Platform::AppProfile::Zed;

    // ====================================================================================================
    // z, Z, Ctrl-w and [ / ] prefixes
    // ====================================================================================================
    const auto special = [&](Platform::SpecialKey value, std::uint8_t mods = 0)
    {
        TrackInput(state, {.value = {0, value}, .modifiers = mods});
    };
    const auto useProfile = [&](Platform::AppProfile app)
    {
        state.appProfile = app;
        baseline();
    };
    const auto completed = [&](VimMode mode)
    {
        return state.mode == mode && state.waiting == WaitingKind::None && !state.operatorKey &&
               state.confidence == Confidence::Inferred;
    };
    const auto assumed = [&](VimMode mode)
    {
        return state.mode == mode && state.waiting == WaitingKind::None && !state.operatorKey &&
               state.confidence == Confidence::Assumed;
    };
    const auto bindingFullHint = [&](char32_t c, std::uint8_t mods)
    {
        const auto hint = FindBinding(state, {c}, mods);
        return hint == NO_HINT ? std::string_view{} : VimBindings()[hint].fullHint;
    };
    const auto zedProfile = Platform::AppProfile::Zed;
    const auto neovimProfile = Platform::AppProfile::Neovim;

    // z folds, horizontal scroll and line positioning keep Normal and Visual modes.
    for (const auto app : {zedProfile, neovimProfile})
    {
        useProfile(app);
        for (char32_t c : std::u32string_view(U"aAcCoOMRhlHLztb-.^+"))
        {
            key(U'z');
            key(c);
            CHECK(completed(VimMode::Normal));
            baseline();
            key(U'v');
            key(U'z');
            key(c);
            CHECK(completed(VimMode::Visual));
            baseline();
        }
        key(U'z');
        special(Platform::SpecialKey::Enter);
        CHECK(completed(VimMode::Normal));
        baseline();
        key(U'v');
        key(U'z');
        special(Platform::SpecialKey::Enter);
        CHECK(completed(VimMode::Visual));
    }

    // zf folds a Zed selection, while Neovim waits for a motion as a fold operator.
    useProfile(zedProfile);
    key(U'z');
    key(U'f');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U'z');
    key(U'f');
    CHECK(completed(VimMode::Normal));
    useProfile(neovimProfile);
    key(U'z');
    CHECK(bindingFullHint(U'f', 0).find("needs a motion") != std::string_view::npos);
    key(U'f');
    CHECK(state.operatorKey == U'f' && state.waiting == WaitingKind::None &&
          state.mode == VimMode::Normal);
    key(U'j');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U'z');
    key(U'f');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'd');
    key(U'z');
    CHECK(state.confidence == Confidence::Uncertain);

    // ZZ and ZQ close the file: Zed assumes Normal, Neovim may quit and must resync.
    useProfile(zedProfile);
    key(U'Z');
    CHECK(state.waiting == WaitingKind::UpperZ &&
          std::string_view(WaitingName(state)) == "Z command");
    key(U'Z');
    CHECK(assumed(VimMode::Normal));
    baseline();
    key(U'Z');
    key(U'Q');
    CHECK(assumed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U'Z');
    key(U'Z');
    CHECK(assumed(VimMode::Normal));
    baseline();
    key(U'Z');
    key(U'j');
    CHECK(state.confidence == Confidence::Uncertain);
    useProfile(neovimProfile);
    key(U'Z');
    CHECK(state.waiting == WaitingKind::UpperZ);
    CHECK(bindingFullHint(U'Z', Platform::SHIFT).find("may quit") != std::string_view::npos);
    key(U'Z');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'Z');
    key(U'Q');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'v');
    key(U'Z');
    CHECK(state.confidence == Confidence::Uncertain);

    // Ctrl-w: Zed navigation assumes Normal; Neovim window commands complete the command.
    useProfile(zedProfile);
    for (char32_t c : std::u32string_view(U"hjklwWpvscqnad]D "))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c);
        CHECK(assumed(VimMode::Normal));
    }
    for (char32_t c : std::u32string_view(U"hjklwpvscqna]W"))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c, Platform::CONTROL);
        CHECK(assumed(VimMode::Normal));
    }
    for (const auto arrow : {Platform::SpecialKey::Left, Platform::SpecialKey::Right,
                             Platform::SpecialKey::Up, Platform::SpecialKey::Down})
    {
        baseline();
        key(U'w', Platform::CONTROL);
        special(arrow);
        CHECK(assumed(VimMode::Normal));
    }
    for (char32_t c : std::u32string_view(U"oxHJKL+-<>=_"))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c);
        CHECK(completed(VimMode::Normal));
    }
    baseline();
    key(U'w', Platform::CONTROL);
    special(Platform::SpecialKey::Left, Platform::SHIFT);
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'c', Platform::CONTROL);
    CHECK(state.mode == VimMode::Normal && state.waiting == WaitingKind::None &&
          state.confidence == Confidence::Assumed);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'g');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'd', Platform::CONTROL);
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'|');
    CHECK(state.confidence == Confidence::Uncertain);

    useProfile(neovimProfile);
    for (char32_t c : std::u32string_view(U"hjklwWpvsScn"))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c);
        CHECK(completed(VimMode::Normal));
    }
    // CTRL-W q quits Neovim from the last window, so it needs a resync.
    for (const std::uint8_t mods : {std::uint8_t{0}, Platform::CONTROL})
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(U'q', mods);
        CHECK(state.confidence == Confidence::Uncertain);
    }
    baseline();
    key(U'w', Platform::CONTROL);
    key(U' ');
    CHECK(state.confidence == Confidence::Uncertain);
    for (char32_t c : std::u32string_view(U"hjklwpvsn"))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c, Platform::CONTROL);
        CHECK(completed(VimMode::Normal));
    }
    for (char32_t c : std::u32string_view(U"oxHJKL+-<>=_rRT|d"))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c);
        CHECK(completed(VimMode::Normal));
    }
    for (char32_t c : std::u32string_view(U"oxr"))
    {
        baseline();
        key(U'w', Platform::CONTROL);
        key(c, Platform::CONTROL);
        CHECK(completed(VimMode::Normal));
    }
    for (const auto arrow : {Platform::SpecialKey::Left, Platform::SpecialKey::Right,
                             Platform::SpecialKey::Up, Platform::SpecialKey::Down})
    {
        baseline();
        key(U'w', Platform::CONTROL);
        special(arrow);
        CHECK(completed(VimMode::Normal));
    }
    baseline();
    key(U'w', Platform::CONTROL);
    special(Platform::SpecialKey::Left, Platform::SHIFT);
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U']');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'a', Platform::CONTROL);
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'D');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'g');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'w', Platform::CONTROL);
    key(U'c', Platform::CONTROL);
    CHECK(state.mode == VimMode::Normal && state.waiting == WaitingKind::None &&
          state.pendingLength == 0 && state.confidence == Confidence::Inferred);

    // Bracket motions depend on Zed's Normal/Visual/operator contexts and Neovim's Normal defaults.
    useProfile(zedProfile);
    key(U'[');
    key(U'd');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U']');
    key(U'd');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U']');
    key(U'd');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'd');
    key(U']');
    key(U'd');
    CHECK(state.confidence == Confidence::Uncertain);
    for (const auto open : {true, false})
    {
        baseline();
        key(open ? U'[' : U']');
        key(U'c');
        CHECK(completed(VimMode::Normal));
        baseline();
        key(U'v');
        key(open ? U'[' : U']');
        key(U'e');
        CHECK(state.confidence == Confidence::Uncertain);
        baseline();
        key(open ? U'[' : U']');
        key(U' ');
        CHECK(completed(VimMode::Normal));
        baseline();
        key(U'v');
        key(open ? U'[' : U']');
        key(U' ');
        CHECK(state.confidence == Confidence::Uncertain);
    }
    baseline();
    key(U'[');
    key(U'e');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U']');
    key(U'e');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U'[');
    key(U'r');
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
    baseline();
    key(U'd');
    key(U']');
    key(U'r');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'[');
    key(U'x');
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
    key(U']');
    key(U'x');
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
    baseline();
    key(U'd');
    key(U'[');
    key(U'x');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U'[');
    key(U'n');
    CHECK(state.confidence == Confidence::Uncertain);
    useProfile(neovimProfile);
    key(U'[');
    key(U'd');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U']');
    key(U'D');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'[');
    key(U'c');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U'e');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U'x');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U'r');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U' ');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U']');
    key(U' ');
    CHECK(completed(VimMode::Normal));
    baseline();
    key(U'v');
    key(U']');
    key(U'n');
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
    baseline();
    key(U'v');
    key(U'[');
    key(U'n');
    CHECK(state.mode == VimMode::Visual && state.confidence == Confidence::Inferred);
    baseline();
    key(U']');
    key(U'n');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U'n');
    CHECK(state.confidence == Confidence::Uncertain);
    for (const auto open : {true, false})
    {
        for (char32_t c : std::u32string_view(U"qQlLtTaA"))
        {
            baseline();
            key(open ? U'[' : U']');
            key(c);
            CHECK(completed(VimMode::Normal));
            baseline();
            key(U'v');
            key(open ? U'[' : U']');
            key(c);
            CHECK(state.confidence == Confidence::Uncertain);
        }
        for (char32_t c : std::u32string_view(U"qlt"))
        {
            baseline();
            key(open ? U'[' : U']');
            key(c, Platform::CONTROL);
            CHECK(completed(VimMode::Normal));
        }
    }
    baseline();
    key(U'd');
    key(U'[');
    key(U'q');
    CHECK(state.confidence == Confidence::Uncertain);
    useProfile(zedProfile);
    key(U'[');
    key(U'q');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U'D');
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(U'[');
    key(U'n');
    CHECK(state.confidence == Confidence::Uncertain);
    std::cout << "Tracker grammar and recovery checks passed\n";
}
