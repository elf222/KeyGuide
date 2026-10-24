// tests/guide_tests.cpp
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
#include "test_helpers.hpp"
#include "gameLogic/aerospace_bindings.hpp"
#include "gameLogic/shortcut_bindings.hpp"
#include "guide/ai_client.hpp"
#include "guide/catalog.hpp"
#include "guide/explanation_parser.hpp"
#include "keymaps/aerospace_config.hpp"

// ai_client.cpp reads the Keychain for the stored key; StripModelNoise never needs it.
namespace Platform
{
    std::string LoadSecret(std::string_view)
    {
        return {};
    }
} // namespace Platform

namespace
{
    using K = Platform::KeyId;
    using Json = nlohmann::json;
    using Guide::CatalogEntry;
    using Guide::Domain;
    using Guide::ParsedBatch;

    CatalogEntry MakeEntry(std::string keys, std::string does)
    {
        CatalogEntry entry;
        entry.app = "Zed (Vim mode)";
        entry.keys = std::move(keys);
        entry.scope = "normal mode";
        entry.does = std::move(does);
        return entry;
    }

    std::vector<CatalogEntry> TwoEntries()
    {
        return {MakeEntry("u", "undo the last change"),
                MakeEntry("Ctrl-r", "redo the last undo")};
    }

    std::vector<const CatalogEntry*> Pointers(const std::vector<CatalogEntry>& entries)
    {
        std::vector<const CatalogEntry*> pointers;
        for (const auto& entry : entries)
        {
            pointers.push_back(&entry);
        }
        return pointers;
    }

    ParsedBatch ParseReply(std::string_view reply, const std::vector<CatalogEntry>& entries,
                           const std::unordered_set<std::string>& known_keys = {})
    {
        return Guide::ParseBatchReply(reply, Pointers(entries), known_keys);
    }

    // A reply with one item whose text is `text`; the id is 0.
    std::string Reply(std::string_view text)
    {
        return "{\"items\":[{\"id\":0,\"text\":\"" + std::string(text) + "\"}]}";
    }

