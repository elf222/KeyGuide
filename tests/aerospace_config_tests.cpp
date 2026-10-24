// tests/aerospace_config_tests.cpp
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "test_helpers.hpp"
#include "keymaps/aerospace_config.hpp"

namespace
{
    using K = Platform::KeyId;

    constexpr std::string_view sample = R"TOML(
config-version = 2
key-mapping.preset = 'qwerty'
on-window-detected = [
    { if = 'test %{app-bundle-id} = com.hnc.Discord', run = 'move-node-to-workspace 10' },
    { if = 'test %{app-bundle-id} = com.apple.finder', run = 'move-node-to-workspace D' },
]

# 'main' binding mode declaration
[mode.main.binding]

    # See: https://nikitabobko.github.io/AeroSpace/commands#layout
    alt-slash = 'layout tiles horizontal vertical'
    alt-h = 'focus left'
    alt-shift-l = 'move right'
    alt-minus = 'resize smart -25'
    alt-0 = 'workspace 10'
    alt-f = 'fullscreen'
    alt-f13 = 'fullscreen'
    alt-tab = 'workspace-back-and-forth'
    alt-shift-tab = 'move-workspace-to-monitor --wrap-around next'
    alt-shift-semicolon = 'mode service'
    alt-enter = ['exec-and-forget open -a Terminal', 'focus left']

# 'service' binding mode declaration.
[mode.service.binding]
    esc = ['reload-config', 'mode main'] # reload
    r = ['flatten-workspace-tree', 'mode main'] # reset layout
    alt-shift-h = ['join-with left', 'mode main']
)TOML";

    // Keeps the main-mode bindings of `sample` (10 valid, alt-f13 skipped) plus the 3 service ones.
    constexpr std::size_t sample_binding_count = 13;

    const AeroSpaceConfigBinding* Find(const AeroSpaceConfig& config, K key, std::uint8_t modifiers,
                                       bool service = false)
    {
        for (const auto& binding : config.bindings)
        {
            if (binding.key == key && binding.modifiers == modifiers && binding.service == service)
            {
                return &binding;
            }
        }
        return nullptr;
    }

    bool HasWarning(const AeroSpaceConfig& config, std::string_view part)
    {
        return std::any_of(config.warnings.begin(), config.warnings.end(),
                           [&](const std::string& warning)
                           { return warning.find(part) != std::string::npos; });
    }

    AeroSpaceConfig ParseOrDie(std::string_view text)
    {
        std::string error;
        const auto config = ParseAeroSpaceConfig(text, &error);
        CHECK(config.has_value());
        CHECK(error.empty());
        return config.value_or(AeroSpaceConfig{});
    }
} // namespace

