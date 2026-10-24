// menu/menu_model.cpp
#include "menu/menu_model.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <sstream>
#include "gameLogic/keyboard_status.hpp"
#include "global/asset_path.hpp"
#include "global/global.hpp"
#include "guide/ai_client.hpp"
#include "guide/explanations.hpp"
#include "keymaps/keymap_files.hpp"
#include "platform/platform.hpp"
#include "registry/global.hpp"
#include "settings/ai.hpp"
#include "settings/app_paths.hpp"
#include "settings/blender.hpp"
#include "settings/persistence.hpp"
#include "settings/settings.hpp"

namespace
{
    using Domain = Guide::Domain;
    constexpr std::size_t STATUS_LINE_LENGTH = 90;
    constexpr std::array<Domain, 3> DOMAINS{Domain::Vim, Domain::Blender, Domain::AeroSpace};

    struct Nudge
    {
        const char* direction;
        float x, y;
        bool large;
    };
    constexpr std::array<Nudge, 8> NUDGES{{
        {"Left", -Default::WINDOW_MOVE_STEP, 0, false},
        {"Right", Default::WINDOW_MOVE_STEP, 0, false},
        {"Up", 0, -Default::WINDOW_MOVE_STEP, false},
        {"Down", 0, Default::WINDOW_MOVE_STEP, false},
        {"Left", -Default::WINDOW_MOVE_LARGE_STEP, 0, true},
        {"Right", Default::WINDOW_MOVE_LARGE_STEP, 0, true},
        {"Up", 0, -Default::WINDOW_MOVE_LARGE_STEP, true},
        {"Down", 0, Default::WINDOW_MOVE_LARGE_STEP, true},
    }};

    MenuItem Item(std::string title, MenuAction action, int argument = 0, bool enabled = true)
    {
        MenuItem item;
        item.title = std::move(title);
        item.action = action;
        item.argument = argument;
        item.enabled = enabled;
        return item;
    }

    MenuItem Choice(std::string title, bool checked, MenuAction action, int argument = 0)
    {
        auto item = Item(std::move(title), action, argument);
        item.checked = checked;
        return item;
    }

    MenuItem Info(std::string title, MenuStyle style = MenuStyle::Detail)
    {
        if (title.size() > STATUS_LINE_LENGTH)
        {
            title = title.substr(0, STATUS_LINE_LENGTH - 3) + "...";
        }
        MenuItem item;
        item.title = std::move(title);
        item.style = style;
        item.enabled = false;
        return item;
    }

    MenuItem Separator()
    {
        MenuItem item;
        item.separator = true;
        return item;
    }

    MenuItem Submenu(std::string title, std::vector<MenuItem> children)
    {
        MenuItem item;
        item.title = std::move(title);
        item.children = std::move(children);
        return item;
    }

    std::string AppName(Platform::AppProfile profile)
    {
        switch (profile)
        {
            case Platform::AppProfile::Zed:
                return "Zed";
            case Platform::AppProfile::Neovim:
                return "Neovim";
            case Platform::AppProfile::Blender:
                return "Blender";
        }
        return "";
    }

    bool Proprietary(const std::string& model)
    {
        return model.starts_with("anthropic-") ||
               (model.starts_with("openai-") && !model.starts_with("openai-gpt-oss"));
    }

    std::string Shorten(std::string text, std::size_t length)
    {
        return text.size() > length ? text.substr(0, length - 3) + "..." : text;
    }

    void AddStatus(std::vector<MenuItem>& menu)
    {
        const auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
        if (!tracked)
        {
            return;
        }
        const auto& state = tracked->displayedState();
        const auto input = tracked->displayedInput(Global::Input::status);
        const auto status = BuildKeyboardStatus(state, input, Global::Input::aerospaceService);
        std::string title = status.kind == StatusKind::Mode || status.kind == StatusKind::Resync
                                ? AppName(state.appProfile) + " · " + std::string(status.label)
                                : std::string(status.label);
        if (!status.pending.empty())
        {
            title += " · " + status.pending;
        }
        menu.push_back(Info(title, MenuStyle::Title));
        std::istringstream lines(status.detail);
        std::string line;
        for (int count = 0; count < 5 && std::getline(lines, line);)
        {
            if (!line.empty())
            {
                menu.push_back(Info(line));
                ++count;
            }
        }
    }