    void BuildBatchPromptListsEveryEntry()
    {
        const auto entries = TwoEntries();
        const auto prompt = Guide::BuildBatchPrompt(Pointers(entries));
        CHECK(prompt.system.find("JSON") != std::string::npos);
        CHECK(prompt.user.find("Explain these 2 shortcuts") != std::string::npos);
        // The shortcut list is the JSON array after the first line break.
        const auto items = Json::parse(prompt.user.substr(prompt.user.find('\n') + 1));
        CHECK(items.is_array());
        CHECK(items.size() == entries.size());
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            CHECK(items[i]["id"] == i);
            CHECK(items[i]["keys"] == entries[i].keys);
            CHECK(items[i]["does"] == entries[i].does);
            CHECK(items[i]["app"] == entries[i].app);
            CHECK(items[i]["where"] == entries[i].scope);
        }
    }

    void ParseAcceptsItemsObject()
    {
        const auto entries = TwoEntries();
        const auto parsed = ParseReply(
            R"JSON({"items":[{"id":0,"text":"Undo a mistake quickly."}]})JSON", entries);
        CHECK(parsed.error.empty());
        CHECK(parsed.rejected == 0);
        CHECK(parsed.accepted.size() == 1);
        CHECK(parsed.accepted[0].index == 0);
        CHECK(parsed.accepted[0].text == "Undo a mistake quickly.");
    }

    void ParseAcceptsBareArrayStringIdsAndExplanationField()
    {
        const auto entries = TwoEntries();
        const auto bare = ParseReply(R"JSON([{"id":1,"text":"Redo it."}])JSON", entries);
        CHECK(bare.error.empty());
        CHECK(bare.accepted.size() == 1);
        CHECK(bare.accepted[0].index == 1);
        CHECK(bare.accepted[0].text == "Redo it.");

        const auto named = ParseReply(
            R"JSON({"items":[{"id":"1","explanation":"Redo a change."}]})JSON", entries);
        CHECK(named.error.empty());
        CHECK(named.accepted.size() == 1);
        CHECK(named.accepted[0].index == 1);
        CHECK(named.accepted[0].text == "Redo a change.");
    }

    void ParseAcceptsJsonInProseAndFences()
    {
        const auto entries = TwoEntries();
        const std::string fenced =
            "Sure, here you go:\n```json\n{\"items\":[{\"id\":0,\"text\":\"Undo.\"}]}\n```\nEnjoy!";
        const auto from_fence = ParseReply(AI::StripModelNoise(fenced), entries);
        CHECK(from_fence.error.empty());
        CHECK(from_fence.accepted.size() == 1);
        CHECK(from_fence.accepted[0].text == "Undo.");

        const std::string prose =
            "Here: {\"items\":[{\"id\":1,\"text\":\"Redo.\"}]} Hope it helps.";
        const auto from_prose = ParseReply(AI::StripModelNoise(prose), entries);
        CHECK(from_prose.error.empty());
        CHECK(from_prose.accepted.size() == 1);
        CHECK(from_prose.accepted[0].index == 1);

        // Brackets in the prose before the object must not break extraction.
        CHECK(ParseReply("Here is the list [draft]: {\"items\":[{\"id\":0,\"text\":\"Save.\"}]}",
                         entries)
                  .accepted.size() == 1);
    }

    void ParseRejectsInvalidItems()
    {
        const auto entries = TwoEntries();
        const std::vector<std::string> bad_items{
            R"JSON({"items":[{"id":2,"text":"Past the batch."}]})JSON",
            R"JSON({"items":[{"id":-1,"text":"Negative id."}]})JSON",
            R"JSON({"items":[{"id":0}]})JSON",
            R"JSON({"items":[{"id":0,"text":42}]})JSON",
            R"JSON({"items":[{"id":0,"text":" \t \n "}]})JSON",
            R"JSON({"items":[{"text":"No id."}]})JSON",
            R"JSON({"items":["just a string"]})JSON",
        };
        for (const auto& reply : bad_items)
        {
            const auto parsed = ParseReply(reply, entries);
            CHECK(parsed.accepted.empty());
            CHECK(parsed.rejected == 1);
            CHECK(parsed.error.empty());
        }

        // The first item for an id wins; the second is rejected as a duplicate.
        const auto duplicate = ParseReply(
            R"JSON({"items":[{"id":0,"text":"First."},{"id":0,"text":"Second."}]})JSON", entries);
        CHECK(duplicate.accepted.size() == 1);
        CHECK(duplicate.accepted[0].text == "First.");
        CHECK(duplicate.rejected == 1);

        // A numeric-string id too large for size_t is rejected, not thrown.
        CHECK(ParseReply(R"JSON({"items":[{"id":"99999999999999999999999","text":"Huge."}]})JSON",
                         entries)
                  .rejected == 1);
    }

    void ParseRejectsNonJsonReply()
    {
        const auto entries = TwoEntries();
        for (const std::string_view reply :
             {std::string_view("I'm sorry, I can't help with that."), std::string_view(""),
              std::string_view(R"JSON({"result":"no items here"})JSON")})
        {
            const auto parsed = ParseReply(reply, entries);
            CHECK(!parsed.error.empty());
            CHECK(parsed.accepted.empty());
            CHECK(parsed.rejected == 0);
        }
    }

    void ParseGroundsModifierShortcuts()
    {
        const std::vector<CatalogEntry> vim{MakeEntry("d", "delete the next motion")};
        const std::unordered_set<std::string> none;
        const std::unordered_set<std::string> known{"ctrl-x"};

        // A modifier shortcut that is neither the entry's own nor a known key is rejected.
        CHECK(ParseReply(Reply("Then press Ctrl-x to finish."), vim, none).rejected == 1);
        CHECK(ParseReply(Reply("Then press Ctrl-x to finish."), vim, none).accepted.empty());
        CHECK(ParseReply(Reply("Then press Ctrl-x to finish."), vim, known).accepted.size() == 1);

        // Matching ignores case and the spelling of the modifier.
        CHECK(ParseReply(Reply("Then press CONTROL-X."), vim, known).accepted.size() == 1);
        CHECK(ParseReply(Reply("Then press Ctrl-X."), vim, none).rejected == 1);

        // Typographic dashes are sanitised to hyphens before the check runs.
        CHECK(ParseReply(Reply("Then press Ctrl–x."), vim, none).rejected == 1);

        // Modifier + mouse gestures are not keyboard shortcuts.
        CHECK(ParseReply(Reply("Shift-click to extend the selection."), vim, none)
                  .accepted.size() == 1);
        CHECK(ParseReply(Reply("Alt+drag moves it."), vim, none).accepted.size() == 1);

        // An entry's own keys may be named in any spelling.
        const std::vector<CatalogEntry> redo{MakeEntry("Ctrl-Shift-R", "redo the last undo")};
        CHECK(ParseReply(Reply("Use Ctrl+Shift+R or control-shift-r to redo."), redo, none)
                  .accepted.size() == 1);
        const std::vector<CatalogEntry> search{MakeEntry("Cmd-K", "open the search box")};
        CHECK(ParseReply(Reply("Press Command+K to search."), search, none).accepted.size() == 1);
        CHECK(ParseReply(Reply("Press Opt-H to focus."), redo, none).rejected == 1);

        CHECK(ParseReply(Reply("Shift-R redoes the change."), redo, none).rejected == 1);
    }

    void SanitiseMapsTypographyToAscii()
    {
        CHECK(Guide::SanitiseExplanation("‘hi’ “there”") ==
              "'hi' \"there\"");
        CHECK(Guide::SanitiseExplanation("a–b—c") == "a-b-c");
        CHECK(Guide::SanitiseExplanation("wait…") == "wait...");
        CHECK(Guide::SanitiseExplanation("open → close") == "open -> close");
        CHECK(Guide::SanitiseExplanation("a b") == "a b");
    }

    void SanitiseDropsOtherNonAscii()
    {
        const auto result = Guide::SanitiseExplanation("Use café 你好 now");
        CHECK(result == "Use caf now");
        for (const char c : result)
        {
            CHECK(static_cast<unsigned char>(c) < 0x80);
        }
    }

    void SanitiseCollapsesWhitespace()
    {
        CHECK(Guide::SanitiseExplanation("one\ntwo\r\n\tthree") == "one two three");
        CHECK(Guide::SanitiseExplanation("a    b") == "a b");
        CHECK(Guide::SanitiseExplanation("done   \n") == "done");
        CHECK(Guide::SanitiseExplanation("   lead") == "lead");
        CHECK(Guide::SanitiseExplanation("\n \t ").empty());
    }

    void SanitiseCutsLongText()
    {
        CHECK(Guide::SanitiseExplanation("short").size() == 5);
        CHECK(Guide::SanitiseExplanation(std::string(140, 'a')).size() ==
              Guide::EXPLANATION_MAX_LENGTH);

        // No space to cut at: keep 138 characters and add "..".
        const auto unbroken = Guide::SanitiseExplanation(std::string(200, 'a'));
        CHECK(unbroken.size() <= Guide::EXPLANATION_MAX_LENGTH);
        CHECK(unbroken == std::string(138, 'a') + "..");

        // Cut at the last space that fits, so the result ends on a whole word.
        std::string words;
        for (int i = 0; i < 60; ++i)
        {
            words += "word ";
        }
        const auto cut = Guide::SanitiseExplanation(words);
        CHECK(cut.size() <= Guide::EXPLANATION_MAX_LENGTH);
        CHECK(cut.ends_with("word.."));
    }

    void NormaliseKeysMapsAliases()
    {
        CHECK(Guide::NormaliseKeys("Control + Shift + R") == "ctrl-shift-r");
        CHECK(Guide::NormaliseKeys("Cmd+K") == "cmd-k");
        CHECK(Guide::NormaliseKeys("Option-H") == "opt-h");
        CHECK(Guide::NormaliseKeys("Alt-H") == "opt-h");
        CHECK(Guide::NormaliseKeys("Command-Shift-P") == "cmd-shift-p");
        CHECK(Guide::NormaliseKeys("Shift-click") == "shift-click");
    }

    void StripModelNoiseRemovesThinkBlocks()
    {
        CHECK(AI::StripModelNoise("<think>\nWorking it out\nstill going\n</think>\n{\"a\":1}") ==
              "{\"a\":1}");
        CHECK(AI::StripModelNoise("{\"a\":1}\n<think>never closed") == "{\"a\":1}");
    }

    void StripModelNoiseRemovesFencesAndTrims()
    {
        CHECK(AI::StripModelNoise("```json\n{\"a\":1}\n```") == "{\"a\":1}");
        CHECK(AI::StripModelNoise("```\n[1]\n```") == "[1]");
        CHECK(AI::StripModelNoise("<think>x</think>```json\n[1]\n```") == "[1]");
        CHECK(AI::StripModelNoise("  \t hello \r\n") == "hello");
        CHECK(AI::StripModelNoise(" \n\t ").empty());
    }

    void CatalogHasOneEntryPerShortcut()
    {
        const auto catalog = Guide::BuildCatalog();
        CHECK(catalog.size() == ShortcutCount());
        std::array<bool, static_cast<std::size_t>(Domain::Count)> domain_seen{};
        for (std::size_t i = 0; i < catalog.size(); ++i)
        {
            CHECK(catalog[i].id == i);
            CHECK(!catalog[i].keys.empty());
            CHECK(!catalog[i].does.empty());
            domain_seen[static_cast<std::size_t>(catalog[i].domain)] = true;
        }
        for (const bool seen : domain_seen)
        {
            CHECK(seen);
        }
    }

    void CacheKeysAreStableAndTrackTheShortcut()
    {
        const auto first = Guide::BuildCatalog();
        const auto second = Guide::BuildCatalog();
        CHECK(first.size() == second.size());
        for (std::size_t i = 0; i < first.size(); ++i)
        {
            CHECK(first[i].cacheKey == second[i].cacheKey);
        }
        // Equal cache keys exactly when domain, scope, keys and description all match.
        for (std::size_t i = 0; i < first.size(); ++i)
        {
            for (std::size_t j = i + 1; j < first.size(); ++j)
            {
                const bool same_shortcut = first[i].domain == first[j].domain &&
                                           first[i].scope == first[j].scope &&
                                           first[i].keys == first[j].keys &&
                                           first[i].does == first[j].does;
                const bool same_key = first[i].cacheKey == first[j].cacheKey;
                CHECK(same_shortcut == same_key);
            }
        }
    }

    void PhysicalKeyLabelFormatsChords()
    {
        CHECK(Guide::PhysicalKeyLabel(K::R, Platform::CONTROL | Platform::SHIFT) ==
              "Ctrl-Shift-R");
        CHECK(Guide::PhysicalKeyLabel(K::H, Platform::OPTION) == "Opt-H");
        CHECK(Guide::PhysicalKeyLabel(K::Escape, 0) == "Esc");
        CHECK(Guide::PhysicalKeyLabel(K::F5, 0) == "F5");
        CHECK(Guide::PhysicalKeyLabel(K::Semicolon, Platform::OPTION | Platform::SHIFT) ==
              "Opt-Shift-;");
        CHECK(Guide::PhysicalKeyLabel(K::Left, Platform::COMMAND) == "Cmd-Left");
        CHECK(Guide::PhysicalKeyLabel(K::Space, 0) == "Space");
    }

    void AeroSpaceDefaultsKeepServiceModeSemantics()
    {
        CHECK(AeroSpaceUsesDefaults());
        CHECK(!AeroSpaceBindings().empty());
        // Enter service from main, leave it with the join or reset bindings, keep it otherwise.
        CHECK(AeroSpaceServiceAfter(false, K::Semicolon, Platform::OPTION | Platform::SHIFT));
        CHECK(!AeroSpaceServiceAfter(false, K::H, Platform::OPTION));
        CHECK(!AeroSpaceServiceAfter(true, K::R, 0));
        CHECK(!AeroSpaceServiceAfter(true, K::H, Platform::OPTION | Platform::SHIFT));
        CHECK(AeroSpaceServiceAfter(true, K::J, 0));
    }

    void AeroSpaceCustomBindingsReplaceDefaults()
    {
        const auto default_count = AeroSpaceBindings().size();
        const std::vector<AeroSpaceConfigBinding> custom{
            {.key = K::H,
             .modifiers = Platform::OPTION,
             .shortHint = "left",
             .fullHint = "focus left",
             .command = "focus left"},
            {.key = K::S,
             .modifiers = Platform::CONTROL | Platform::OPTION,
             .entersService = true,
             .shortHint = "service",
             .fullHint = "enter service mode",
             .command = "mode service"},
            {.key = K::X,
             .service = true,
             .shortHint = "x",
             .fullHint = "close the window",
             .command = "close"},
            {.key = K::Escape,
             .service = true,
             .returnsToMain = true,
             .shortHint = "esc",
             .fullHint = "leave service mode",
             .command = "mode main"},
        };
        SetAeroSpaceBindings(custom);

        CHECK(!AeroSpaceUsesDefaults());
        const auto bindings = AeroSpaceBindings();
        CHECK(bindings.size() == custom.size());
        CHECK(bindings[0].shortHint == "left");
        CHECK(bindings[0].fullHint == "focus left");
        // "left" has no 'h', so no mnemonic is picked for H.
        CHECK(bindings[0].mnemonic == 0);
        // "x" contains the letter, so X gets the lowercase mnemonic.
        CHECK(bindings[2].mnemonic == U'x');
        CHECK(bindings[2].service && !bindings[2].returnsToMain);
        CHECK(bindings[3].service && bindings[3].returnsToMain);

        // Bound keys follow their binding; the default enter key is no longer bound.
        CHECK(AeroSpaceServiceAfter(false, K::S, Platform::CONTROL | Platform::OPTION));
        CHECK(!AeroSpaceServiceAfter(false, K::H, Platform::OPTION));
        CHECK(AeroSpaceServiceAfter(true, K::X, 0));
        CHECK(!AeroSpaceServiceAfter(true, K::Escape, 0));
        CHECK(AeroSpaceServiceAfter(true, K::R, 0));
        CHECK(!AeroSpaceServiceAfter(false, K::Semicolon, Platform::OPTION | Platform::SHIFT));

        // The catalog picks up the custom bindings, and service entries name the enter key.
        const auto catalog = Guide::BuildCatalog();
        CHECK(catalog.size() == ShortcutCount());
        const auto offset = catalog.size() - custom.size();
        CHECK(catalog[offset].domain == Domain::AeroSpace);
        CHECK(catalog[offset].keys == "Opt-H");
        CHECK(catalog[offset + 1].keys == "Ctrl-Opt-S");
        CHECK(catalog[offset + 2].keys == "X");
        CHECK(catalog[offset + 3].keys == "Esc");
        CHECK(catalog[offset].scope == "anywhere on macOS while AeroSpace runs");
        CHECK(catalog[offset + 2].scope == "AeroSpace service mode (after Ctrl-Opt-S)");
        CHECK(catalog[offset + 3].scope == "AeroSpace service mode (after Ctrl-Opt-S)");
        CHECK(catalog[offset + 2].does == "close the window");

        // An empty list restores the built-in bindings.
        RestoreDefaultAeroSpaceBindings();
        CHECK(AeroSpaceUsesDefaults());
        CHECK(AeroSpaceBindings().size() == default_count);
        CHECK(AeroSpaceServiceAfter(false, K::Semicolon, Platform::OPTION | Platform::SHIFT));
        CHECK(!AeroSpaceServiceAfter(false, K::H, Platform::OPTION));
    }

    // Review round 1: punctuation keys and space-separated chords are shortcuts too.
    void ParseGroundsPunctuationAndSpacedChords()
    {
        const std::vector<CatalogEntry> vim{MakeEntry("d", "delete operator")};
        const std::unordered_set<std::string> none;
        CHECK(ParseReply(Reply("Press Cmd-, to finish."), vim, none).rejected == 1);
        CHECK(ParseReply(Reply("Press Ctrl X to finish."), vim, none).rejected == 1);
        CHECK(ParseReply(Reply("Press Ctrl+[ to leave."), vim, none).rejected == 1);
        CHECK(ParseReply(Reply("Hold Shift F5 to go back."), vim, none).rejected == 1);
        // Ordinary prose with modifier words is not a shortcut.
        CHECK(ParseReply(Reply("Shift the text right; the command line stays."), vim, none)
                  .accepted.size() == 1);
        // Review round 2: without a separator only key notation counts.
        CHECK(ParseReply(Reply("Shift a block left to align it."), vim, none).accepted.size() ==
              1);
        CHECK(ParseReply(Reply("Control a group of windows together."), vim, none)
                  .accepted.size() == 1);
        CHECK(ParseReply(Reply("Shift right to make room."), vim, none).accepted.size() == 1);
        CHECK(ParseReply(Reply("Shift 2 lines down at once."), vim, none).accepted.size() == 1);
        CHECK(ParseReply(Reply("Shift-select several objects first."), vim, none)
                  .accepted.size() == 1);
        CHECK(ParseReply(Reply("Press Shift Tab to go back."), vim, none).rejected == 1);
        CHECK(ParseReply(Reply("Then shift-a appends."), vim, none).rejected == 1);
        CHECK(ParseReply(Reply("Press Cmd-, to finish."), vim, {"cmd-,"}).accepted.size() == 1);
        const std::vector<CatalogEntry> comma{MakeEntry("Cmd-,", "open settings")};
        CHECK(ParseReply(Reply("Use Cmd-, to tweak preferences."), comma, none)
                  .accepted.size() == 1);
    }

    // Review round 1: Zed-only shortcuts must not ground tips for shared or Neovim entries.
    void ParseUsesPerAppAllowlists()
    {
        CatalogEntry shared = MakeEntry("w", "next word");
        shared.app = "Zed and Neovim (Vim keys)";
        shared.allowGroup = "vim";
        CatalogEntry zed = MakeEntry("Cmd-p", "file finder");
        zed.allowGroup = "vim-zed";
        CatalogEntry neovim = MakeEntry("Ctrl-l", "redraw");
        neovim.app = "Neovim";
        neovim.allowGroup = "vim-neovim";
        const auto allowed = Guide::BuildAllowedKeys({shared, zed, neovim});
        const auto parse = [&](const CatalogEntry& entry, std::string_view text)
        {
            const std::vector<CatalogEntry> batch{entry};
            return Guide::ParseBatchReply(Reply(text), Pointers(batch), allowed);
        };
        CHECK(parse(shared, "Then Cmd-P opens a file.").rejected == 1);
        CHECK(parse(neovim, "Then Cmd-P opens a file.").rejected == 1);
        CHECK(parse(zed, "Afterwards Cmd-P again.").accepted.size() == 1);
        CHECK(parse(neovim, "Then Ctrl-L clears.").accepted.size() == 1);
        CHECK(parse(zed, "Then Ctrl-L clears.").rejected == 1);
        CHECK(allowed.at("vim-zed").contains("w") && allowed.at("vim-neovim").contains("w"));
        CHECK(!allowed.at("vim").contains("cmd-p"));
    }

    void NormaliseKeysKeepsPunctuationKeys()
    {
        CHECK(Guide::NormaliseKeys("Opt--") == "opt--");
        CHECK(Guide::NormaliseKeys("Ctrl-+") == "ctrl-+");
        CHECK(Guide::NormaliseKeys("Ctrl X") == "ctrl-x");
        CHECK(Guide::NormaliseKeys("-") == "-");
        CHECK(Guide::NormaliseKeys("f + char") == "f-char");
    }

    // Review round 1: stripping must stay linear for adversarial replies.
    void StripModelNoiseIsLinear()
    {
        std::string noisy;
        for (int i = 0; i < 200000; ++i)
        {
            noisy += "```x\n";
        }
        noisy += "{\"items\":[]}";
        const auto started = std::chrono::steady_clock::now();
        CHECK(AI::StripModelNoise(noisy).find("{\"items\":[]}") != std::string::npos);
        CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(1));
        CHECK(AI::StripModelNoise("<think>a</think>x<think>b</think>y") == "xy");
    }

    // Review round 1: a parsed config without bindings means "no AeroSpace hotkeys".
    void AeroSpaceEmptyConfigIsNotDefaults()
    {
        const auto defaults = AeroSpaceBindings().size();
        SetAeroSpaceBindings({});
        CHECK(AeroSpaceBindings().empty() && !AeroSpaceUsesDefaults());
        CHECK(!AeroSpaceServiceAfter(false, K::Semicolon, Platform::OPTION | Platform::SHIFT));
        RestoreDefaultAeroSpaceBindings();
        CHECK(AeroSpaceBindings().size() == defaults && AeroSpaceUsesDefaults());
    }
} // namespace

int main()
{
    BuildBatchPromptListsEveryEntry();
    ParseAcceptsItemsObject();
    ParseAcceptsBareArrayStringIdsAndExplanationField();
    ParseAcceptsJsonInProseAndFences();
    ParseRejectsInvalidItems();
    ParseRejectsNonJsonReply();
    ParseGroundsModifierShortcuts();
    SanitiseMapsTypographyToAscii();
    SanitiseDropsOtherNonAscii();
    SanitiseCollapsesWhitespace();
    SanitiseCutsLongText();
    NormaliseKeysMapsAliases();
    StripModelNoiseRemovesThinkBlocks();
    StripModelNoiseRemovesFencesAndTrims();
    CatalogHasOneEntryPerShortcut();
    CacheKeysAreStableAndTrackTheShortcut();
    PhysicalKeyLabelFormatsChords();
    AeroSpaceDefaultsKeepServiceModeSemantics();
    AeroSpaceCustomBindingsReplaceDefaults();
    ParseGroundsPunctuationAndSpacedChords();
    ParseUsesPerAppAllowlists();
    NormaliseKeysKeepsPunctuationKeys();
    StripModelNoiseIsLinear();
    AeroSpaceEmptyConfigIsNotDefaults();
    return 0;
}
