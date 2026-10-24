// guide/explanation_parser.cpp
#include "guide/explanation_parser.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <regex>
#include <nlohmann/json.hpp>

namespace
{
    using Json = nlohmann::json;

    constexpr std::string_view SYSTEM_PROMPT =
        "You are KeyGuide, a patient keyboard-shortcut tutor shown inside a shortcut overlay.\n"
        "You receive a JSON list of shortcuts. Each has an id, the app, the keys, where it "
        "works and what it does. Those facts are the only ground truth.\n"
        "For every shortcut write ONE plain-ASCII sentence of at most 90 characters for a "
        "learner: when to use it, why it helps, or a tiny concrete example.\n"
        "Rules:\n"
        "- Do not restate the keys and do not repeat the 'does' text word for word.\n"
        "- Never mention any other modifier shortcut (Ctrl/Cmd/Opt/Shift combinations).\n"
        "- If unsure, describe a typical situation where the action is useful.\n"
        "Reply with JSON only, no other text: {\"items\":[{\"id\":0,\"text\":\"...\"}]}";

    std::string Lowercase(std::string_view text)
    {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    void ReplaceAll(std::string& text, std::string_view from, std::string_view to)
    {
        for (auto position = text.find(from); position != std::string::npos;
             position = text.find(from, position + to.size()))
        {
            text.replace(position, from.size(), to);
        }
    }

    Json ParseSlice(std::string_view reply, char open, char close)
    {
        const auto start = reply.find(open);
        const auto end = reply.rfind(close);
        if (start == std::string_view::npos || end == std::string_view::npos || end < start)
        {
            return Json();
        }
        return Json::parse(reply.substr(start, end - start + 1), nullptr, false);
    }

    // Replies sometimes wrap the JSON in prose, which may itself contain brackets. Prefer the
    // requested {"items": [...]} object, then fall back to a bare array.
    Json ExtractJson(std::string_view reply)
    {
        auto whole = Json::parse(reply, nullptr, false);
        if (!whole.is_discarded() && (whole.is_object() || whole.is_array()))
        {
            return whole;
        }
        auto object = ParseSlice(reply, '{', '}');
        if (object.is_object())
        {
            return object;
        }
        auto array = ParseSlice(reply, '[', ']');
        return array.is_array() ? array : Json();
    }

    bool ReadIndex(const Json& value, std::size_t& index)
    {
        if (value.is_number_unsigned() || value.is_number_integer())
        {
            const auto number = value.get<long long>();
            if (number < 0)
            {
                return false;
            }
            index = static_cast<std::size_t>(number);
            return true;
        }
        if (value.is_string())
        {
            const auto text = value.get<std::string>();
            std::size_t number = 0;
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), number);
            if (text.empty() || error != std::errc() || end != text.data() + text.size())
            {
                return false;
            }
            index = number;
            return true;
        }
        return false;
    }

    // Modifier chords: "Ctrl-Shift-R", "cmd + k", "Cmd-," (punctuation key) and, with plain
    // spaces, "Ctrl X" when the key is a capitalised letter or named key (see MentionedChords).
    const std::regex& JoinedChordPattern()
    {
        static const std::regex chord(
            R"re(\b(?:ctrl|control|cmd|command|opt|option|alt|shift))re"
            R"re((?:\s*[-+]\s*(?:ctrl|control|cmd|command|opt|option|alt|shift))*)re"
            R"re(\s*[-+]\s*([a-z0-9]+|[^\sa-z0-9]))re",
            std::regex::icase | std::regex::optimize);
        return chord;
    }

    const std::regex& SpacedChordPattern()
    {
        static const std::regex chord(
            R"re(\b(?:ctrl|control|cmd|command|opt|option|alt|shift))re"
            R"re((?:\s+(?:ctrl|control|cmd|command|opt|option|alt|shift))*\s+)re"
            R"re(([a-z]|f[0-9]{1,2}|tab|enter|return|esc|escape|space|backspace|delete)re"
            R"re(|up|down|left|right|home|end|pageup|pagedown)(?![a-z0-9]))re",
            std::regex::icase | std::regex::optimize);
        return chord;
    }

    // Normalised chords named in `text`, skipping modifier + mouse gestures ("Shift-click").
    std::vector<std::string> MentionedChords(const std::string& text)
    {
        static const std::regex mouse(
            R"re(^(?:click|drag|scroll|wheel|lmb|mmb|rmb|select)\w*$)re",
            std::regex::icase | std::regex::optimize);
        std::vector<std::string> chords;
        for (const auto* pattern : {&JoinedChordPattern(), &SpacedChordPattern()})
        {
            for (auto match = std::sregex_iterator(text.begin(), text.end(), *pattern);
                 match != std::sregex_iterator(); ++match)
            {
                const auto key = (*match)[1].str();
                // Without a separator only key notation counts: "Ctrl X" and "Shift Tab" are
                // shortcuts, "shift a block" and "Shift right to make room" are prose.
                const bool spaced = pattern == &SpacedChordPattern();
                if ((spaced && !std::isupper(static_cast<unsigned char>(key.front()))) ||
                    std::regex_match(key, mouse))
                {
                    continue;
                }
                chords.push_back(Guide::NormaliseKeys(match->str()));
            }
        }
        return chords;
    }

    template <typename AllowedFor>
    Guide::ParsedBatch ParseReply(std::string_view reply,
                                  const std::vector<const Guide::CatalogEntry*>& batch,
                                  AllowedFor allowed_for);
} // namespace