    std::vector<MenuItem> ResyncMenu()
    {
        using Platform::EventType;
        const auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
        const bool blender =
            tracked && tracked->value.appProfile == Platform::AppProfile::Blender;
        if (blender)
        {
            return {
                Item("Object Mode · 3D Viewport", MenuAction::Baseline,
                     static_cast<int>(EventType::ObjectBaseline)),
                Item("Mesh Edit Mode · 3D Viewport", MenuAction::Baseline,
                     static_cast<int>(EventType::MeshEditBaseline)),
                Item("Curve Edit Mode · 3D Viewport", MenuAction::Baseline,
                     static_cast<int>(EventType::CurveEditBaseline)),
                Item("Node Editor", MenuAction::Baseline,
                     static_cast<int>(EventType::NodeEditorBaseline)),
                Separator(),
                Info("Pick the mode Blender actually shows"),
            };
        }
        return {
            Item("Normal", MenuAction::Baseline, static_cast<int>(EventType::NormalBaseline)),
            Item("Insert", MenuAction::Baseline, static_cast<int>(EventType::InsertBaseline)),
            Item("Visual", MenuAction::Baseline, static_cast<int>(EventType::VisualBaseline)),
            Item("Replace", MenuAction::Baseline, static_cast<int>(EventType::ReplaceBaseline)),
            Separator(),
            Info("Tip: pressing Esc twice also resyncs to Normal"),
        };
    }

    std::vector<MenuItem> ModelMenu()
    {
        std::vector<MenuItem> menu, proprietary;
        const auto& current = CurrentAIModel();
        const auto& models = Guide::AvailableModels();
        if (!current.empty() && std::find(models.begin(), models.end(), current) == models.end())
        {
            menu.push_back(Choice(current, true, MenuAction::None));
        }
        for (const auto& model : models)
        {
            auto item = Choice(model, model == current, MenuAction::SelectModel);
            item.payload = model;
            (Proprietary(model) ? proprietary : menu).push_back(std::move(item));
        }
        if (models.empty())
        {
            menu.push_back(Info(Guide::CurrentStatus().loadingModels ? "Loading models..."
                                                                     : "No models loaded"));
        }
        if (!proprietary.empty())
        {
            menu.push_back(Separator());
            menu.push_back(Submenu("Proprietary (not open-weight)", std::move(proprietary)));
        }
        menu.push_back(Separator());
        menu.push_back(Item("Refresh Model List", MenuAction::RefreshModels));
        return menu;
    }

