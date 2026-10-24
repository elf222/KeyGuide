// guide/catalog.cpp
#include "guide/catalog.hpp"
#include "gameLogic/aerospace_bindings.hpp"
#include "gameLogic/blender_bindings.hpp"
#include "gameLogic/shortcut_bindings.hpp"
#include "gameLogic/vim_bindings.hpp"
#include "guide/binding_scopes.hpp"

namespace
{
    std::string SpecialKeyName(Platform::KeyId key)
    {
        using K = Platform::KeyId;
        if (key >= K::F1 && key <= K::F12)
        {
            return "F" + std::to_string(static_cast<int>(key) - static_cast<int>(K::F1) + 1);
        }
        if (key >= K::NumpadZero && key <= K::NumpadNine)
        {
            return "Num" + std::to_string(static_cast<int>(key) - static_cast<int>(K::NumpadZero));
        }
        switch (key)
        {
            case K::Space:
                return "Space";
            case K::Escape:
            case K::CapsLock:
                return "Esc";
            case K::Enter:
                return "Enter";
            case K::Tab:
                return "Tab";
            case K::Backspace:
                return "Backspace";
            case K::Delete:
                return "Delete";
            case K::Left:
                return "Left";
            case K::Right:
                return "Right";
            case K::Up:
                return "Up";
            case K::Down:
                return "Down";
            case K::PageUp:
                return "PageUp";
            case K::PageDown:
                return "PageDown";
            case K::Home:
                return "Home";
            case K::End:
                return "End";
            case K::NumpadPeriod:
                return "Num.";
            case K::NumpadDivide:
                return "Num/";
            case K::NumpadMultiply:
                return "Num*";
            case K::NumpadMinus:
                return "Num-";
            case K::NumpadPlus:
                return "Num+";
            case K::NumpadEnter:
                return "NumEnter";
            case K::NumpadEqual:
                return "Num=";
            default:
                return "?";
        }
    }

    std::string VimGroup(const VimBinding& binding)
    {
        if (!binding.profile)
        {
            return "vim";
        }
        return *binding.profile == Platform::AppProfile::Zed ? "vim-zed" : "vim-neovim";
    }

    std::string VimApp(const VimBinding& binding)
    {
        if (!binding.profile)
        {
            return "Zed and Neovim (Vim keys)";
        }
        return *binding.profile == Platform::AppProfile::Zed ? "Zed (Vim mode)" : "Neovim";
    }

    std::string CacheKey(Guide::Domain domain, const std::string& keys, const std::string& scope,
                         const std::string& does)
    {
        return std::string(Guide::DomainFileName(domain)) + "|" + scope + "|" + keys + "|" + does;
    }
} // namespace

namespace Guide
{
    std::string_view DomainName(Domain domain)
    {
        switch (domain)
        {
            case Domain::Vim:
                return "Vim keys (Zed & Neovim)";
            case Domain::Blender:
                return "Blender";
            case Domain::AeroSpace:
                return "AeroSpace";
            case Domain::Count:
                break;
        }
        return "";
    }

    std::string_view DomainFileName(Domain domain)
    {
        switch (domain)
        {
            case Domain::Vim:
                return "vim";
            case Domain::Blender:
                return "blender";
            case Domain::AeroSpace:
                return "aerospace";
            case Domain::Count:
                break;
        }
        return "";
    }

    std::string PhysicalKeyLabel(Platform::KeyId key, std::uint8_t modifiers)
    {
        std::string label;
        if (modifiers & Platform::CONTROL)
        {
            label += "Ctrl-";
        }
        if (modifiers & Platform::OPTION)
        {
            label += "Opt-";
        }
        if (modifiers & Platform::COMMAND)
        {
            label += "Cmd-";
        }
        if (modifiers & Platform::SHIFT)
        {
            label += "Shift-";
        }
        const auto code = static_cast<int>(key);
        if (code > 32 && code < 127)
        {
            return label + static_cast<char>(code);
        }
        return label + SpecialKeyName(key);
    }

    std::vector<CatalogEntry> BuildCatalog()
    {
        std::vector<CatalogEntry> catalog;
        catalog.reserve(ShortcutCount());
        const auto vim = VimBindings();
        const auto blender = BlenderBindings();
        const auto aerospace = AeroSpaceBindings();
        std::string service_scope = "AeroSpace service mode";
        for (const auto& binding : aerospace)
        {
            if (!binding.service && binding.entersService)
            {
                service_scope +=
                    " (after " + PhysicalKeyLabel(binding.key, binding.modifiers) + ")";
                break;
            }
        }
        for (HintId id = 0; id < ShortcutCount(); ++id)
        {
            CatalogEntry entry;
            entry.id = id;
            if (id < vim.size())
            {
                const auto& binding = vim[id];
                entry.domain = Domain::Vim;
                entry.app = VimApp(binding);
                entry.allowGroup = VimGroup(binding);
                entry.keys = std::string(binding.chord);
                entry.scope = std::string(VimScopeDescription(binding.scope));
                entry.does = std::string(binding.fullHint);
            }
            else if (id < vim.size() + blender.size())
            {
                const auto& binding = blender[id - vim.size()];
                entry.domain = Domain::Blender;
                entry.app = binding.macOnly ? "Blender (macOS)" : "Blender";
                entry.allowGroup = "blender";
                entry.keys = binding.anyModifiers ? PhysicalKeyLabel(binding.key, 0) +
                                                        " (with any modifiers)"
                                                  : PhysicalKeyLabel(binding.key,
                                                                     binding.modifiers);
                entry.scope = std::string(BlenderScopeDescription(binding.scope));
                entry.does = std::string(binding.fullHint);
            }
            else
            {
                const auto& binding = aerospace[id - vim.size() - blender.size()];
                entry.domain = Domain::AeroSpace;
                entry.app = "AeroSpace window manager (global hotkeys)";
                entry.allowGroup = "aerospace";
                entry.keys = PhysicalKeyLabel(binding.key, binding.modifiers);
                entry.scope =
                    binding.service ? service_scope : "anywhere on macOS while AeroSpace runs";
                // `exec-and-forget` arguments can hold private paths or scripts; the model only
                // learns that a shell command runs.
                entry.does = binding.fullHint.starts_with("run:")
                                 ? std::string("run a custom shell command from the user's config")
                                 : std::string(binding.fullHint);
            }
            entry.cacheKey = CacheKey(entry.domain, entry.keys, entry.scope, entry.does);
            catalog.push_back(std::move(entry));
        }
        return catalog;
    }
} // namespace Guide