namespace Guide
{
    BatchPrompt BuildBatchPrompt(const std::vector<const CatalogEntry*>& batch)
    {
        auto items = Json::array();
        for (std::size_t i = 0; i < batch.size(); ++i)
        {
            items.push_back({{"id", i},
                             {"app", batch[i]->app},
                             {"keys", batch[i]->keys},
                             {"where", batch[i]->scope},
                             {"does", batch[i]->does}});
        }
        return {std::string(SYSTEM_PROMPT),
                "Explain these " + std::to_string(batch.size()) +
                    " shortcuts. Return exactly one item per id.\n" + items.dump()};
    }

    std::string NormaliseKeys(std::string_view keys)
    {
        auto text = Lowercase(keys);
        ReplaceAll(text, "control", "ctrl");
        ReplaceAll(text, "command", "cmd");
        ReplaceAll(text, "option", "opt");
        ReplaceAll(text, "alt", "opt");
        std::string normalised;
        bool separator = false, symbol_in_run = false;
        for (const char c : text)
        {
            const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
            const bool symbol = c == '+' || c == '-';
            if (space || (symbol && !symbol_in_run && !normalised.empty()))
            {
                separator = !normalised.empty();
                symbol_in_run |= symbol;
                continue;
            }
            if (separator)
            {
                normalised += '-';
            }
            normalised += c;
            separator = false;
            symbol_in_run = false;
        }
        return normalised;
    }

    std::string SanitiseExplanation(std::string_view text)
    {
        std::string ascii;
        for (std::size_t i = 0; i < text.size();)
        {
            const auto c = static_cast<unsigned char>(text[i]);
            if (c < 0x80)
            {
                ascii += c == '\n' || c == '\r' || c == '\t' ? ' ' : static_cast<char>(c);
                ++i;
                continue;
            }
            const std::size_t length = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
            const auto sequence = text.substr(i, length);
            if (sequence == "‘" || sequence == "’")
            {
                ascii += '\'';
            }
            else if (sequence == "“" || sequence == "”")
            {
                ascii += '"';
            }
            else if (sequence == "–" || sequence == "—")
            {
                ascii += '-';
            }
            else if (sequence == "…")
            {
                ascii += "...";
            }
            else if (sequence == "→")
            {
                ascii += "->";
            }
            else if (sequence == " ")
            {
                ascii += ' ';
            }
            i += length;
        }
        std::string collapsed;
        for (const char c : ascii)
        {
            if (c == ' ' && (collapsed.empty() || collapsed.back() == ' '))
            {
                continue;
            }
            collapsed += c;
        }
        while (!collapsed.empty() && collapsed.back() == ' ')
        {
            collapsed.pop_back();
        }
        if (collapsed.size() > EXPLANATION_MAX_LENGTH)
        {
            auto cut = collapsed.rfind(' ', EXPLANATION_MAX_LENGTH - 2);
            if (cut == std::string::npos || cut < EXPLANATION_MAX_LENGTH / 2)
            {
                cut = EXPLANATION_MAX_LENGTH - 2;
            }
            collapsed = collapsed.substr(0, cut) + "..";
        }
        return collapsed;
    }