    std::vector<MenuItem> AIMenu()
    {
        const auto status = Guide::CurrentStatus();
        const bool digitalocean = S::aiProvider == Default::AIProvider::DigitalOcean;
        const auto& model = CurrentAIModel();
        std::vector<MenuItem> menu;
        menu.push_back(Info(std::string(AIProviderName(S::aiProvider)) + " · " +
                                (model.empty() ? "no model selected" : model),
                            MenuStyle::Title));
        if (status.generating)
        {
            menu.push_back(Info("Explaining " + std::to_string(status.generated) + " of " +
                                std::to_string(status.toGenerate) + " shortcuts..."));
        }
        if (!status.message.empty())
        {
            menu.push_back(
                Info(status.message, status.messageIsError ? MenuStyle::Warning : MenuStyle::Detail));
        }
        if (status.rejected > 0)
        {
            menu.push_back(Info(std::to_string(status.rejected) +
                                " tips rejected by the grounding check"));
        }
        menu.push_back(Separator());
        const auto placement = [](const char* title, Default::TipPlacement value)
        {
            return Choice(title, S::tipPlacement == value, MenuAction::TipPlacement,
                          static_cast<int>(value));
        };
        menu.push_back(Submenu("Tip on Hover",
                               {
                                   placement("Off", Default::TipPlacement::Off),
                                   placement("Below the Key", Default::TipPlacement::Below),
                                   placement("Right of the Key", Default::TipPlacement::Right),
                               }));
        std::vector<MenuItem> generate, export_guide;
        for (const auto domain : DOMAINS)
        {
            const auto total = Guide::EntryCount(domain);
            const auto done = Guide::ExplainedCount(domain);
            const auto name = std::string(Guide::DomainName(domain));
            generate.push_back(Item(name + "  (" + std::to_string(done) + "/" +
                                        std::to_string(total) + ")",
                                    MenuAction::Generate, static_cast<int>(domain),
                                    done < total));
            export_guide.push_back(Item(name + "…", MenuAction::ExportGuide,
                                        static_cast<int>(domain)));
        }
        menu.push_back(Submenu("Explain All Shortcuts", std::move(generate)));
        if (status.generating)
        {
            menu.push_back(Item("Stop Explaining", MenuAction::StopGenerating));
        }
        menu.push_back(Submenu("Export Study Guide", std::move(export_guide)));
        menu.push_back(Separator());
        menu.push_back(Submenu(
            "Provider",
            {
                Choice("Ollama · local open-weight models", !digitalocean,
                       MenuAction::SelectProvider, static_cast<int>(Default::AIProvider::Ollama)),
                Choice("DigitalOcean Inference · cloud", digitalocean, MenuAction::SelectProvider,
                       static_cast<int>(Default::AIProvider::DigitalOcean)),
            }));
        menu.push_back(Submenu("Model", ModelMenu()));
        menu.push_back(Item(status.testing ? "Testing Connection..." : "Test Connection",
                            MenuAction::TestConnection, 0, !status.testing));
        menu.push_back(Separator());
        if (std::getenv(Default::DIGITALOCEAN_KEY_ENVIRONMENT))
        {
            menu.push_back(Info(std::string("DigitalOcean key from $") +
                                Default::DIGITALOCEAN_KEY_ENVIRONMENT));
        }
        const bool key_stored = AI::HasStoredKey();
        menu.push_back(Item(key_stored ? "Replace DigitalOcean Access Key…"
                                       : "Set DigitalOcean Access Key…",
                            MenuAction::SetDigitalOceanKey));
        if (key_stored)
        {
            menu.push_back(Item("Remove DigitalOcean Access Key", MenuAction::RemoveDigitalOceanKey));
        }
        menu.push_back(Item("Ollama Server: " + Shorten(S::ollamaUrl, 40) + "…",
                            MenuAction::SetOllamaServer));
        menu.push_back(Separator());
        menu.push_back(Item("Clear Saved Tips", MenuAction::ClearTips));
        return menu;
    }

    std::vector<MenuItem> KeybindingsMenu()
    {
        const auto& files = KeymapFiles();
        std::vector<MenuItem> menu;
        if (files.aerospaceFromFile)
        {
            menu.push_back(Info("AeroSpace: " + files.aerospacePath + " · " +
                                std::to_string(files.aerospaceBindings) + " shortcuts"));
        }
        else
        {
            menu.push_back(Info("AeroSpace: built-in default config"));
            if (!files.aerospaceError.empty())
            {
                menu.push_back(Info(files.aerospaceError, MenuStyle::Warning));
            }
        }
        for (std::size_t i = 0; i < files.aerospaceWarnings.size() && i < 3; ++i)
        {
            menu.push_back(Info(files.aerospaceWarnings[i], MenuStyle::Warning));
        }
        menu.push_back(Item("Reload Keybinding Files", MenuAction::ReloadKeybindings));
        menu.push_back(Item("Open AeroSpace Config", MenuAction::OpenAeroSpaceConfig, 0,
                            !files.aerospacePath.empty()));
        menu.push_back(Separator());
        menu.push_back(Choice("Read Blender State", S::readBlenderState,
                              MenuAction::ToggleBlenderState));
        menu.push_back(Item("Reveal Blender Add-on…", MenuAction::RevealBlenderAddon));
        return menu;
    }

