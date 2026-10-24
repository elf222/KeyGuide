// keymaps/aerospace_config.cpp
#include "keymaps/aerospace_config.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <toml++/toml.hpp>

namespace
{
    using K = Platform::KeyId;

    struct NamedKey
    {
        std::string_view name;
        K key;
    };

    // Keys that AeroSpace spells by name. Letters, digits, f-keys and keypad digits are computed.
    constexpr std::array named_keys{
        NamedKey{"minus", K::Minus},
        NamedKey{"equal", K::Equal},
        NamedKey{"period", K::Period},
        NamedKey{"comma", K::Comma},
        NamedKey{"slash", K::Slash},
        NamedKey{"backslash", K::Backslash},
        NamedKey{"semicolon", K::Semicolon},
        NamedKey{"quote", K::Apostrophe},
        NamedKey{"backtick", K::Grave},
        NamedKey{"leftSquareBracket", K::LeftBracket},
        NamedKey{"rightSquareBracket", K::RightBracket},
        NamedKey{"space", K::Space},
        NamedKey{"enter", K::Enter},
        NamedKey{"esc", K::Escape},
        NamedKey{"backspace", K::Backspace},
        NamedKey{"tab", K::Tab},
        NamedKey{"forwardDelete", K::Delete},
        NamedKey{"left", K::Left},
        NamedKey{"right", K::Right},
        NamedKey{"up", K::Up},
        NamedKey{"down", K::Down},
        NamedKey{"pageUp", K::PageUp},
        NamedKey{"pageDown", K::PageDown},
        NamedKey{"home", K::Home},
        NamedKey{"end", K::End},
        NamedKey{"keypadDecimalMark", K::NumpadPeriod},
        NamedKey{"keypadDivide", K::NumpadDivide},
        NamedKey{"keypadMultiply", K::NumpadMultiply},
        NamedKey{"keypadMinus", K::NumpadMinus},
        NamedKey{"keypadPlus", K::NumpadPlus},
        NamedKey{"keypadEnter", K::NumpadEnter},
        NamedKey{"keypadEqual", K::NumpadEqual},
    };

    void SetError(std::string* error, const std::string& message)
    {
        if (error != nullptr)
        {
            *error = message;
        }
    }

    bool ParseNumber(std::string_view text, int& number)
    {
        if (text.empty() || text.size() > 3)
        {
            return false;
        }
        const char* last = text.data() + text.size();
        const auto [end, error] = std::from_chars(text.data(), last, number);
        return error == std::errc() && end == last;
    }

    bool KeyFromName(std::string_view name, K& key)
    {
        for (const auto& named : named_keys)
        {
            if (named.name == name)
            {
                key = named.key;
                return true;
            }
        }
        int number = 0;
        if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z')
        {
            key = static_cast<K>(static_cast<int>(K::A) + (name[0] - 'a'));
            return true;
        }
        if (name.size() == 1 && name[0] >= '0' && name[0] <= '9')
        {
            key = static_cast<K>(static_cast<int>(K::Zero) + (name[0] - '0'));
            return true;
        }
        if (name.starts_with('f') && ParseNumber(name.substr(1), number) && number >= 1 &&
            number <= 12)
        {
            key = static_cast<K>(static_cast<int>(K::F1) + number - 1);
            return true;
        }
        if (name.starts_with("keypad") && ParseNumber(name.substr(6), number) && number >= 0 &&
            number <= 9)
        {
            key = static_cast<K>(static_cast<int>(K::NumpadZero) + number);
            return true;
        }
        return false;
    }

    bool ModifierFromName(std::string_view name, std::uint8_t& bit)
    {
        if (name == "cmd")
        {
            bit = Platform::COMMAND;
            return true;
        }
        if (name == "alt")
        {
            bit = Platform::OPTION;
            return true;
        }
        if (name == "ctrl")
        {
            bit = Platform::CONTROL;
            return true;
        }
        if (name == "shift")
        {
            bit = Platform::SHIFT;
            return true;
        }
        return false;
    }

