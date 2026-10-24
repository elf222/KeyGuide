// settings/persistence.cpp
#include "settings/persistence.hpp"
#include <array>
#include <string_view>
#include <utility>
#include <nlohmann/json.hpp>
#include "settings/ai.hpp"
#include "settings/app_paths.hpp"
#include "settings/blender.hpp"
#include "settings/settings.hpp"

namespace
{
    using Json = nlohmann::json;
    using Position = Default::WindowPosition;
    using Transparency = Default::TransparencyMode;
    using Provider = Default::AIProvider;
    using Placement = Default::TipPlacement;

    constexpr std::array<std::pair<Position, std::string_view>, 7> POSITIONS{{
        {Position::TopLeft, "top-left"},
        {Position::TopRight, "top-right"},
        {Position::BottomLeft, "bottom-left"},
        {Position::BottomRight, "bottom-right"},
        {Position::Center, "center"},
        {Position::TopCenter, "top-center"},
        {Position::BottomCenter, "bottom-center"},
    }};
    constexpr std::array<std::pair<Transparency, std::string_view>, 4> TRANSPARENCY{{
        {Transparency::Off, "off"},
        {Transparency::Always, "always"},
        {Transparency::OnHover, "on-hover"},
        {Transparency::WhenNotHovered, "when-not-hovered"},
    }};
    constexpr std::array<std::pair<Placement, std::string_view>, 3> PLACEMENTS{{
        {Placement::Below, "below"},
        {Placement::Right, "right"},
        {Placement::Off, "off"},
    }};
    constexpr std::array<std::pair<Provider, std::string_view>, 2> PROVIDERS{{
        {Provider::Ollama, "ollama"},
        {Provider::DigitalOcean, "digitalocean"},
    }};

    template <typename T, std::size_t N>
    std::string_view NameOf(const std::array<std::pair<T, std::string_view>, N>& names, T value)
    {
        for (const auto& [candidate, name] : names)
        {
            if (candidate == value)
            {
                return name;
            }
        }
        return names[0].second;
    }

    template <typename T, std::size_t N>
    void ReadEnum(const Json& object, const char* key,
                  const std::array<std::pair<T, std::string_view>, N>& names, T& value)
    {
        const auto found = object.find(key);
        if (found == object.end() || !found->is_string())
        {
            return;
        }
        const auto text = found->template get<std::string>();
        for (const auto& [candidate, name] : names)
        {
            if (name == text)
            {
                value = candidate;
            }
        }
    }

    template <typename T> void Read(const Json& object, const char* key, T& value)
    {
        const auto found = object.find(key);
        if (found == object.end())
        {
            return;
        }
        if constexpr (std::is_same_v<T, bool>)
        {
            if (found->is_boolean())
            {
                value = found->template get<bool>();
            }
        }
        else if (found->is_string())
        {
            value = found->template get<std::string>();
        }
    }

    const Json& Section(const Json& root, const char* key)
    {
        static const Json empty = Json::object();
        const auto found = root.find(key);
        return found != root.end() && found->is_object() ? *found : empty;
    }
} // namespace

std::filesystem::path SettingsPath()
{
    return AppDataDirectory() / "settings.json";
}

void LoadSettings()
{
    std::string text;
    if (!ReadWholeFile(SettingsPath(), text))
    {
        return;
    }
    const auto root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        return;
    }
    const auto& overlay = Section(root, "overlay");
    Read(overlay, "visible", S::windowVisible);
    Read(overlay, "functionRow", S::showFunctionRow);
    Read(overlay, "numpad", S::showNumpad);
    Read(overlay, "readBlenderState", S::readBlenderState);
    ReadEnum(overlay, "position", POSITIONS, S::windowPosition);
    ReadEnum(overlay, "transparency", TRANSPARENCY, S::transparencyMode);
    const auto& ai = Section(root, "ai");
    ReadEnum(ai, "provider", PROVIDERS, S::aiProvider);
    // Settings from before tip placement only had an on/off switch.
    bool hover_tips = true;
    Read(ai, "hoverExplanations", hover_tips);
    if (!hover_tips)
    {
        S::tipPlacement = Placement::Off;
    }
    ReadEnum(ai, "tipPlacement", PLACEMENTS, S::tipPlacement);
    Read(ai, "ollamaUrl", S::ollamaUrl);
    Read(ai, "ollamaModel", S::ollamaModel);
    Read(ai, "digitalOceanModel", S::digitalOceanModel);
    if (S::ollamaUrl.empty())
    {
        S::ollamaUrl = Default::OLLAMA_URL;
    }
}

void SaveSettings()
{
    const Json root = {
        {"version", 1},
        {"overlay",
         {
             {"visible", S::windowVisible},
             {"functionRow", S::showFunctionRow},
             {"numpad", S::showNumpad},
             {"readBlenderState", S::readBlenderState},
             {"position", NameOf(POSITIONS, S::windowPosition)},
             {"transparency", NameOf(TRANSPARENCY, S::transparencyMode)},
         }},
        {"ai",
         {
             {"provider", NameOf(PROVIDERS, S::aiProvider)},
             {"tipPlacement", NameOf(PLACEMENTS, S::tipPlacement)},
             {"ollamaUrl", S::ollamaUrl},
             {"ollamaModel", S::ollamaModel},
             {"digitalOceanModel", S::digitalOceanModel},
         }},
    };
    WriteFileAtomically(SettingsPath(), root.dump(2) + "\n");
}

void ResetSettings()
{
    S::windowVisible = Default::WINDOW_VISIBLE;
    S::showFunctionRow = Default::SHOW_FUNCTION_ROW;
    S::showNumpad = Default::SHOW_NUMPAD;
    S::readBlenderState = true;
    S::windowPosition = Default::WINDOW_POSITION;
    S::transparencyMode = Default::TRANSPARENCY_MODE;
    S::aiProvider = Default::AI_PROVIDER;
    S::tipPlacement = Default::TIP_PLACEMENT;
    S::ollamaUrl = Default::OLLAMA_URL;
    S::ollamaModel = Default::OLLAMA_MODEL;
    S::digitalOceanModel = Default::DIGITALOCEAN_MODEL;
    S::keyboardLayoutChanged = true;
    SaveSettings();
}