    std::vector<MenuItem> AppearanceMenu()
    {
        using enum Default::TransparencyMode;
        using enum Default::WindowPosition;
        const auto transparency = [](const char* title, Default::TransparencyMode mode)
        {
            return Choice(title, S::transparencyMode == mode, MenuAction::Transparency,
                          static_cast<int>(mode));
        };
        const auto position = [](const char* title, Default::WindowPosition value)
        {
            return Choice(title, S::windowPosition == value, MenuAction::Position,
                          static_cast<int>(value));
        };
        std::vector<MenuItem> nudge;
        for (std::size_t i = 0; i < NUDGES.size(); ++i)
        {
            if (i == 4)
            {
                nudge.push_back(Separator());
            }
            nudge.push_back(Item(std::string(NUDGES[i].direction) + " " +
                                     std::to_string(static_cast<int>(
                                         NUDGES[i].large ? Default::WINDOW_MOVE_LARGE_STEP
                                                         : Default::WINDOW_MOVE_STEP)) +
                                     " px",
                                 MenuAction::Nudge, static_cast<int>(i)));
        }
        return {
            Submenu("Transparency",
                    {
                        transparency("Opaque", Off),
                        transparency("Always Translucent", Always),
                        transparency("Translucent While Hovered", OnHover),
                        transparency("Translucent When Not Hovered", WhenNotHovered),
                    }),
            Choice("Show Function Row", S::showFunctionRow, MenuAction::ToggleFunctionRow),
            Choice("Show Numpad", S::showNumpad, MenuAction::ToggleNumpad),
            Separator(),
            Submenu("Position",
                    {
                        position("Top Left", TopLeft),
                        position("Top Center", TopCenter),
                        position("Top Right", TopRight),
                        position("Center", Center),
                        position("Bottom Left", BottomLeft),
                        position("Bottom Center", BottomCenter),
                        position("Bottom Right", BottomRight),
                        Separator(),
                        Item("Move to Next Display", MenuAction::NextDisplay),
                    }),
            Submenu("Nudge", std::move(nudge)),
        };
    }

    std::vector<MenuItem> PreviewMenu()
    {
        const bool preview = Platform::IsPreview();
        const auto profile = Global::Input::status.appProfile;
        const auto choice = [&](const char* title, Platform::AppProfile value)
        {
            return Choice(title, preview && profile == value, MenuAction::Preview,
                          static_cast<int>(value));
        };
        return {
            Choice("Off · Floating Overlay", !preview, MenuAction::Preview, -1),
            Separator(),
            choice("Zed · Vim", Platform::AppProfile::Zed),
            choice("Ghostty · Neovim", Platform::AppProfile::Neovim),
            choice("Blender", Platform::AppProfile::Blender),
        };
    }

    std::string TrimmedText(std::string text)
    {
        const auto first = text.find_first_not_of(" \t\r\n");
        const auto last = text.find_last_not_of(" \t\r\n");
        return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    }

    void PromptDigitalOceanKey()
    {
        const auto key = Platform::PromptText(
            "DigitalOcean model access key",
            "Create one at cloud.digitalocean.com → Inference → Model Access Keys, then paste "
            "it here. KeyGuide stores it in your login Keychain and only sends it to "
            "inference.do-ai.run.",
            "", true);
        if (!key)
        {
            return;
        }
        const auto trimmed = TrimmedText(*key);
        if (trimmed.empty())
        {
            return;
        }
        if (!Platform::StoreSecret(Default::DIGITALOCEAN_SECRET, trimmed))
        {
            Platform::ShowMessage("KeyGuide", "The key could not be saved.");
            return;
        }
        AI::ForgetCachedKey();
        if (S::aiProvider == Default::AIProvider::DigitalOcean)
        {
            Guide::ProviderChanged();
        }
    }