    // Keeps empty parts so "alt--h" and "alt-" are rejected instead of silently shortened.
    std::vector<std::string_view> SplitOn(std::string_view text, char separator)
    {
        std::vector<std::string_view> parts;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t end = text.find(separator, start);
            parts.push_back(text.substr(start, end - start));
            if (end == std::string_view::npos)
            {
                return parts;
            }
            start = end + 1;
        }
    }

    std::vector<std::string_view> Words(std::string_view text)
    {
        std::vector<std::string_view> words;
        std::size_t start = text.find_first_not_of(" \t");
        while (start != std::string_view::npos)
        {
            const std::size_t end = text.find_first_of(" \t", start);
            words.push_back(text.substr(start, end - start));
            start = text.find_first_not_of(" \t", end);
        }
        return words;
    }

    std::string_view Trim(std::string_view text)
    {
        const std::size_t first = text.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            return {};
        }
        const std::size_t last = text.find_last_not_of(" \t");
        return text.substr(first, last - first + 1);
    }

    std::string_view FirstWord(std::string_view text)
    {
        const auto words = Words(text);
        return words.empty() ? std::string_view() : words.front();
    }

    // Splits one TOML value on ';'. Once a command is exec-and-forget, the rest of the text is its
    // shell line and keeps any ';'. Empty pieces are dropped.
    void SplitCommands(std::string_view text, std::vector<std::string>& commands)
    {
        while (true)
        {
            text = Trim(text);
            if (text.empty())
            {
                return;
            }
            if (FirstWord(text) == "exec-and-forget")
            {
                commands.emplace_back(text);
                return;
            }
            const std::size_t end = text.find(';');
            const std::string_view piece = Trim(text.substr(0, end));
            if (!piece.empty())
            {
                commands.emplace_back(piece);
            }
            if (end == std::string_view::npos)
            {
                return;
            }
            text = text.substr(end + 1);
        }
    }

    // Each non-ASCII code point becomes one '?', so UTF-8 sequences do not expand to several.
    std::string Ascii(std::string_view text)
    {
        std::string result;
        for (const char character : text)
        {
            const auto byte = static_cast<unsigned char>(character);
            if (byte < 0x80)
            {
                result.push_back(character);
            }
            else if ((byte & 0xC0) != 0x80)
            {
                result.push_back('?');
            }
        }
        return result;
    }

    void AppendHint(std::string& full_hint, std::string_view part)
    {
        if (!full_hint.empty())
        {
            full_hint += "; ";
        }
        full_hint += part;
    }

    // Options whose value is the following word, e.g. "--boundaries all-monitors-outer-frame".
    bool TakesValue(std::string_view option)
    {
        return option == "--boundaries" || option == "--boundaries-action" ||
               option == "--window-id" || option == "--workspace" || option == "--monitor";
    }

    // Words after the command name that are neither options nor option values.
    std::vector<std::string_view> Positionals(const std::vector<std::string_view>& words)
    {
        std::vector<std::string_view> positionals;
        for (std::size_t index = 1; index < words.size(); index++)
        {
            if (!words[index].starts_with('-'))
            {
                positionals.push_back(words[index]);
            }
            else if (TakesValue(words[index]))
            {
                index++;
            }
        }
        return positionals;
    }

    // The first non-option word after "mode", e.g. "service" in "mode -- service".
    std::string_view ModeTarget(const std::vector<std::string_view>& words)
    {
        for (std::size_t index = 1; index < words.size(); index++)
        {
            if (!words[index].starts_with('-'))
            {
                return words[index];
            }
        }
        return {};
    }

    // Returns false for commands without a dedicated hint; the caller falls back to the first word.
    bool KnownHint(const std::vector<std::string_view>& words, std::string_view command,
                   std::string& short_hint, std::string& full_hint)
    {
        const std::vector<std::string_view> args = Positionals(words);
        const std::string_view name = words.front();
        const std::string_view arg = args.empty() ? std::string_view() : args.front();
        const std::string_view last = words.back();
        if (name == "focus" && !arg.empty())
        {
            short_hint = std::string(arg);
            full_hint = "focus the window " + std::string(arg);
        }
        else if (name == "move" && !arg.empty())
        {
            short_hint = "move";
            full_hint = "move the window " + std::string(arg);
        }
        else if (name == "join-with" && !arg.empty())
        {
            short_hint = "join";
            full_hint = "join with the window " + std::string(arg);
        }
        else if (name == "resize" && (last.starts_with('-') || last.starts_with('+')))
        {
            short_hint = last.starts_with('-') ? "shrink" : "grow";
            full_hint = std::string(command);
        }
        else if (name == "workspace" && !arg.empty())
        {
            short_hint = "ws " + std::string(arg);
            full_hint = "switch to workspace " + std::string(arg);
        }
        else if (name == "move-node-to-workspace" && !arg.empty())
        {
            short_hint = "send " + std::string(arg);
            full_hint = "move the window to workspace " + std::string(arg);
        }
        else if (name == "workspace-back-and-forth")
        {
            short_hint = "back";
            full_hint = "previous workspace (back and forth)";
        }
        else if (name == "move-workspace-to-monitor")
        {
            short_hint = "monitor";
            const bool previous = std::find(args.begin(), args.end(), "prev") != args.end();
            full_hint = previous ? "move the workspace to the previous monitor"
                                 : "move the workspace to the next monitor";
        }
        else if (name == "layout" && arg == "tiles")
        {
            short_hint = "tiles";
            full_hint = "tiles layout; toggle horizontal / vertical";
        }
        else if (name == "layout" && arg == "accordion")
        {
            short_hint = "accordion";
            full_hint = "accordion layout; toggle horizontal / vertical";
        }
        else if (name == "layout" && arg == "floating" && args.size() > 1 && args[1] == "tiling")
        {
            short_hint = "float";
            full_hint = "toggle floating / tiling";
        }
        else if (name == "fullscreen")
        {
            short_hint = "fullscreen";
            full_hint = "toggle fullscreen";
        }
        else if (name == "reload-config")
        {
            short_hint = "reload";
            full_hint = "reload config";
        }
        else if (name == "flatten-workspace-tree")
        {
            short_hint = "reset";
            full_hint = "reset layout (flatten workspace tree)";
        }
        else if (name == "close-all-windows-but-current")
        {
            short_hint = "close others";
            full_hint = "close all windows but current";
        }
        else if (name == "close")
        {
            short_hint = "close";
            full_hint = "close the window";
        }
        else if (name == "exec-and-forget" && words.size() > 1)
        {
            short_hint = "run";
            full_hint = "run: " + std::string(Trim(command.substr(name.size())));
            if (full_hint.size() > 60)
            {
                full_hint = full_hint.substr(0, 58) + "..";
            }
        }
        else
        {
            return false;
        }
        return true;
    }

    bool ReadCommands(const toml::node& node, std::vector<std::string>& commands)
    {
        if (const auto* text = node.as_string())
        {
            commands.push_back(text->get());
            return true;
        }
        const auto* list = node.as_array();
        if (list == nullptr)
        {
            return false;
        }
        for (const auto& element : *list)
        {
            const auto* text = element.as_string();
            if (text == nullptr)
            {
                return false;
            }
            commands.push_back(text->get());
        }
        return true;
    }

    void ReadMode(const std::string& mode_name, const toml::table& bindings,
                  AeroSpaceConfig& config)
    {
        const bool service = mode_name == "service";
        for (const auto& [notation_key, value] : bindings)
        {
            const std::string notation(notation_key.str());
            const std::string label = "'" + notation + "' in mode '" + mode_name + "'";
            AeroSpaceConfigBinding binding;
            binding.service = service;
            if (!ParseAeroSpaceKey(notation, binding.key, binding.modifiers))
            {
                config.warnings.push_back("skipped " + label + ": unsupported key or modifier");
                continue;
            }
            std::vector<std::string> raw_commands;
            if (!ReadCommands(value, raw_commands))
            {
                config.warnings.push_back("skipped " + label +
                                          ": expected a string or an array of strings");
                continue;
            }
            std::vector<std::string> commands;
            for (const auto& raw : raw_commands)
            {
                SplitCommands(raw, commands);
            }
            if (commands.empty())
            {
                config.warnings.push_back("skipped " + label + ": no commands");
                continue;
            }

            std::string joined;
            for (std::size_t index = 0; index < raw_commands.size(); index++)
            {
                if (index > 0)
                {
                    joined += "; ";
                }
                joined += raw_commands[index];
            }
            binding.command = Ascii(joined);

            // Mode switches are not hinted; the last one decides where the binding leads.
            std::vector<std::string> real_commands;
            bool switches = false;
            std::string_view target;
            for (const auto& command : commands)
            {
                const auto words = Words(command);
                if (!words.empty() && words.front() == "mode")
                {
                    switches = true;
                    target = ModeTarget(words);
                }
                else
                {
                    real_commands.push_back(command);
                }
            }
            if (switches)
            {
                binding.entersService = !service && target == "service";
                binding.returnsToMain = service && target == "main";
                if (target != "main" && target != "service")
                {
                    config.warnings.push_back(
                        label + " switches to mode '" + std::string(target) +
                        "': KeyGuide only models main and service, so no mode change is shown");
                }
            }

            DescribeAeroSpaceCommands(real_commands, service, binding.shortHint, binding.fullHint);
            if (real_commands.empty())
            {
                binding.shortHint = Ascii(target).substr(0, 12);
                if (target != "main" && target != "service")
                {
                    binding.fullHint = "switch to mode " + Ascii(target);
                }
            }
            if (binding.entersService)
            {
                AppendHint(binding.fullHint, "enter service mode");
            }
            if (binding.returnsToMain)
            {
                AppendHint(binding.fullHint, "back to main mode");
            }
            config.bindings.push_back(binding);
        }
    }

    AeroSpaceConfig ReadConfig(const toml::table& root)
    {
        AeroSpaceConfig config;
        const auto* mapping = root.get_as<toml::table>("key-mapping");
        if (mapping != nullptr && mapping->contains("preset"))
        {
            const auto* preset = mapping->get_as<std::string>("preset");
            if (preset == nullptr || *preset != "qwerty")
            {
                config.warnings.push_back(
                    "key-mapping.preset is not 'qwerty': hints show QWERTY physical keys");
            }
        }

        const auto* modes = root.get_as<toml::table>("mode");
        if (modes == nullptr)
        {
            return config;
        }
        for (const auto& [mode_key, mode_node] : *modes)
        {
            const std::string mode_name(mode_key.str());
            if (mode_name != "main" && mode_name != "service")
            {
                config.warnings.push_back("skipped mode '" + mode_name +
                                          "': only main and service are supported");
                continue;
            }
            const auto* mode = mode_node.as_table();
            const auto* bindings = mode != nullptr ? mode->get_as<toml::table>("binding") : nullptr;
            if (bindings != nullptr)
            {
                ReadMode(mode_name, *bindings, config);
            }
        }
        return config;
    }
} // namespace

