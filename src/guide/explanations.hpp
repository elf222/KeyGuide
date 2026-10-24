// guide/explanations.hpp
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "gameLogic/tracked_state.hpp"
#include "guide/catalog.hpp"

// AI explanations for shortcut hints. Network work runs on one background thread; every public
// function is called from the main thread, which owns the cache and applies results in Update().
namespace Guide
{
    struct Status
    {
        bool generating = false, testing = false, loadingModels = false;
        // `generated` counts processed shortcuts; `failed` those that got no usable tip.
        std::size_t generated = 0, failed = 0, toGenerate = 0, rejected = 0;
        std::string message; // Latest outcome for the menu, e.g. an error or a test result.
        bool messageIsError = false;
    };

    void Initialise();
    void Shutdown();
    // Applies finished background work. True when hover text may have changed.
    bool Update();
    // Call after keybinding files are reloaded (HintIds may have moved).
    void RebuildCatalog();

    std::string_view Explanation(HintId id);
    bool Pending(HintId id);
    // Lazily explains the hovered shortcut together with others available in the same state.
    void RequestHover(HintId id, const TrackedState& state, bool aerospace_service);
    void Generate(Domain domain);
    void CancelGeneration();
    void TestConnection();
    void RefreshModels();
    // Call after the provider, its address or its key changes.
    void ProviderChanged();
    void ModelChanged();
    const std::vector<std::string>& AvailableModels();
    void ClearCache();

    std::size_t EntryCount(Domain domain);
    std::size_t ExplainedCount(Domain domain);
    Status CurrentStatus();
    // Writes <AppDataDirectory>/guides/<domain>.md from the cached explanations.
    bool ExportStudyGuide(Domain domain, std::string& path, std::string& error);
} // namespace Guide