int main()
{
    const auto config = ParseOrDie(sample);

    {
        const auto* binding = Find(config, K::H, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "left");
        CHECK(binding->fullHint == "focus the window left");
        CHECK(!binding->service);
        CHECK(!binding->entersService);
        CHECK(!binding->returnsToMain);
        CHECK(binding->command == "focus left");
    }
    {
        const auto* binding = Find(config, K::L, Platform::OPTION | Platform::SHIFT);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "move");
        CHECK(binding->fullHint == "move the window right");
    }
    {
        const auto* binding = Find(config, K::Minus, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "shrink");
        CHECK(binding->fullHint == "resize smart -25");
    }
    {
        const auto* binding = Find(config, K::Zero, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "ws 10");
        CHECK(binding->fullHint == "switch to workspace 10");
    }
    {
        const auto* binding = Find(config, K::F, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "fullscreen");
        CHECK(binding->fullHint == "toggle fullscreen");
    }
    {
        const auto* binding = Find(config, K::Tab, Platform::OPTION | Platform::SHIFT);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "monitor");
        CHECK(binding->fullHint == "move the workspace to the next monitor");
    }
    {
        const auto* binding = Find(config, K::Tab, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "back");
    }
    {
        const auto* binding = Find(config, K::Slash, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "tiles");
        CHECK(binding->fullHint == "tiles layout; toggle horizontal / vertical");
    }
    {
        const auto* binding = Find(config, K::Semicolon, Platform::OPTION | Platform::SHIFT);
        CHECK(binding != nullptr);
        CHECK(binding->entersService);
        CHECK(!binding->returnsToMain);
        CHECK(binding->shortHint == "service");
        CHECK(binding->fullHint == "enter service mode");
        CHECK(binding->command == "mode service");
    }
    {
        // Array values: several commands, mode entries kept in `command` only.
        const auto* binding = Find(config, K::Enter, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "run");
        CHECK(binding->fullHint == "run: open -a Terminal + more");
        CHECK(binding->command == "exec-and-forget open -a Terminal; focus left");
    }
    {
        const auto* binding = Find(config, K::Escape, 0, true);
        CHECK(binding != nullptr);
        CHECK(binding->service);
        CHECK(binding->returnsToMain);
        CHECK(!binding->entersService);
        CHECK(binding->shortHint == "reload");
        CHECK(binding->fullHint.find("reload config") != std::string::npos);
        CHECK(binding->fullHint.find("back to main mode") != std::string::npos);
        CHECK(binding->command == "reload-config; mode main");
    }
    {
        const auto* binding = Find(config, K::R, 0, true);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "reset");
        CHECK(binding->fullHint == "reset layout (flatten workspace tree); back to main mode");
    }
    {
        const auto* binding = Find(config, K::H, Platform::OPTION | Platform::SHIFT, true);
        CHECK(binding != nullptr);
        CHECK(binding->shortHint == "join");
        CHECK(binding->fullHint == "join with the window left; back to main mode");
    }
    CHECK(config.bindings.size() == sample_binding_count);
    CHECK(HasWarning(config, "alt-f13"));
    CHECK(config.warnings.size() == 1);

    // Invalid TOML reports an error instead of throwing.
    {
        std::string error;
        const auto broken =
            ParseAeroSpaceConfig("[mode.main.binding\nalt-h = 'focus left'", &error);
        CHECK(!broken.has_value());
        CHECK(!error.empty());
    }
    // Other modes are skipped with a warning naming the mode.
    {
        const auto third = ParseOrDie(R"TOML(
[mode.main.binding]
    alt-h = 'focus left'
[mode.resize.binding]
    h = 'resize width -50'
)TOML");
        CHECK(third.bindings.size() == 1);
        CHECK(HasWarning(third, "resize"));
    }
    // Non-qwerty presets still parse but warn.
    {
        const auto dvorak = ParseOrDie(R"TOML(
key-mapping.preset = 'dvorak'
[mode.main.binding]
    alt-h = 'focus left'
)TOML");
        CHECK(dvorak.bindings.size() == 1);
        CHECK(HasWarning(dvorak, "qwerty"));
    }
    // Non-string, non-array values are skipped.
    {
        const auto bad_value = ParseOrDie(R"TOML(
[mode.main.binding]
    alt-h = 42
    alt-j = 'focus down'
)TOML");
        CHECK(bad_value.bindings.size() == 1);
        CHECK(HasWarning(bad_value, "alt-h"));
    }

    // Key notation.
    {
        Platform::KeyId key = K::Unknown;
        std::uint8_t modifiers = 0;
        CHECK(ParseAeroSpaceKey("cmd-ctrl-leftSquareBracket", key, modifiers));
        CHECK(key == K::LeftBracket);
        CHECK(modifiers == (Platform::COMMAND | Platform::CONTROL));
        CHECK(ParseAeroSpaceKey("alt-shift-h", key, modifiers));
        CHECK(key == K::H);
        CHECK(modifiers == (Platform::OPTION | Platform::SHIFT));
        CHECK(ParseAeroSpaceKey("keypad7", key, modifiers));
        CHECK(key == K::NumpadSeven);
        CHECK(ParseAeroSpaceKey("f12", key, modifiers));
        CHECK(key == K::F12);
        CHECK(!ParseAeroSpaceKey("alt-f13", key, modifiers));
        CHECK(!ParseAeroSpaceKey("hyper-h", key, modifiers));
        CHECK(!ParseAeroSpaceKey("alt-", key, modifiers));
        CHECK(!ParseAeroSpaceKey("alt--h", key, modifiers));
        CHECK(!ParseAeroSpaceKey("alt-H", key, modifiers));
    }

    // Mode switches: options before the target, reordered lists, `;` in one string, other modes.
    {
        const auto modes = ParseOrDie(R"TOML(
[mode.main.binding]
    alt-s = 'mode -- service'
    x = ['mode main', 'mode service']
    y = ['mode service', 'mode main']
    z = 'mode resize'
[mode.service.binding]
    alt-shift-s = ['reload-config; mode main']
)TOML");
        const auto* enter = Find(modes, K::S, Platform::OPTION);
        CHECK(enter != nullptr);
        CHECK(enter->entersService);
        CHECK(!enter->returnsToMain);
        CHECK(enter->shortHint == "service");
        const auto* last_service = Find(modes, K::X, 0);
        CHECK(last_service != nullptr);
        CHECK(last_service->entersService);
        const auto* last_main = Find(modes, K::Y, 0);
        CHECK(last_main != nullptr);
        CHECK(!last_main->entersService);
        CHECK(!last_main->returnsToMain);
        const auto* other = Find(modes, K::Z, 0);
        CHECK(other != nullptr);
        CHECK(!other->entersService);
        CHECK(!other->returnsToMain);
        CHECK(HasWarning(modes, "'z' in mode 'main'"));
        CHECK(HasWarning(modes, "resize"));
        const auto* back = Find(modes, K::S, Platform::OPTION | Platform::SHIFT, true);
        CHECK(back != nullptr);
        CHECK(back->returnsToMain);
        CHECK(!back->entersService);
        CHECK(back->shortHint == "reload");
        CHECK(back->fullHint == "reload config; back to main mode");
        CHECK(back->command == "reload-config; mode main");
    }
    // exec-and-forget keeps its whole shell line, including ';', as a single command.
    {
        const auto shell = ParseOrDie(R"TOML(
[mode.main.binding]
    alt-e = 'exec-and-forget echo a; echo b'
    alt-f = ['exec-and-forget echo a; mode service']
)TOML");
        const auto* binding = Find(shell, K::E, Platform::OPTION);
        CHECK(binding != nullptr);
        CHECK(binding->command == "exec-and-forget echo a; echo b");
        CHECK(binding->shortHint == "run");
        CHECK(binding->fullHint == "run: echo a; echo b");
        const auto* swallowed = Find(shell, K::F, Platform::OPTION);
        CHECK(swallowed != nullptr);
        CHECK(!swallowed->entersService);
        CHECK(swallowed->fullHint == "run: echo a; mode service");
    }

    // Hint text rules.
    {
        std::string short_hint;
        std::string full_hint;
        DescribeAeroSpaceCommands({"exec-and-forget " + std::string(80, 'x')}, false, short_hint,
                                  full_hint);
        CHECK(short_hint == "run");
        CHECK(full_hint.size() == 60);
        CHECK(full_hint.ends_with(".."));
        DescribeAeroSpaceCommands({"move-workspace-to-monitor --wrap-around prev"}, false,
                                  short_hint, full_hint);
        CHECK(full_hint == "move the workspace to the previous monitor");
        DescribeAeroSpaceCommands({"layout floating tiling"}, true, short_hint, full_hint);
        CHECK(short_hint == "float");
        CHECK(full_hint == "toggle floating / tiling");
        DescribeAeroSpaceCommands({"close-all-windows-but-current"}, true, short_hint, full_hint);
        CHECK(short_hint == "close others");
        DescribeAeroSpaceCommands({"caf\xC3\xA9 now"}, false, short_hint, full_hint);
        CHECK(short_hint == "caf?");
        CHECK(full_hint == "caf? now");
        DescribeAeroSpaceCommands({"some-very-long-command-name"}, false, short_hint, full_hint);
        CHECK(short_hint == "some-very-lo");
        CHECK(full_hint == "some-very-long-command-name");
        // Options (and their values) are never the positional argument.
        DescribeAeroSpaceCommands({"focus --boundaries all-monitors-outer-frame left"}, false,
                                  short_hint, full_hint);
        CHECK(short_hint == "left");
        CHECK(full_hint == "focus the window left");
        DescribeAeroSpaceCommands({"move-node-to-workspace --focus-follows-window 3"}, false,
                                  short_hint, full_hint);
        CHECK(short_hint == "send 3");
        CHECK(full_hint == "move the window to workspace 3");
        DescribeAeroSpaceCommands({"workspace --wrap-around next"}, false, short_hint, full_hint);
        CHECK(short_hint == "ws next");
        CHECK(full_hint == "switch to workspace next");
        DescribeAeroSpaceCommands({"move-workspace-to-monitor --wrap-around next"}, false,
                                  short_hint, full_hint);
        CHECK(short_hint == "monitor");
        CHECK(full_hint == "move the workspace to the next monitor");
        // No positional word left: generic fallback.
        DescribeAeroSpaceCommands({"focus --boundaries all"}, false, short_hint, full_hint);
        CHECK(short_hint == "focus");
    }

    // The real user config is only checked when present on this machine.
    if (const auto path = FindAeroSpaceConfigPath(); !path.empty())
    {
        std::string error;
        const auto real = LoadAeroSpaceConfig(path, &error);
        CHECK(real.has_value());
        CHECK(real.value_or(AeroSpaceConfig{}).bindings.size() > 0);
    }

    return 0;
}