std::optional<AeroSpaceConfig> ParseAeroSpaceConfig(std::string_view text, std::string* error)
{
    try
    {
        const auto root = toml::parse(text);
        return ReadConfig(root);
    }
    catch (const toml::parse_error& failure)
    {
        SetError(error, "invalid TOML: " + std::string(failure.description()));
        return std::nullopt;
    }
    catch (const std::exception& failure)
    {
        SetError(error, failure.what());
        return std::nullopt;
    }
}

std::optional<AeroSpaceConfig> LoadAeroSpaceConfig(const std::string& path, std::string* error)
{
    std::error_code file_error;
    if (!std::filesystem::is_regular_file(path, file_error))
    {
        SetError(error, "cannot read " + path);
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        SetError(error, "cannot open " + path);
        return std::nullopt;
    }
    const std::string text((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    if (file.bad())
    {
        SetError(error, "cannot read " + path);
        return std::nullopt;
    }
    return ParseAeroSpaceConfig(text, error);
}

std::string FindAeroSpaceConfigPath()
{
    const char* home_env = std::getenv("HOME");
    const char* xdg_env = std::getenv("XDG_CONFIG_HOME");
    const std::string home = home_env != nullptr ? home_env : "";
    const std::string xdg = xdg_env != nullptr ? xdg_env : "";

    std::vector<std::string> candidates;
    if (!home.empty())
    {
        candidates.push_back(home + "/.aerospace.toml");
    }
    if (!xdg.empty())
    {
        candidates.push_back(xdg + "/aerospace/aerospace.toml");
    }
    if (!home.empty())
    {
        candidates.push_back(home + "/.config/aerospace/aerospace.toml");
    }
    for (const auto& candidate : candidates)
    {
        std::error_code file_error;
        if (std::filesystem::is_regular_file(candidate, file_error))
        {
            return candidate;
        }
    }
    return {};
}

bool ParseAeroSpaceKey(std::string_view notation, Platform::KeyId& key, std::uint8_t& modifiers)
{
    const auto parts = SplitOn(notation, '-');
    K parsed_key = K::Unknown;
    if (!KeyFromName(parts.back(), parsed_key))
    {
        return false;
    }
    std::uint8_t parsed_modifiers = 0;
    for (std::size_t index = 0; index + 1 < parts.size(); index++)
    {
        std::uint8_t bit = 0;
        if (!ModifierFromName(parts[index], bit))
        {
            return false;
        }
        parsed_modifiers |= bit;
    }
    key = parsed_key;
    modifiers = parsed_modifiers;
    return true;
}

void DescribeAeroSpaceCommands(const std::vector<std::string>& commands, bool /*service*/,
                               std::string& short_hint, std::string& full_hint)
{
    short_hint.clear();
    full_hint.clear();
    if (commands.empty())
    {
        return;
    }
    const std::string_view command = Trim(commands.front());
    const auto words = Words(command);
    if (words.empty())
    {
        return;
    }
    if (!KnownHint(words, command, short_hint, full_hint))
    {
        short_hint = std::string(words.front().substr(0, 12));
        full_hint = std::string(command);
    }
    if (commands.size() > 1)
    {
        full_hint += " + more";
    }
    short_hint = Ascii(short_hint).substr(0, 12);
    full_hint = Ascii(full_hint);
}