    bool MentionsUnknownShortcut(std::string_view text, const CatalogEntry& entry,
                                 const std::unordered_set<std::string>& known_keys)
    {
        // Whole chords only: "Shift-R" is not part of "Ctrl-Shift-R".
        std::unordered_set<std::string> own_chords{NormaliseKeys(entry.keys)};
        for (auto& chord : MentionedChords(entry.keys))
        {
            own_chords.insert(std::move(chord));
        }
        for (const auto& mentioned : MentionedChords(std::string(text)))
        {
            if (!own_chords.contains(mentioned) && !known_keys.contains(mentioned))
            {
                return true;
            }
        }
        return false;
    }

    AllowedKeys BuildAllowedKeys(const std::vector<CatalogEntry>& catalog)
    {
        AllowedKeys allowed;
        for (const auto& entry : catalog)
        {
            const auto keys = NormaliseKeys(entry.keys);
            allowed[entry.allowGroup].insert(keys);
            if (entry.allowGroup == "vim")
            {
                allowed["vim-zed"].insert(keys);
                allowed["vim-neovim"].insert(keys);
            }
        }
        return allowed;
    }

    ParsedBatch ParseBatchReply(std::string_view reply,
                                const std::vector<const CatalogEntry*>& batch,
                                const AllowedKeys& allowed)
    {
        static const std::unordered_set<std::string> none;
        return ParseReply(reply, batch,
                          [&](const CatalogEntry& entry) -> const std::unordered_set<std::string>&
                          {
                              const auto found = allowed.find(entry.allowGroup);
                              return found == allowed.end() ? none : found->second;
                          });
    }

    ParsedBatch ParseBatchReply(std::string_view reply,
                                const std::vector<const CatalogEntry*>& batch,
                                const std::unordered_set<std::string>& known_keys)
    {
        return ParseReply(reply, batch,
                          [&](const CatalogEntry&) -> const std::unordered_set<std::string>&
                          { return known_keys; });
    }
} // namespace Guide

namespace
{
    template <typename AllowedFor>
    Guide::ParsedBatch ParseReply(std::string_view reply,
                                  const std::vector<const Guide::CatalogEntry*>& batch,
                                  AllowedFor allowed_for)
    {
        using namespace Guide;
        ParsedBatch parsed;
        const auto root = ExtractJson(reply);
        const Json* items = nullptr;
        if (root.is_object() && root.contains("items") && root["items"].is_array())
        {
            items = &root["items"];
        }
        else if (root.is_array())
        {
            items = &root;
        }
        if (!items)
        {
            parsed.error = "The model did not return the expected JSON";
            return parsed;
        }
        std::vector<bool> seen(batch.size(), false);
        for (const auto& item : *items)
        {
            if (!item.is_object() || !item.contains("id"))
            {
                ++parsed.rejected;
                continue;
            }
            std::size_t index = 0;
            const auto text_field = item.contains("text")          ? item.find("text")
                                    : item.contains("explanation") ? item.find("explanation")
                                                                   : item.end();
            if (!ReadIndex(item["id"], index) || index >= batch.size() || seen[index] ||
                text_field == item.end() || !text_field->is_string())
            {
                ++parsed.rejected;
                continue;
            }
            const auto text = SanitiseExplanation(text_field->get<std::string>());
            if (text.empty() ||
                MentionsUnknownShortcut(text, *batch[index], allowed_for(*batch[index])))
            {
                ++parsed.rejected;
                continue;
            }
            seen[index] = true;
            parsed.accepted.push_back({index, text});
        }
        return parsed;
    }
} // namespace
