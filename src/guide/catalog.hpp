// guide/catalog.hpp
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "gameLogic/tracking_types.hpp"
#include "platform/input_event.hpp"

namespace Guide
{
    enum class Domain
    {
        Vim,
        Blender,
        AeroSpace,
        Count
    };

    // Everything the model may know about one shortcut. `cacheKey` changes whenever the
    // binding's keys, context or description change, so stale explanations are never shown.
    struct CatalogEntry
    {
        HintId id = NO_HINT;
        Domain domain = Domain::Vim;
        std::string app, keys, scope, does, cacheKey;
        // Shortcuts a tip may mention come from this group: "vim" (shared by Zed and Neovim),
        // "vim-zed", "vim-neovim", "blender" or "aerospace".
        std::string allowGroup;
    };

    std::string_view DomainName(Domain domain);
    std::string_view DomainFileName(Domain domain);
    // "Ctrl-Shift-R", "Opt-H", "Esc"; ASCII only.
    std::string PhysicalKeyLabel(Platform::KeyId key, std::uint8_t modifiers);
    // One entry per HintId in [0, ShortcutCount()).
    std::vector<CatalogEntry> BuildCatalog();
} // namespace Guide