    void PromptOllamaServer()
    {
        const auto url = Platform::PromptText(
            "Ollama server",
            "Address of an Ollama server, e.g. http://localhost:11434 or a GPU droplet "
            "reached over Tailscale.",
            S::ollamaUrl, false);
        if (!url)
        {
            return;
        }
        const auto trimmed = TrimmedText(*url);
        if (!trimmed.starts_with("http://") && !trimmed.starts_with("https://"))
        {
            Platform::ShowMessage("KeyGuide", "The address must start with http:// or https://");
            return;
        }
        S::ollamaUrl = trimmed;
        SaveSettings();
        if (S::aiProvider == Default::AIProvider::Ollama)
        {
            Guide::ProviderChanged();
        }
    }
} // namespace

std::vector<MenuItem> BuildMenu()
{
    std::vector<MenuItem> menu;
    AddStatus(menu);
    switch (Platform::ObservationState())
    {
        case Platform::Observation::NotGranted:
        {
            auto item = Item("Grant Input Monitoring…", MenuAction::RequestObservation);
            item.style = MenuStyle::Warning;
            menu.push_back(std::move(item));
        }
        break;
        case Platform::Observation::Failed:
        {
            auto item = Item("Retry Observation", MenuAction::RequestObservation);
            item.style = MenuStyle::Warning;
            menu.push_back(std::move(item));
        }
        break;
        case Platform::Observation::NotNeeded:
        case Platform::Observation::Running:
            break;
    }
    menu.push_back(Separator());
    menu.push_back(Choice("Show Keyboard", S::windowVisible, MenuAction::ToggleWindow));
    if (!Platform::IsPreview())
    {
        menu.push_back(Item(Platform::IsPaused() ? "Resume Observation" : "Pause Observation",
                            MenuAction::TogglePause));
    }
    menu.push_back(Submenu("Resync Mode", ResyncMenu()));
    menu.push_back(Separator());
    menu.push_back(Submenu("AI Tips", AIMenu()));
    menu.push_back(Submenu("Keybindings", KeybindingsMenu()));
    menu.push_back(Submenu("Appearance", AppearanceMenu()));
    menu.push_back(Submenu("Preview Window", PreviewMenu()));
    menu.push_back(Separator());
    menu.push_back(Item("Open KeyGuide Folder", MenuAction::OpenDataFolder));
    menu.push_back(Item("Reset Settings", MenuAction::ResetSettings));
    auto quit = Item("Quit KeyGuide", MenuAction::Quit);
    quit.keyEquivalent = 'q';
    menu.push_back(std::move(quit));
    return menu;
}

