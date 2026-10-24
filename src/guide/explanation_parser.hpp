// guide/explanation_parser.hpp
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "guide/catalog.hpp"

namespace Guide
{
    inline constexpr std::size_t EXPLANATION_MAX_LENGTH = 140;

    struct BatchPrompt
    {
        std::string system, user;
    };

    struct ParsedExplanation
    {
        std::size_t index = 0; // Position in the batch that was sent.
        std::string text;
    };

    struct ParsedBatch
    {
        std::vector<ParsedExplanation> accepted;
        std::size_t rejected = 0; // Failed the grounding checks.
        std::string error;        // Reply was not usable JSON.
    };

    // NormaliseKeys() of the shortcuts each CatalogEntry::allowGroup may mention.
    using AllowedKeys = std::unordered_map<std::string, std::unordered_set<std::string>>;

    // Batch ids are positions 0..n-1, so a reply can only refer to shortcuts that were sent.
    BatchPrompt BuildBatchPrompt(const std::vector<const CatalogEntry*>& batch);
    // A group allows its own shortcuts; Zed-only and Neovim-only groups also allow the shared
    // Vim shortcuts, but the shared group allows neither app's own shortcuts.
    AllowedKeys BuildAllowedKeys(const std::vector<CatalogEntry>& catalog);
    ParsedBatch ParseBatchReply(std::string_view reply,
                                const std::vector<const CatalogEntry*>& batch,
                                const AllowedKeys& allowed);
    // Same allowlist for every entry.
    ParsedBatch ParseBatchReply(std::string_view reply,
                                const std::vector<const CatalogEntry*>& batch,
                                const std::unordered_set<std::string>& known_keys);

    // Lowercase; control/command/option/alt -> ctrl/cmd/opt/opt; every run of spaces, "+" or
    // "-" between keys becomes one "-" ("Control + Shift + R" -> "ctrl-shift-r"). A second
    // "+"/"-" in a run is the key itself ("Opt--" stays "opt--").
    std::string NormaliseKeys(std::string_view keys);
    // ASCII-only, single line, collapsed whitespace, at most EXPLANATION_MAX_LENGTH characters.
    std::string SanitiseExplanation(std::string_view text);
    // Rejects explanations that name a modifier shortcut absent from this shortcut and its app.
    bool MentionsUnknownShortcut(std::string_view text, const CatalogEntry& entry,
                                 const std::unordered_set<std::string>& known_keys);
} // namespace Guide