void PerformMenuAction(MenuAction action, int argument, const std::string& payload)
{
    switch (action)
    {
        case MenuAction::None:
            break;
        case MenuAction::ToggleWindow:
            Platform::ToggleWindowVisibility();
            SaveSettings();
            break;
        case MenuAction::TogglePause:
            Platform::TogglePause();
            break;
        case MenuAction::Baseline:
            Platform::QueueBaseline(static_cast<Platform::EventType>(argument));
            break;
        case MenuAction::TipPlacement:
            S::tipPlacement = static_cast<Default::TipPlacement>(argument);
            SaveSettings();
            break;
        case MenuAction::Generate:
            Guide::Generate(static_cast<Domain>(argument));
            break;
        case MenuAction::StopGenerating:
            Guide::CancelGeneration();
            break;
        case MenuAction::ExportGuide:
        {
            std::string path, error;
            if (Guide::ExportStudyGuide(static_cast<Domain>(argument), path, error))
            {
                Platform::OpenPath(path);
            }
            else
            {
                Platform::ShowMessage("Could not export the study guide", error);
            }
        }
        break;
        case MenuAction::SelectProvider:
        {
            const auto provider = static_cast<Default::AIProvider>(argument);
            if (provider != S::aiProvider)
            {
                S::aiProvider = provider;
                SaveSettings();
                Guide::ProviderChanged();
            }
        }
        break;
        case MenuAction::SelectModel:
            if (!payload.empty() && payload != CurrentAIModel())
            {
                CurrentAIModel() = payload;
                SaveSettings();
                Guide::ModelChanged();
            }
            break;
        case MenuAction::RefreshModels:
            Guide::RefreshModels();
            break;
        case MenuAction::TestConnection:
            Guide::TestConnection();
            break;
        case MenuAction::SetDigitalOceanKey:
            PromptDigitalOceanKey();
            break;
        case MenuAction::RemoveDigitalOceanKey:
            Platform::DeleteSecret(Default::DIGITALOCEAN_SECRET);
            AI::ForgetCachedKey();
            if (S::aiProvider == Default::AIProvider::DigitalOcean)
            {
                Guide::ProviderChanged();
            }
            break;
        case MenuAction::SetOllamaServer:
            PromptOllamaServer();
            break;
        case MenuAction::ClearTips:
            Guide::ClearCache();
            break;
        case MenuAction::ReloadKeybindings:
            LoadKeymapFiles();
            Guide::RebuildCatalog();
            S::keyboardLayoutChanged = true;
            break;
        case MenuAction::OpenAeroSpaceConfig:
            Platform::OpenPath(KeymapFiles().aerospacePath);
            break;
        case MenuAction::ToggleBlenderState:
            S::readBlenderState = !S::readBlenderState;
            SaveSettings();
            break;
        case MenuAction::RevealBlenderAddon:
            Platform::RevealPath(AssetPath("blender/keyguide_bridge.py"));
            break;
        case MenuAction::Transparency:
            S::transparencyMode = static_cast<Default::TransparencyMode>(argument);
            SaveSettings();
            break;
        case MenuAction::ToggleFunctionRow:
            S::showFunctionRow = !S::showFunctionRow;
            S::keyboardLayoutChanged = true;
            SaveSettings();
            break;
        case MenuAction::ToggleNumpad:
            S::showNumpad = !S::showNumpad;
            S::keyboardLayoutChanged = true;
            SaveSettings();
            break;
        case MenuAction::Position:
            Platform::MoveOverlay(static_cast<Default::WindowPosition>(argument));
            SaveSettings();
            break;
        case MenuAction::NextDisplay:
            Platform::MoveOverlayToNextDisplay();
            break;
        case MenuAction::Nudge:
            if (argument >= 0 && argument < static_cast<int>(NUDGES.size()))
            {
                Platform::NudgeOverlay(NUDGES[argument].x, NUDGES[argument].y);
            }
            break;
        case MenuAction::RequestObservation:
            Platform::RequestObservation();
            break;
        case MenuAction::Preview:
            if (argument < 0)
            {
                if (Platform::IsPreview())
                {
                    Platform::TogglePreview();
                }
            }
            else
            {
                Platform::SetPreviewProfile(static_cast<Platform::AppProfile>(argument));
                if (!Platform::IsPreview())
                {
                    Platform::TogglePreview();
                }
            }
            break;
        case MenuAction::OpenDataFolder:
            Platform::OpenPath(AppDataDirectory().string());
            break;
        case MenuAction::ResetSettings:
        {
            const bool was_visible = S::windowVisible;
            ResetSettings();
            if (!was_visible)
            {
                S::windowVisible = false;
                Platform::ToggleWindowVisibility();
            }
            Platform::MoveOverlay(S::windowPosition);
            Guide::ProviderChanged();
        }
        break;
        case MenuAction::Quit:
            Platform::RequestQuit();
            break;
    }
    Platform::WakeMainLoop();
}

std::string DescribeMenu(const std::vector<MenuItem>& items, int depth)
{
    std::string text;
    for (const auto& item : items)
    {
        text += std::string(static_cast<std::size_t>(depth) * 2, ' ');
        if (item.separator)
        {
            text += "    ---\n";
            continue;
        }
        text += item.checked ? "[x] " : "    ";
        text += item.title;
        if (!item.enabled)
        {
            text += "  (disabled)";
        }
        text += item.children.empty() ? "\n" : " >\n";
        text += DescribeMenu(item.children, depth + 1);
    }
    return text;
}
