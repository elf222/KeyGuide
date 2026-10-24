// gameLogic/vim_bindings.cpp
#include "gameLogic/vim_bindings.hpp"
#include <vector>
#include "gameLogic/vim_ex_commands.hpp"

namespace
{
    bool IsVisual(VimMode mode)
    {
        return mode == VimMode::Visual || mode == VimMode::VisualLine ||
               mode == VimMode::VisualBlock;
    }
} // namespace

std::span<const VimBinding> VimBindings()
{
    static const std::vector<VimBinding> bindings = []
    {
        std::vector<VimBinding> result;
        const auto add = [&](char32_t key, BindingScope scope, BindingAction action,
                             std::string_view small, std::string_view full, std::string_view chord,
                             std::uint8_t modifiers = 0,
                             std::optional<Platform::AppProfile> profile = {})
        {
            if ((key >= U'A' && key <= U'Z') ||
                std::u32string_view(U"~!@#$%^&*()_+{}|:\"<>?").find(key) !=
                    std::u32string_view::npos)
            {
                modifiers |= Platform::SHIFT;
            }
            result.push_back({{key}, modifiers, scope, action, small, full, chord, key, profile});
        };
        const auto addPending = [&](char32_t key, BindingScope scope, BindingAction action,
                                    std::string_view pendingOperator, std::string_view small,
                                    std::string_view full, std::string_view chord,
                                    std::optional<Platform::AppProfile> profile = {})
        {
            add(key, scope, action, small, full, chord, 0, profile);
            result.back().pendingOperator = pendingOperator;
        };
        using S = BindingScope;
        using A = BindingAction;
        using P = Platform::AppProfile;
        add(U'h', S::Motion, A::Motion, "left", "move left", "h");
        add(U'j', S::Motion, A::Motion, "down", "move down", "j");
        add(U'k', S::Motion, A::Motion, "up", "move up", "k");
        add(U'l', S::Motion, A::Motion, "right", "move right", "l");
        add(U'w', S::Motion, A::Motion, "word", "next word", "w");
        add(U'W', S::Motion, A::Motion, "WORD", "next WORD", "W");
        add(U'b', S::Motion, A::Motion, "back", "previous word", "b");
        add(U'B', S::Motion, A::Motion, "Back", "previous WORD", "B");
        add(U'e', S::Motion, A::Motion, "end", "word end", "e");
        add(U'E', S::Motion, A::Motion, "End", "WORD end", "E");
        add(U'0', S::Motion, A::Motion, "start", "line start", "0");
        add(U'^', S::Motion, A::Motion, "first", "first nonblank", "^");
        add(U'$', S::Motion, A::Motion, "end", "line end", "$");
        add(U'G', S::Motion, A::Motion, "Go", "last / counted line", "G");
        add(U'%', S::Motion, A::Motion, "pair", "matching bracket / quote", "%");
        add(U'{', S::Motion, A::Motion, "back", "previous paragraph", "{");
        add(U'}', S::Motion, A::Motion, "next", "next paragraph", "}");
        add(U'f', S::Motion, A::Find, "find", "find character forward", "f + char");
        add(U'F', S::Motion, A::Find, "Find", "find character backward", "F + char");
        add(U't', S::Motion, A::Find, "till", "till character forward", "t + char");
        add(U'T', S::Motion, A::Find, "Till", "till character backward", "T + char");
        add(U';', S::Motion, A::Motion, "again", "repeat character search", ";");
        add(U',', S::Motion, A::Motion, "reverse", "reverse character search", ",");
        add(U'n', S::Motion, A::Motion, "next", "next search result", "n");
        add(U'N', S::Motion, A::Motion, "Next", "previous search result", "N");
        add(U'/', S::Motion, A::Search, "search", "search forward", "/");
        add(U'?', S::Motion, A::Search, "search", "search backward", "?");
        for (char32_t c : std::u32string_view(U"123456789"))
        {
            add(c, S::Motion, A::Count, "count", "command / motion count", "count");
        }
        add(U'i', S::Normal, A::Insert, "insert", "insert before cursor", "i");
        add(U'I', S::Normal, A::Insert, "Insert", "insert at first nonblank", "I");
        add(U'a', S::Normal, A::Insert, "append", "append after cursor", "a");
        add(U'A', S::Normal, A::Insert, "Append", "append at line end", "A");
        add(U'I', S::VisualBlock, A::Insert, "Insert", "insert at the start of each block row",
            "I");
        add(U'A', S::VisualBlock, A::Insert, "Append", "append at the end of each block row", "A");
        add(U'I', S::Visual, A::Insert, "Insert", "insert before each selection", "I", 0, P::Zed);
        add(U'A', S::Visual, A::Insert, "Append", "append after each selection", "A", 0, P::Zed);
        add(U'I', S::Visual, A::Insert, "Insert", "insert at first nonblank", "I", 0, P::Neovim);
        add(U'A', S::Visual, A::Insert, "Append", "append at line end", "A", 0, P::Neovim);
        add(U'o', S::Visual, A::KeepVisual, "other", "move to the other selection end", "o");
        add(U'O', S::Visual, A::KeepVisual, "other", "move to the other selection corner", "O");
        add(U'o', S::Normal, A::Insert, "open", "open line below", "o");
        add(U'O', S::Normal, A::Insert, "Open", "open line above", "O");
        add(U's', S::NormalVisual, A::Insert, "sub", "substitute selection / char", "s");
        add(U'S', S::NormalVisual, A::Insert, "Sub", "substitute line", "S");
        add(U'C', S::Normal, A::Insert, "Change", "change to line end", "C");
        add(U'C', S::Visual, A::Insert, "Change", "change selected lines", "C", 0, P::Neovim);
        add(U'R', S::Normal, A::Replace, "Replace", "enter Replace mode", "R");
        add(U'r', S::NormalVisual, A::ReplaceCharacter, "replace", "replace with character",
            "r + char");
        add(U'v', S::NormalVisual, A::Visual, "visual", "character selection", "v");
        add(U'V', S::NormalVisual, A::VisualLine, "Visual", "line selection", "V");
        add(U'v', S::NormalVisual, A::VisualBlock, "visual", "block selection", "Ctrl-v",
            Platform::CONTROL);
        add(U'q', S::NormalVisual, A::VisualBlock, "visual", "block selection", "Ctrl-q",
            Platform::CONTROL, P::Zed);
        add(U'd', S::NormalVisual, A::Operator, "del", "delete", "d + motion / dd");
        add(U'y', S::NormalVisual, A::Operator, "yank", "yank", "y + motion / yy");
        add(U'c', S::NormalVisual, A::Operator, "change", "change", "c + motion / cc");
        add(U'>', S::NormalVisual, A::Operator, "indent", "indent", " > + motion / >>");
        add(U'<', S::NormalVisual, A::Operator, "outdent", "outdent", "< + motion / <<");
        add(U'=', S::NormalVisual, A::Operator, "indent", "auto-indent motion", "= + motion / ==");
        add(U'D', S::NormalVisual, A::Complete, "Delete", "delete to line end", "D");
        add(U'Y', S::NormalVisual, A::Complete, "Yank", "yank line", "Y", 0, P::Zed);
        add(U'Y', S::Normal, A::Complete, "Yank", "yank to line end", "Y", 0, P::Neovim);
        add(U'Y', S::Visual, A::Complete, "Yank", "yank selected lines", "Y", 0, P::Neovim);
        add(U'x', S::NormalVisual, A::Complete, "delete", "delete character / selection", "x");
        add(U'X', S::Normal, A::Complete, "delete", "delete previous character", "X");
        add(U'X', S::Visual, A::Complete, "delete", "delete selected lines", "X");
        add(U'u', S::Normal, A::Complete, "undo", "undo", "u");
        add(U'U', S::Normal, A::Complete, "Undo", "undo last line", "U");
        add(U'u', S::Visual, A::Complete, "lower", "lowercase selection", "u");
        add(U'U', S::Visual, A::Complete, "Upper", "uppercase selection", "U");
        add(U'r', S::Normal, A::Complete, "redo", "redo", "Ctrl-r", Platform::CONTROL);
        add(U'p', S::NormalVisual, A::Complete, "paste", "paste after", "p");
        add(U'P', S::NormalVisual, A::Complete, "Paste", "paste before", "P");
        add(U'J', S::NormalVisual, A::Complete, "Join", "join lines", "J");
        add(U'~', S::NormalVisual, A::Complete, "case", "toggle case", "~");
        add(U':', S::NormalVisual, A::Palette, "command", "command palette", ":", 0, P::Zed);
        add(U':', S::NormalVisual, A::Palette, "command", "command line", ":", 0, P::Neovim);
        add(U'"', S::NormalVisual, A::Register, "register", "select register", "\" + register");
        add(U'm', S::Normal, A::MarkSet, "mark", "set mark", "m + mark");
        add(U'\'', S::Motion, A::MarkJump, "mark", "jump to mark line", "' + mark");
        add(U'`', S::Motion, A::MarkJump, "mark", "jump to mark position", "` + mark");
        add(U'q', S::Normal, A::MacroRecord, "record", "record / stop macro", "q + register");
        add(U'@', S::NormalVisual, A::MacroPlay, "macro", "play macro (resync needed)",
            "@ + register");
        add(U'Q', S::NormalVisual, A::Unknown, "replay", "replay last macro (resync needed)", "Q");
        add(U'K', S::NormalVisual, A::KeepVisual, "hover", "show hover documentation", "K");
        add(U'&', S::Normal, A::Complete, "again", "repeat last :s", "&", 0, P::Neovim);
        add(U'l', S::Normal, A::Complete, "redraw", "redraw; clear search highlight", "Ctrl-l",
            Platform::CONTROL, P::Neovim);
        add(U'v', S::Operator, A::ForcedMotion, "charwise", "force the motion charwise",
            "operator + v", 0, P::Neovim);
        add(U'V', S::Operator, A::ForcedMotion, "linewise", "force the motion linewise",
            "operator + V", 0, P::Neovim);
        add(U'v', S::Operator, A::ForcedMotion, "blockwise", "force the motion blockwise",
            "operator + Ctrl-v", Platform::CONTROL, P::Neovim);
        add(U'.', S::Normal, A::Unknown, "repeat", "repeat edit (resync needed)", ".");
        add(U'g', S::Motion, A::PrefixG, "go", "g command prefix", "g + command");
        add(U'[', S::Motion, A::PrefixBracketOpen, "back", "[ motion prefix", "[ + motion");
        add(U']', S::Motion, A::PrefixBracketClose, "next", "] motion prefix", "] + motion");
        add(U'z', S::NormalVisual, A::PrefixZ, "view", "z view prefix", "z + command");
        add(U'Z', S::Normal, A::PrefixUpperZ, "close", "Z close prefix", "Z + command", 0,
            P::Neovim);
        add(U'Z', S::NormalVisual, A::PrefixUpperZ, "close", "Z close prefix", "Z + command", 0,
            P::Zed);
        add(U'w', S::Normal, A::PrefixWindow, "window", "window command prefix", "Ctrl-w",
            Platform::CONTROL);
        add(U'o', S::Insert, A::TemporaryNormal, "normal", "one Normal command", "Ctrl-o",
            Platform::CONTROL);
        add(U'v', S::Insert, A::Literal, "literal", "insert next key literally", "Ctrl-v",
            Platform::CONTROL);
        add(U'w', S::Insert, A::Complete, "word", "delete previous word", "Ctrl-w",
            Platform::CONTROL);
        add(U'u', S::Insert, A::Complete, "line", "delete to line beginning", "Ctrl-u",
            Platform::CONTROL, P::Zed);
        add(U'u', S::Insert, A::Complete, "line", "delete inserted text before cursor on this line",
            "Ctrl-u", Platform::CONTROL, P::Neovim);
        add(U'h', S::Insert, A::Complete, "back", "backspace", "Ctrl-h", Platform::CONTROL);
        add(U'b', S::Insert, A::Complete, "left", "move left", "Ctrl-b", Platform::CONTROL, P::Zed);
        add(U'r', S::Insert, A::Register, "register", "insert from register", "Ctrl-r + register",
            Platform::CONTROL);
        add(U'q', S::Insert, A::Literal, "literal", "insert next key literally", "Ctrl-q",
            Platform::CONTROL);
        add(U'k', S::Insert, A::Digraph, "digraph", "enter a digraph from two keys",
            "Ctrl-k + 2 keys", Platform::CONTROL);
        add(U'v', S::Replace, A::Literal, "literal", "insert next key literally", "Ctrl-v",
            Platform::CONTROL);
        add(U'q', S::Replace, A::Literal, "literal", "insert next key literally", "Ctrl-q",
            Platform::CONTROL);
        add(U'k', S::Replace, A::Digraph, "digraph", "enter a digraph from two keys",
            "Ctrl-k + 2 keys", Platform::CONTROL);
        add(U'n', S::Insert, A::Complete, "next", "next word completion", "Ctrl-n",
            Platform::CONTROL, P::Zed);
        add(U'p', S::Insert, A::Complete, "previous", "previous word completion", "Ctrl-p",
            Platform::CONTROL, P::Zed);
        add(U't', S::Insert, A::Complete, "indent", "increase indent", "Ctrl-t", Platform::CONTROL,
            P::Zed);
        add(U'd', S::Insert, A::Complete, "indent", "decrease indent", "Ctrl-d", Platform::CONTROL,
            P::Zed);
        add(U'y', S::Insert, A::Complete, "above", "insert character from above", "Ctrl-y",
            Platform::CONTROL, P::Zed);
        add(U'e', S::Insert, A::Complete, "below", "insert character from below", "Ctrl-e",
            Platform::CONTROL, P::Zed);
        add(U'a', S::Insert, A::Complete, "start", "move to line start", "Ctrl-a",
            Platform::CONTROL, P::Zed);
        add(U'f', S::Insert, A::Complete, "right", "move right", "Ctrl-f", Platform::CONTROL,
            P::Zed);
        add(U's', S::Insert, A::Complete, "signature", "show signature help", "Ctrl-s",
            Platform::CONTROL, P::Zed);
        add(U'l', S::Insert, A::Complete, "center", "scroll cursor to center", "Ctrl-l",
            Platform::CONTROL, P::Zed);
        add(U'x', S::Insert, A::PrefixCtrlX, "prefix", "Ctrl-x command prefix; one key follows",
            "Ctrl-x + key", Platform::CONTROL, P::Zed);
        add(U'o', S::CtrlX, A::Complete, "complete", "show completions", "Ctrl-x Ctrl-o",
            Platform::CONTROL, P::Zed);
        add(U'c', S::CtrlX, A::Complete, "predict", "show edit prediction", "Ctrl-x Ctrl-c",
            Platform::CONTROL, P::Zed);
        add(U'l', S::CtrlX, A::Complete, "actions", "code actions menu", "Ctrl-x Ctrl-l",
            Platform::CONTROL, P::Zed);
        add(U'z', S::CtrlX, A::Complete, "cancel", "stop completion; text unchanged",
            "Ctrl-x Ctrl-z", Platform::CONTROL, P::Zed);
        add(U'e', S::CtrlX, A::Complete, "down", "move one line down", "Ctrl-x Ctrl-e",
            Platform::CONTROL, P::Zed);
        add(U'y', S::CtrlX, A::Complete, "up", "move one line up", "Ctrl-x Ctrl-y",
            Platform::CONTROL, P::Zed);
        add(U'a', S::CtrlX, A::Unknown, "assist", "inline assistant; focus moves", "Ctrl-x Ctrl-a",
            Platform::CONTROL, P::Zed);
        add(U'R', S::Visual, A::Insert, "Replace", "replace selected lines with inserted text",
            "R");
        add(U'w', S::Replace, A::Complete, "word", "delete previous word", "Ctrl-w",
            Platform::CONTROL, P::Neovim);
        add(U'u', S::Replace, A::Complete, "line",
            "delete inserted text before cursor on this line", "Ctrl-u", Platform::CONTROL,
            P::Neovim);
        add(U'h', S::Replace, A::Complete, "back", "backspace", "Ctrl-h", Platform::CONTROL,
            P::Neovim);
        add(U'r', S::Replace, A::Register, "register", "insert from register", "Ctrl-r + register",
            Platform::CONTROL, P::Neovim);
        add(U'i', S::Operator, A::TextObject, "inner", "inner text object", "i + object");
        add(U'a', S::Operator, A::TextObject, "around", "around text object", "a + object");
        for (char32_t c : std::u32string_view(U"dyc><="))
        {
            add(c, S::Operator, A::Motion, "line", "repeat operator on line", "operator twice");
        }
        addPending(U'u', S::Operator, A::Motion, "gu", "lower", "lowercase line", "guu");
        addPending(U'u', S::G, A::Motion, "gu", "lower", "lowercase line", "gugu");
        addPending(U'U', S::Operator, A::Motion, "gU", "Upper", "uppercase line", "gUU");
        addPending(U'U', S::G, A::Motion, "gU", "Upper", "uppercase line", "gUgU");
        addPending(U'~', S::Operator, A::Motion, "g~", "case", "toggle case of line", "g~~");
        addPending(U'~', S::G, A::Motion, "g~", "case", "toggle case of line", "g~g~");
        addPending(U'?', S::Operator, A::Motion, "g?", "rot13", "rot13 encode line", "g??");
        addPending(U'?', S::G, A::Motion, "g?", "rot13", "rot13 encode line", "g?g?");
        addPending(U'c', S::Operator, A::Motion, "gc", "comment", "comment line", "gcc");
        addPending(U'c', S::G, A::Motion, "gc", "comment", "comment block", "gcgc");
        addPending(U'q', S::Operator, A::Motion, "gq", "format", "format line", "gqq");
        addPending(U'q', S::G, A::Motion, "gq", "format", "format line", "gqgq");
        addPending(U'w', S::Operator, A::Motion, "gw", "format", "format line; keep cursor", "gww");
        addPending(U'w', S::G, A::Motion, "gw", "format", "format line; keep cursor", "gwgw");
        addPending(U'w', S::Operator, A::Motion, "gq", "format", "format line", "gqw", P::Zed);
        addPending(U'w', S::G, A::Motion, "gq", "format", "format line", "gqgw", P::Zed);
        addPending(U'q', S::Operator, A::Motion, "gw", "format", "format line", "gwq", P::Zed);
        addPending(U'q', S::G, A::Motion, "gw", "format", "format line", "gwgq", P::Zed);
        addPending(U'c', S::Operator, A::Motion, "gb", "block", "block-comment line", "gbc",
                   P::Zed);
        addPending(U'R', S::Operator, A::Motion, "gR", "replace", "replace line with register",
                   "gRR", P::Zed);
        addPending(U'r', S::Operator, A::Motion, "gR", "replace", "replace line with register",
                   "gRr", P::Zed);
        addPending(U'x', S::Operator, A::Motion, "cx", "exchange", "exchange line", "cxx", P::Zed);
        addPending(U'c', S::Operator, A::Complete, "cx", "clear", "clear pending exchange", "cxc",
                   P::Zed);
        addPending(U's', S::Operator, A::Motion, "ys", "surround", "surround line", "yss", P::Zed);
        addPending(U's', S::Operator, A::Operator, "y", "surround",
                   "add surround: ys + motion + char", "ys", P::Zed);
        addPending(U's', S::Operator, A::Operator, "d", "surround", "delete surround: ds + char",
                   "ds", P::Zed);
        addPending(U's', S::Operator, A::Operator, "c", "surround",
                   "change surround: cs + two chars", "cs", P::Zed);
        addPending(U'x', S::Operator, A::Operator, "c", "exchange", "exchange motion: cx + motion",
                   "cx", P::Zed);
        addPending(U'd', S::Operator, A::Palette, "c", "rename", "rename symbol", "cd", P::Zed);
        addPending(U'v', S::Operator, A::ForcedMotion, "d", "charwise", "force the motion charwise",
                   "d + v", P::Zed);
        addPending(U'v', S::Operator, A::ForcedMotion, "y", "charwise", "force the motion charwise",
                   "y + v", P::Zed);
        addPending(U'o', S::Operator, A::Complete, "d", "hunk", "toggle selected diff hunks", "do",
                   P::Zed);
        addPending(U'O', S::Operator, A::Complete, "d", "staged", "toggle staged for hunk", "dO",
                   P::Zed);
        addPending(U'p', S::Operator, A::Complete, "d", "restore", "restore the hunk", "dp",
                   P::Zed);
        addPending(U'u', S::Operator, A::Complete, "d", "stage", "stage hunk; go to next", "du",
                   P::Zed);
        addPending(U'U', S::Operator, A::Complete, "d", "unstage", "unstage hunk; go to next", "dU",
                   P::Zed);
        add(U'I', S::VisualG, A::Insert, "Insert",
            "insert at the first nonblank of every selected line", "gI", 0, P::Zed);
        add(U'A', S::VisualG, A::Insert, "Append", "insert at the end of every selected line", "gA",
            0, P::Zed);
        add(U'J', S::VisualG, A::Complete, "join", "join selected lines without spaces", "gJ");
        add(U'q', S::VisualG, A::Complete, "format", "format selected lines", "gq");
        add(U'w', S::VisualG, A::Complete, "format", "format selected lines", "gw");
        add(U'a', S::VisualG, A::Complete, "number", "increment numbers across selected lines",
            "g Ctrl-a", Platform::CONTROL);
        add(U'x', S::VisualG, A::Complete, "number", "decrement numbers across selected lines",
            "g Ctrl-x", Platform::CONTROL);
        add(U'c', S::VisualG, A::Complete, "comment", "toggle comments on selected lines", "gc");
        add(U'b', S::VisualG, A::Complete, "block", "toggle block comment on selection", "gb", 0,
            P::Zed);
        add(U'?', S::VisualG, A::Complete, "rot13", "rot13 encode selected text", "g?");
        add(U'R', S::VisualG, A::Complete, "paste", "replace selection with register", "gR", 0,
            P::Zed);
        add(U'a', S::Normal, A::Complete, "number", "increment number at cursor", "Ctrl-a",
            Platform::CONTROL);
        add(U'x', S::Normal, A::Complete, "number", "decrement number at cursor", "Ctrl-x",
            Platform::CONTROL);
        add(U'a', S::Visual, A::Complete, "number", "increment selected numbers", "Ctrl-a",
            Platform::CONTROL);
        add(U'x', S::Visual, A::Complete, "number", "decrement selected numbers", "Ctrl-x",
            Platform::CONTROL);
        add(U'l', S::G, A::SelectMultiple, "next", "add the next occurrence to the selections",
            "gl", 0, P::Zed);
        add(U'L', S::G, A::SelectMultiple, "previous",
            "add the previous occurrence to the selections", "gL", 0, P::Zed);
        add(U'a', S::G, A::SelectMultiple, "all", "select all occurrences", "ga", 0, P::Zed);
        add(U'>', S::G, A::SelectMultiple, "skip", "skip to the next occurrence", "g>", 0, P::Zed);
        add(U'<', S::G, A::SelectMultiple, "previous", "skip to the previous occurrence", "g<", 0,
            P::Zed);
        add(U'n', S::G, A::SelectMatch, "next", "select the next search match", "gn");
        add(U'N', S::G, A::SelectMatch, "previous", "select the previous search match", "gN");
        add(U'd', S::Cursor, A::SelectMultiple, "next", "select the next occurrence", "Cmd-d",
            Platform::COMMAND, P::Zed);
        add(U'd', S::Cursor, A::SelectMultiple, "previous", "select the previous occurrence",
            "Cmd-Ctrl-d", Platform::COMMAND | Platform::CONTROL, P::Zed);
        add(U'L', S::Cursor, A::SelectMultiple, "all", "select all occurrences", "Cmd-Shift-l",
            Platform::COMMAND, P::Zed);
        add(U'p', S::Cursor, A::KeepVisual, "above", "add a cursor above", "Cmd-Ctrl-p",
            Platform::COMMAND | Platform::CONTROL, P::Zed);
        add(U'n', S::Cursor, A::KeepVisual, "below", "add a cursor below", "Cmd-Ctrl-n",
            Platform::COMMAND | Platform::CONTROL, P::Zed);
        result.push_back({{0, Platform::SpecialKey::Up},
                          Platform::COMMAND | Platform::OPTION,
                          S::Cursor,
                          A::KeepVisual,
                          "above",
                          "add a cursor above",
                          "Cmd-Option-Up",
                          0,
                          P::Zed});
        result.push_back({{0, Platform::SpecialKey::Down},
                          Platform::COMMAND | Platform::OPTION,
                          S::Cursor,
                          A::KeepVisual,
                          "below",
                          "add a cursor below",
                          "Cmd-Option-Down",
                          0,
                          P::Zed});
        for (const auto scope : {S::NormalVisual, S::Insert, S::Replace})
        {
            add(U's', scope, A::KeepVisual, "save", "save file", "Cmd-s", Platform::COMMAND,
                P::Zed);
            add(U's', scope, A::KeepVisual, "save all", "save all files", "Cmd-Option-s",
                Platform::COMMAND | Platform::OPTION, P::Zed);
            add(U'c', scope, A::KeepVisual, "copy", "copy selection", "Cmd-c", Platform::COMMAND,
                P::Zed);
            add(U'/', scope, A::KeepVisual, "comment", "toggle comments", "Cmd-/",
                Platform::COMMAND, P::Zed);
            add(U'w', scope, A::Navigation, "close", "close tab; assume Normal", "Cmd-w",
                Platform::COMMAND, P::Zed);
        }
        for (const auto scope : {S::Normal, S::Insert, S::Replace})
        {
            add(U'z', scope, A::KeepVisual, "undo", "undo", "Cmd-z", Platform::COMMAND, P::Zed);
            add(U'Z', scope, A::KeepVisual, "redo", "redo", "Cmd-Shift-z", Platform::COMMAND,
                P::Zed);
            add(U'x', scope, A::KeepVisual, "cut", "cut selection", "Cmd-x", Platform::COMMAND,
                P::Zed);
            add(U'v', scope, A::KeepVisual, "paste", "paste", "Cmd-v", Platform::COMMAND, P::Zed);
        }
        add(U'a', S::NormalVisual, A::SelectAll, "all", "select all; Visual", "Cmd-a",
            Platform::COMMAND, P::Zed);
        add(U'a', S::Insert, A::KeepVisual, "all", "select all; stays Insert", "Cmd-a",
            Platform::COMMAND, P::Zed);
        add(U'f', S::NormalVisual, A::Search, "find", "buffer search; Enter / Esc returns", "Cmd-f",
            Platform::COMMAND, P::Zed);
        add(U'p', S::NormalVisual, A::Palette, "file", "file finder", "Cmd-p", Platform::COMMAND,
            P::Zed);
        add(U'P', S::NormalVisual, A::Palette, "command", "command palette", "Cmd-Shift-p",
            Platform::COMMAND, P::Zed);
        add(U'g', S::G, A::Motion, "go", "first / counted line", "gg");
        add(U'e', S::G, A::Motion, "end", "previous word end", "ge");
        add(U'E', S::G, A::Motion, "End", "previous WORD end", "gE");
        add(U'j', S::G, A::Motion, "down", "next display line", "gj");
        add(U'k', S::G, A::Motion, "up", "previous display line", "gk");
        add(U'u', S::G, A::Operator, "lower", "lowercase motion", "gu + motion");
        add(U'U', S::G, A::Operator, "Upper", "uppercase motion", "gU + motion");
        add(U'~', S::G, A::Operator, "case", "toggle case motion", "g~ + motion");
        add(U'd', S::G, A::Motion, "def", "go to definition", "gd");
        add(U'?', S::G, A::Operator, "rot13", "rot13 motion", "g? + motion");
        add(U'q', S::G, A::Operator, "format", "format motion", "gq + motion");
        add(U'w', S::G, A::Operator, "format", "format motion; keep cursor", "gw + motion");
        add(U'c', S::G, A::Operator, "comment", "toggle comments motion", "gc + motion");
        add(U'b', S::G, A::Operator, "block", "toggle block comment motion", "gb + motion", 0,
            P::Zed);
        add(U'R', S::G, A::Operator, "register", "replace motion with register", "gR + motion", 0,
            P::Zed);
        add(U'_', S::G, A::Motion, "end", "last non-blank of line", "g_");
        add(U'0', S::G, A::Motion, "start", "screen line start", "g0");
        add(U'$', S::G, A::Motion, "end", "screen line end", "g$");
        add(U'^', S::G, A::Motion, "first", "screen line first nonblank", "g^");
        add(U'M', S::G, A::Motion, "middle", "middle of the line", "gM");
        add(U'*', S::G, A::Motion, "match", "partial word match forward", "g*");
        add(U'#', S::G, A::Motion, "match", "partial word match backward", "g#");
        add(U',', S::G, A::Motion, "newer", "newer change position", "g,");
        add(U';', S::G, A::Motion, "older", "older change position", "g;");
        add(U'h', S::G, A::KeepVisual, "hover", "show hover information", "gh", 0, P::Zed);
        add(U'B', S::G, A::KeepVisual, "blame", "show blame hover", "gB", 0, P::Zed);
        add(U'D', S::G, A::KeepVisual, "decl", "go to declaration", "gD", 0, P::Zed);
        add(U'y', S::G, A::KeepVisual, "type", "go to type definition", "gy", 0, P::Zed);
        add(U'x', S::G, A::KeepVisual, "url", "open URL under cursor", "gx", 0, P::Zed);
        add(U'[', S::G, A::KeepVisual, "diag", "previous diagnostic", "g [", 0, P::Zed);
        add(U']', S::G, A::KeepVisual, "diag", "next diagnostic", "g ]", 0, P::Zed);
        add(U'f', S::G, A::Navigation, "file", "open file under cursor; assume Normal", "gf", 0,
            P::Zed);
        add(U' ', S::G, A::Navigation, "excerpts", "open excerpts; assume Normal", "g Space", 0,
            P::Zed);
        add(U't', S::G, A::Navigation, "tab", "next tab; assume Normal", "gt", 0, P::Zed);
        add(U'T', S::G, A::Navigation, "Tab", "previous tab; assume Normal", "gT", 0, P::Zed);
        add(U's', S::G, A::Palette, "outline", "outline picker", "gs", 0, P::Zed);
        add(U'S', S::G, A::Palette, "symbols", "project symbols picker", "gS", 0, P::Zed);
        add(U'O', S::G, A::Palette, "outline", "outline picker", "gO", 0, P::Zed);
        add(U'O', S::G, A::Unknown, "outline", "outline opens a location list; resync needed", "gO",
            0, P::Neovim);
        add(U'.', S::G, A::Palette, "actions", "code actions menu", "g .", 0, P::Zed);
        add(U'/', S::G, A::Palette, "search", "project search panel", "g /", 0, P::Zed);
        add(U'h', S::NormalG, A::Select, "select", "select characters", "gh", 0, P::Neovim);
        add(U'H', S::NormalG, A::SelectLine, "Select", "select lines", "gH", 0, P::Neovim);
        add(U'h', S::NormalG, A::SelectBlock, "select", "select block", "g Ctrl-h",
            Platform::CONTROL, P::Neovim);
        add(U'g', S::Visual, A::SelectToggle, "select", "switch to Select mode", "Ctrl-g",
            Platform::CONTROL, P::Neovim);
        add(U'g', S::Select, A::SelectToggle, "visual", "switch to Visual mode", "Ctrl-g",
            Platform::CONTROL, P::Neovim);
        add(U'R', S::NormalG, A::VirtualReplace, "VReplace", "enter Virtual Replace mode", "gR", 0,
            P::Neovim);
        add(U'Q', S::NormalG, A::ExMode, "Ex", "Ex mode; :visual returns to Normal", "gQ", 0,
            P::Neovim);
        add(U'J', S::NormalG, A::Complete, "join", "join lines without spaces", "gJ");
        add(U'i', S::NormalG, A::Insert, "insert", "insert at the last insert position", "gi");
        add(U'I', S::NormalG, A::Insert, "Insert", "insert in column 1", "gI", 0, P::Neovim);
        add(U'I', S::NormalG, A::KeepVisual, "impl", "go to implementation", "gI", 0, P::Zed);
        add(U'v', S::NormalG, A::RestoreVisual, "reselect", "reselect the last Visual area", "gv");
        add(U'A', S::NormalG, A::Navigation, "refs", "find all references; assume Normal", "gA", 0,
            P::Zed);
        for (const auto profile : {P::Zed, P::Neovim})
        {
            add(U'r', S::G, A::PrefixLsp, "LSP", "LSP command prefix; one key follows", "gr + key",
                0, profile);
            add(U'n', S::Lsp, A::Complete, "rename", "LSP rename symbol", "grn", 0, profile);
            add(U'a', S::Lsp, A::Complete, "action", "LSP code action", "gra", 0, profile);
            add(U'r', S::Lsp, A::Complete, "refs", "LSP references", "grr", 0, profile);
            add(U'i', S::Lsp, A::Complete, "impl", "LSP implementation", "gri", 0, profile);
        }
        add(U't', S::Lsp, A::Complete, "type", "LSP type definition", "grt", 0, P::Neovim);
        add(U'x', S::Lsp, A::Complete, "lens", "LSP run codelens", "grx", 0, P::Neovim);
        for (char32_t c : std::u32string_view(U"ztb"))
        {
            add(c, S::Z, A::KeepVisual, "view", "position current line", "z + z/t/b");
        }
        add(U'a', S::Z, A::KeepVisual, "fold", "toggle fold under cursor", "z a");
        add(U'A', S::Z, A::KeepVisual, "fold", "toggle folds recursively", "z A");
        add(U'c', S::Z, A::KeepVisual, "fold", "close fold under cursor", "z c");
        add(U'C', S::Z, A::KeepVisual, "fold", "close folds recursively", "z C");
        add(U'o', S::Z, A::KeepVisual, "fold", "open fold under cursor", "z o");
        add(U'O', S::Z, A::KeepVisual, "fold", "open folds recursively", "z O");
        add(U'M', S::Z, A::KeepVisual, "fold", "close all folds", "z M");
        add(U'R', S::Z, A::KeepVisual, "fold", "open all folds", "z R");
        add(U'f', S::Z, A::Complete, "fold", "fold selected ranges; Visual returns to Normal",
            "z f", 0, P::Zed);
        add(U'f', S::Z, A::Operator, "fold", "fold operator; needs a motion", "z f + motion", 0,
            P::Neovim);
        add(U'h', S::Z, A::KeepVisual, "left", "scroll view left", "z h");
        add(U'l', S::Z, A::KeepVisual, "right", "scroll view right", "z l");
        add(U'H', S::Z, A::KeepVisual, "Left", "scroll view half a screen left", "z H");
        add(U'L', S::Z, A::KeepVisual, "Right", "scroll view half a screen right", "z L");
        result.push_back({{0, Platform::SpecialKey::Enter},
                          0,
                          S::Z,
                          A::KeepVisual,
                          "top",
                          "line to top, first nonblank",
                          "z Enter"});
        add(U'-', S::Z, A::KeepVisual, "bottom", "line to bottom, first nonblank", "z -");
        add(U'.', S::Z, A::KeepVisual, "center", "line to center, first nonblank", "z .");
        add(U'^', S::Z, A::KeepVisual, "prev", "previous line to top, first nonblank", "z ^");
        add(U'+', S::Z, A::KeepVisual, "next", "next line to top, first nonblank", "z +");
        add(U'Z', S::UpperZ, A::Navigation, "close", "write and close file; assume Normal", "Z Z",
            0, P::Zed);
        add(U'Q', S::UpperZ, A::Navigation, "close", "close file without saving; assume Normal",
            "Z Q", 0, P::Zed);
        add(U'Z', S::UpperZ, A::Unknown, "resync", "write and quit; Neovim may quit, resync needed",
            "Z Z", 0, P::Neovim);
        add(U'Q', S::UpperZ, A::Unknown, "resync",
            "quit without saving; Neovim may quit, resync needed", "Z Q", 0, P::Neovim);
        for (const auto profile : {P::Zed, P::Neovim})
        {
            const bool zed = profile == P::Zed;
            for (char32_t c : std::u32string_view(U"hjkl"))
            {
                add(c, S::Window, A::Navigation, "pane",
                    zed ? "switch pane; assume Normal" : "switch Neovim window",
                    "Ctrl-w + direction", 0, profile);
                add(c, S::Window, A::Navigation, "pane",
                    zed ? "switch pane; assume Normal" : "switch Neovim window",
                    "Ctrl-w, Ctrl-direction", Platform::CONTROL, profile);
            }
            for (auto special : {Platform::SpecialKey::Left, Platform::SpecialKey::Right,
                                 Platform::SpecialKey::Up, Platform::SpecialKey::Down})
            {
                result.push_back({{0, special},
                                  0,
                                  S::Window,
                                  A::Navigation,
                                  "pane",
                                  zed ? "switch pane; assume Normal" : "switch Neovim window",
                                  "Ctrl-w + arrow",
                                  0,
                                  profile});
            }
            add(U'w', S::Window, A::Navigation, "next",
                zed ? "next pane; assume Normal" : "next Neovim window", "Ctrl-w, w", 0, profile);
            add(U'w', S::Window, A::Navigation, "next",
                zed ? "next pane; assume Normal" : "next Neovim window", "Ctrl-w, Ctrl-w",
                Platform::CONTROL, profile);
            add(U'W', S::Window, A::Navigation, "previous",
                zed ? "previous pane; assume Normal" : "previous Neovim window", "Ctrl-w, W", 0,
                profile);
            add(U'p', S::Window, A::Navigation, "previous",
                zed ? "previous pane; assume Normal" : "previous Neovim window", "Ctrl-w, p", 0,
                profile);
            add(U'p', S::Window, A::Navigation, "previous",
                zed ? "previous pane; assume Normal" : "previous Neovim window", "Ctrl-w, Ctrl-p",
                Platform::CONTROL, profile);
            add(U'v', S::Window, A::Navigation, "split",
                zed ? "split vertically; assume Normal" : "split Neovim window vertically",
                "Ctrl-w, v", 0, profile);
            add(U'v', S::Window, A::Navigation, "split",
                zed ? "split vertically; assume Normal" : "split Neovim window vertically",
                "Ctrl-w, Ctrl-v", Platform::CONTROL, profile);
            add(U's', S::Window, A::Navigation, "split",
                zed ? "split horizontally; assume Normal" : "split Neovim window horizontally",
                "Ctrl-w, s", 0, profile);
            add(U'S', S::Window, A::Navigation, "split",
                zed ? "split horizontally; assume Normal" : "split Neovim window horizontally",
                "Ctrl-w, S", 0, profile);
            add(U's', S::Window, A::Navigation, "split",
                zed ? "split horizontally; assume Normal" : "split Neovim window horizontally",
                "Ctrl-w, Ctrl-s", Platform::CONTROL, profile);
            add(U'c', S::Window, A::Navigation, "close",
                zed ? "close item; assume Normal" : "close Neovim window", "Ctrl-w, c", 0, profile);
            // Neovim's CTRL-W q quits Neovim from the last window, leaving the shell focused.
            add(U'q', S::Window, zed ? A::Navigation : A::Unknown, "close",
                zed ? "close item; assume Normal"
                    : "close Neovim window; may quit Neovim (resync needed)",
                "Ctrl-w, q", 0, profile);
            add(U'q', S::Window, zed ? A::Navigation : A::Unknown, "close",
                zed ? "close item; assume Normal"
                    : "close Neovim window; may quit Neovim (resync needed)",
                "Ctrl-w, Ctrl-q", Platform::CONTROL, profile);
            add(U'n', S::Window, A::Navigation, "new",
                zed ? "new file split; assume Normal" : "new Neovim window", "Ctrl-w, n", 0,
                profile);
            add(U'n', S::Window, A::Navigation, "new",
                zed ? "new file split; assume Normal" : "new Neovim window", "Ctrl-w, Ctrl-n",
                Platform::CONTROL, profile);
            add(U'o', S::Window, A::Complete, "only", "close other windows", "Ctrl-w, o", 0,
                profile);
            add(U'o', S::Window, A::Complete, "only", "close other windows", "Ctrl-w, Ctrl-o",
                Platform::CONTROL, profile);
            add(U'x', S::Window, A::Complete, "swap", "exchange with next pane", "Ctrl-w, x", 0,
                profile);
            add(U'x', S::Window, A::Complete, "swap", "exchange with next pane", "Ctrl-w, Ctrl-x",
                Platform::CONTROL, profile);
            for (char32_t c : std::u32string_view(U"HJKL"))
            {
                add(c, S::Window, A::Complete, "move", "move pane to the edge",
                    "Ctrl-w, shift + dir", 0, profile);
            }
            for (char32_t c : std::u32string_view(U"+-<>=_"))
            {
                add(c, S::Window, A::Complete, "resize", "resize the pane", "Ctrl-w + size", 0,
                    profile);
            }
        }
        add(U'c', S::Window, A::Navigation, "close", "close item; assume Normal", "Ctrl-w, Ctrl-c",
            Platform::CONTROL, P::Zed);
        add(U'a', S::Window, A::Navigation, "close", "close all items; assume Normal", "Ctrl-w, a",
            0, P::Zed);
        add(U'a', S::Window, A::Navigation, "close", "close all items; assume Normal",
            "Ctrl-w, Ctrl-a", Platform::CONTROL, P::Zed);
        add(U'd', S::Window, A::Navigation, "def", "definition in split; assume Normal",
            "Ctrl-w, d", 0, P::Zed);
        add(U']', S::Window, A::Navigation, "def", "definition in split; assume Normal",
            "Ctrl-w, ]", 0, P::Zed);
        add(U']', S::Window, A::Navigation, "def", "definition in split; assume Normal",
            "Ctrl-w, Ctrl-]", Platform::CONTROL, P::Zed);
        add(U'D', S::Window, A::Navigation, "type", "type definition in split; assume Normal",
            "Ctrl-w, D", 0, P::Zed);
        add(U' ', S::Window, A::Navigation, "excerpt", "open excerpts in split; assume Normal",
            "Ctrl-w, Space", 0, P::Zed);
        add(U'W', S::Window, A::Navigation, "previous", "previous pane; assume Normal",
            "Ctrl-w, Ctrl-Shift-w", Platform::CONTROL, P::Zed);
        for (auto special : {Platform::SpecialKey::Left, Platform::SpecialKey::Right,
                             Platform::SpecialKey::Up, Platform::SpecialKey::Down})
        {
            result.push_back({{0, special},
                              Platform::SHIFT,
                              S::Window,
                              A::Complete,
                              "swap",
                              "swap pane in that direction",
                              "Ctrl-w, shift + arrow",
                              0,
                              P::Zed});
        }
        add(U'd', S::Window, A::Complete, "diagnostic", "diagnostic float", "Ctrl-w, d", 0,
            P::Neovim);
        add(U'r', S::Window, A::Complete, "rotate", "rotate windows downwards", "Ctrl-w, r", 0,
            P::Neovim);
        add(U'r', S::Window, A::Complete, "rotate", "rotate windows downwards", "Ctrl-w, Ctrl-r",
            Platform::CONTROL, P::Neovim);
        add(U'R', S::Window, A::Complete, "rotate", "rotate windows upwards", "Ctrl-w, R", 0,
            P::Neovim);
        add(U'T', S::Window, A::Complete, "tab", "move window to a new tab", "Ctrl-w, T", 0,
            P::Neovim);
        add(U'|', S::Window, A::Complete, "width", "set window width", "Ctrl-w, |", 0, P::Neovim);
        for (char32_t c : std::u32string_view(U"wWspb(){}[]<>\"'`tB"))
        {
            add(c, S::TextObject, A::Motion, "object", "complete text object", "object");
        }
        add(U'q', S::TextObject, A::Motion, "quotes", "any quote pair", "object", 0, P::Zed);
        add(U'|', S::TextObject, A::Motion, "bars", "vertical bar pair", "object", 0, P::Zed);
        add(U'r', S::TextObject, A::Motion, "square", "square brackets", "object", 0, P::Zed);
        add(U'a', S::TextObject, A::Motion, "argument", "function argument", "object", 0, P::Zed);
        add(U'i', S::TextObject, A::Motion, "indent", "indent block", "object", 0, P::Zed);
        add(U'I', S::TextObject, A::Motion, "indent", "indent block and line below", "object", 0,
            P::Zed);
        add(U'f', S::TextObject, A::Motion, "method", "method body", "object", 0, P::Zed);
        add(U'c', S::TextObject, A::Motion, "class", "class body", "object", 0, P::Zed);
        add(U'e', S::TextObject, A::Motion, "file", "entire file", "object", 0, P::Zed);
        add(U'n', S::VisualTextObject, A::Motion, "node", "treesitter node (Visual)", "an / in", 0,
            P::Neovim);
        add(U'[', S::BracketOpen, A::Motion, "section", "previous section start", "[[");
        add(U']', S::BracketOpen, A::Motion, "section", "previous section end", "[]");
        add(U'[', S::BracketClose, A::Motion, "section", "next section end", "][");
        add(U']', S::BracketClose, A::Motion, "section", "next section start", "]]");
        add(U'm', S::BracketOpen, A::Motion, "method", "previous method start", "[m");
        add(U'M', S::BracketOpen, A::Motion, "method", "previous method end", "[M");
        add(U'm', S::BracketClose, A::Motion, "method", "next method start", "]m");
        add(U'M', S::BracketClose, A::Motion, "method", "next method end", "]M");
        for (char32_t c : std::u32string_view(U"*/"))
        {
            add(c, S::BracketOpen, A::Motion, "comment", "previous comment", "[ comment");
            add(c, S::BracketClose, A::Motion, "comment", "next comment", "] comment");
        }

        add(U'-', S::BracketOpen, A::Motion, "indent", "previous lesser indent", "[-", 0, P::Zed);
        add(U'+', S::BracketOpen, A::Motion, "indent", "previous greater indent", "[+", 0, P::Zed);
        add(U'=', S::BracketOpen, A::Motion, "indent", "previous same indent", "[=", 0, P::Zed);
        add(U'-', S::BracketClose, A::Motion, "indent", "next lesser indent", "]-", 0, P::Zed);
        add(U'+', S::BracketClose, A::Motion, "indent", "next greater indent", "]+", 0, P::Zed);
        add(U'=', S::BracketClose, A::Motion, "indent", "next same indent", "]=", 0, P::Zed);
        for (const auto profile : {P::Zed, P::Neovim})
        {
            add(U'b', S::BracketOpen, A::Navigation, "buffer",
                profile == P::Zed ? "previous file; assume Normal" : "previous Neovim buffer", "[b",
                0, profile);
            add(U'b', S::BracketClose, A::Navigation, "buffer",
                profile == P::Zed ? "next file; assume Normal" : "next Neovim buffer", "]b", 0,
                profile);
            add(U'B', S::BracketOpen, A::Navigation, "buffer",
                profile == P::Zed ? "first file; assume Normal" : "first Neovim buffer", "[B", 0,
                profile);
            add(U'B', S::BracketClose, A::Navigation, "buffer",
                profile == P::Zed ? "last file; assume Normal" : "last Neovim buffer", "]B", 0,
                profile);
        }
        add(U'd', S::NormalBracketOpen, A::Motion, "diagnostic", "previous diagnostic", "[d");
        add(U'd', S::NormalBracketClose, A::Motion, "diagnostic", "next diagnostic", "]d");
        add(U'D', S::NormalBracketOpen, A::Motion, "diagnostic", "first diagnostic", "[D", 0,
            P::Neovim);
        add(U'D', S::NormalBracketClose, A::Motion, "diagnostic", "last diagnostic", "]D", 0,
            P::Neovim);
        add(U'c', S::NormalBracketOpen, A::Motion, "hunk", "previous change hunk", "[c", 0, P::Zed);
        add(U'c', S::NormalBracketClose, A::Motion, "hunk", "next change hunk", "]c", 0, P::Zed);
        add(U'e', S::NormalBracketOpen, A::Complete, "line", "move line up", "[e", 0, P::Zed);
        add(U'e', S::NormalBracketClose, A::Complete, "line", "move line down", "]e", 0, P::Zed);
        add(U' ', S::NormalBracketOpen, A::Complete, "blank", "insert blank line above", "[Space");
        add(U' ', S::NormalBracketClose, A::Complete, "blank", "insert blank line below", "]Space");
        add(U'r', S::BracketOpen, A::Motion, "reference", "previous reference", "[r", 0, P::Zed);
        add(U'r', S::BracketClose, A::Motion, "reference", "next reference", "]r", 0, P::Zed);
        add(U'x', S::BracketOpen, A::SelectNode, "syntax", "select larger syntax node", "[x", 0,
            P::Zed);
        add(U'x', S::BracketClose, A::SelectNode, "syntax", "select smaller syntax node", "]x", 0,
            P::Zed);
        add(U'n', S::VisualBracketOpen, A::KeepVisual, "node", "select previous treesitter node",
            "[n", 0, P::Neovim);
        add(U'n', S::VisualBracketClose, A::KeepVisual, "node", "select next treesitter node", "]n",
            0, P::Neovim);
        add(U'q', S::BracketOpen, A::Navigation, "quickfix",
            "previous quickfix entry; may switch buffer", "[q", 0, P::Neovim);
        add(U'q', S::BracketClose, A::Navigation, "quickfix",
            "next quickfix entry; may switch buffer", "]q", 0, P::Neovim);
        add(U'Q', S::BracketOpen, A::Navigation, "quickfix",
            "first quickfix entry; may switch buffer", "[Q", 0, P::Neovim);
        add(U'Q', S::BracketClose, A::Navigation, "quickfix",
            "last quickfix entry; may switch buffer", "]Q", 0, P::Neovim);
        add(U'q', S::BracketOpen, A::Navigation, "quickfix",
            "previous quickfix entry; may switch buffer", "[Ctrl-q", Platform::CONTROL, P::Neovim);
        add(U'q', S::BracketClose, A::Navigation, "quickfix",
            "next quickfix entry; may switch buffer", "]Ctrl-q", Platform::CONTROL, P::Neovim);
        add(U'l', S::BracketOpen, A::Navigation, "location",
            "previous location entry; may switch buffer", "[l", 0, P::Neovim);
        add(U'l', S::BracketClose, A::Navigation, "location",
            "next location entry; may switch buffer", "]l", 0, P::Neovim);
        add(U'L', S::BracketOpen, A::Navigation, "location",
            "first location entry; may switch buffer", "[L", 0, P::Neovim);
        add(U'L', S::BracketClose, A::Navigation, "location",
            "last location entry; may switch buffer", "]L", 0, P::Neovim);
        add(U'l', S::BracketOpen, A::Navigation, "location",
            "previous location entry; may switch buffer", "[Ctrl-l", Platform::CONTROL, P::Neovim);
        add(U'l', S::BracketClose, A::Navigation, "location",
            "next location entry; may switch buffer", "]Ctrl-l", Platform::CONTROL, P::Neovim);
        add(U't', S::BracketOpen, A::Navigation, "tag", "previous tag match; may switch buffer",
            "[t", 0, P::Neovim);
        add(U't', S::BracketClose, A::Navigation, "tag", "next tag match; may switch buffer", "]t",
            0, P::Neovim);
        add(U'T', S::BracketOpen, A::Navigation, "tag", "first tag match; may switch buffer", "[T",
            0, P::Neovim);
        add(U'T', S::BracketClose, A::Navigation, "tag", "last tag match; may switch buffer", "]T",
            0, P::Neovim);
        add(U't', S::BracketOpen, A::Navigation, "tag", "previous tag match; may switch buffer",
            "[Ctrl-t", Platform::CONTROL, P::Neovim);
        add(U't', S::BracketClose, A::Navigation, "tag", "next tag match; may switch buffer",
            "]Ctrl-t", Platform::CONTROL, P::Neovim);
        add(U'a', S::BracketOpen, A::Navigation, "argument", "previous argument; may switch buffer",
            "[a", 0, P::Neovim);
        add(U'a', S::BracketClose, A::Navigation, "argument", "next argument; may switch buffer",
            "]a", 0, P::Neovim);
        add(U'A', S::BracketOpen, A::Navigation, "argument", "first argument; may switch buffer",
            "[A", 0, P::Neovim);
        add(U'A', S::BracketClose, A::Navigation, "argument", "last argument; may switch buffer",
            "]A", 0, P::Neovim);
        add(U'{', S::BracketOpen, A::Motion, "pair", "previous unmatched brace", "[{");
        add(U'(', S::BracketOpen, A::Motion, "pair", "previous unmatched parenthesis", "[(");
        add(U'}', S::BracketClose, A::Motion, "pair", "next unmatched brace", "]}");
        add(U')', S::BracketClose, A::Motion, "pair", "next unmatched parenthesis", "])");
        for (std::uint8_t mods = 0; mods < 16; ++mods)
        {
            result.push_back({{},
                              mods,
                              S::LiteralArgument,
                              A::Literal,
                              "literal",
                              "insert this key literally",
                              "literal key",
                              0});
        }
        result.push_back({{},
                          Platform::SHIFT,
                          S::Argument,
                          A::Motion,
                          "char",
                          "next literal character",
                          "character",
                          0});
        result.push_back(
            {{}, 0, S::Argument, A::Motion, "char", "next literal character", "character", 0});
        result.push_back({{0, Platform::SpecialKey::Escape},
                          0,
                          S::Editor,
                          A::Escape,
                          "normal",
                          "cancel / leave mode",
                          "Esc",
                          0});
        add(U'[', S::Editor, A::Escape, "normal", "cancel / leave mode", "Ctrl-[",
            Platform::CONTROL);
        add(U'c', S::Editor, A::Escape, "normal", "cancel / leave mode", "Ctrl-c",
            Platform::CONTROL);
        for (auto special : {Platform::SpecialKey::Left, Platform::SpecialKey::Right,
                             Platform::SpecialKey::Up, Platform::SpecialKey::Down})
        {
            result.push_back(
                {{0, special}, 0, S::Motion, A::Motion, "move", "move cursor", "arrow", 0});
        }
        add(U'f', S::Motion, A::Motion, "page", "page down", "Ctrl-f", Platform::CONTROL);
        add(U'b', S::Motion, A::Motion, "page", "page up", "Ctrl-b", Platform::CONTROL);
        add(U'd', S::Motion, A::Motion, "scroll", "half-page down", "Ctrl-d", Platform::CONTROL);
        add(U'u', S::Motion, A::Motion, "scroll", "half-page up", "Ctrl-u", Platform::CONTROL);
        add(U'e', S::Motion, A::Motion, "scroll", "one line down", "Ctrl-e", Platform::CONTROL);
        add(U'y', S::Motion, A::Motion, "scroll", "one line up", "Ctrl-y", Platform::CONTROL);
        add(U'i', S::Motion, A::Motion, "jump", "forward in jump list", "Ctrl-i",
            Platform::CONTROL);
        add(U'o', S::Motion, A::Motion, "jump", "back in jump list", "Ctrl-o", Platform::CONTROL);
        add(U'*', S::Motion, A::Motion, "match", "next word match", "*");
        add(U'#', S::Motion, A::Motion, "match", "previous word match", "#");
        add(U' ', S::Motion, A::Motion, "right", "move right, wrapping", "Space");
        add(U'+', S::Motion, A::Motion, "line", "next line, first nonblank", "+");
        add(U'm', S::Motion, A::Motion, "line", "next line, first nonblank", "Ctrl-m",
            Platform::CONTROL);
        add(U'-', S::Motion, A::Motion, "line", "previous line, first nonblank", "-");
        add(U'_', S::Motion, A::Motion, "line", "line down, first nonblank", "_");
        add(U'|', S::Motion, A::Motion, "column", "go to column", "|");
        add(U'(', S::Motion, A::Motion, "sentence", "previous sentence", "(");
        add(U')', S::Motion, A::Motion, "sentence", "next sentence", ")");
        add(U'H', S::Motion, A::Motion, "top", "window top", "H");
        add(U'M', S::Motion, A::Motion, "middle", "window middle", "M");
        add(U'L', S::Motion, A::Motion, "bottom", "window bottom", "L");
        add(U'j', S::Motion, A::Motion, "down", "move down", "Ctrl-j", Platform::CONTROL);
        result.push_back({{0, Platform::SpecialKey::Home},
                          0,
                          S::Motion,
                          A::Motion,
                          "start",
                          "line start",
                          "Home",
                          0});
        result.push_back(
            {{0, Platform::SpecialKey::End}, 0, S::Motion, A::Motion, "end", "line end", "End", 0});
        result.push_back({{0, Platform::SpecialKey::PageUp},
                          0,
                          S::Motion,
                          A::Motion,
                          "page",
                          "page up",
                          "PageUp",
                          0});
        result.push_back({{0, Platform::SpecialKey::PageDown},
                          0,
                          S::Motion,
                          A::Motion,
                          "page",
                          "page down",
                          "PageDown",
                          0});
        for (const auto scope : {S::Insert, S::Replace})
        {
            add(U'a', scope, A::Complete, "again", "insert previous text", "Ctrl-a",
                Platform::CONTROL, P::Neovim);
            add(U'd', scope, A::Complete, "indent", "decrease indent", "Ctrl-d", Platform::CONTROL,
                P::Neovim);
            add(U't', scope, A::Complete, "indent", "increase indent", "Ctrl-t", Platform::CONTROL,
                P::Neovim);
            add(U'e', scope, A::Complete, "below", "insert character from below", "Ctrl-e",
                Platform::CONTROL, P::Neovim);
            add(U'y', scope, A::Complete, "above", "insert character from above", "Ctrl-y",
                Platform::CONTROL, P::Neovim);
            add(U'j', scope, A::Complete, "line", "new line", "Ctrl-j", Platform::CONTROL,
                P::Neovim);
            add(U'm', scope, A::Complete, "line", "new line", "Ctrl-m", Platform::CONTROL,
                P::Neovim);
            add(U'i', scope, A::Complete, "tab", "insert tab", "Ctrl-i", Platform::CONTROL,
                P::Neovim);
            add(U'n', scope, A::Complete, "next", "next completion; stays Insert", "Ctrl-n",
                Platform::CONTROL, P::Neovim);
            add(U'p', scope, A::Complete, "previous", "previous completion; stays Insert", "Ctrl-p",
                Platform::CONTROL, P::Neovim);
            add(U's', scope, A::Complete, "signature", "LSP signature help", "Ctrl-s",
                Platform::CONTROL, P::Neovim);
            add(U'x', scope, A::PrefixCtrlX, "prefix", "CTRL-X mode; one Ctrl key follows",
                "Ctrl-x + key", Platform::CONTROL, P::Neovim);
            for (const auto special : {Platform::SpecialKey::Left, Platform::SpecialKey::Right,
                                       Platform::SpecialKey::Up, Platform::SpecialKey::Down})
            {
                result.push_back({{0, special},
                                  0,
                                  scope,
                                  A::Complete,
                                  "move",
                                  "move cursor",
                                  "arrow",
                                  0,
                                  P::Neovim});
            }
        }
        result.push_back({{0, Platform::SpecialKey::Enter},
                          0,
                          S::Motion,
                          A::Motion,
                          "line",
                          "next line, first nonblank",
                          "Enter",
                          0});
        result.push_back({{0, Platform::SpecialKey::Backspace},
                          0,
                          S::Motion,
                          A::Motion,
                          "left",
                          "move left, wrapping",
                          "Backspace",
                          0});
        result.push_back({{0, Platform::SpecialKey::Tab},
                          0,
                          S::Motion,
                          A::Motion,
                          "jump",
                          "jump forward",
                          "Tab",
                          0,
                          P::Neovim});
        for (const auto scope : {S::Insert, S::Replace})
        {
            result.push_back({{0, Platform::SpecialKey::Enter},
                              0,
                              scope,
                              A::Complete,
                              "line",
                              "new line",
                              "Enter",
                              0,
                              P::Neovim});
            result.push_back({{0, Platform::SpecialKey::Backspace},
                              0,
                              scope,
                              A::Complete,
                              "back",
                              "delete previous character",
                              "Backspace",
                              0,
                              P::Neovim});
            result.push_back({{0, Platform::SpecialKey::Tab},
                              0,
                              scope,
                              A::Complete,
                              "tab",
                              "insert tab",
                              "Tab",
                              0,
                              P::Neovim});
        }
        for (const auto& command : EX_COMMANDS)
        {
            for (std::size_t i = 0; i < command.name.size(); ++i)
            {
                add(command.name[i], S::Ex, A::ExInput, command.shortHint, command.fullHint,
                    command.chord, 0, P::Neovim);
                result.back().commandPrefix = command.name.substr(0, i);
            }
        }
        result.push_back({{0, Platform::SpecialKey::Enter},
                          0,
                          S::ExControl,
                          A::ExInput,
                          "run",
                          "execute the Ex command",
                          "Enter",
                          0,
                          P::Neovim});
        result.push_back({{0, Platform::SpecialKey::Backspace},
                          0,
                          S::ExControl,
                          A::ExInput,
                          "back",
                          "erase the previous character",
                          "Backspace",
                          0,
                          P::Neovim});
        add(U'u', S::ExControl, A::ExInput, "clear", "erase the command before the cursor",
            "Ctrl-u", Platform::CONTROL, P::Neovim);
        add(U'w', S::ExControl, A::ExInput, "word", "erase the previous word", "Ctrl-w",
            Platform::CONTROL, P::Neovim);
        add(U'h', S::ExControl, A::ExInput, "back", "erase the previous character", "Ctrl-h",
            Platform::CONTROL, P::Neovim);
        add(U'm', S::ExControl, A::ExInput, "run", "execute the Ex command", "Ctrl-m",
            Platform::CONTROL, P::Neovim);
        add(U'j', S::ExControl, A::ExInput, "run", "execute the Ex command", "Ctrl-j",
            Platform::CONTROL, P::Neovim);
        add(U'!', S::ExControl, A::ExInput, "force", "force this command", "!", 0, P::Neovim);
        add(U'\\', S::Terminal, A::TerminalPrefix, "normal", "leave Terminal mode",
            "Ctrl-\\ Ctrl-n", Platform::CONTROL, P::Neovim);
        add(U'n', S::TerminalPrefix, A::LeaveTerminal, "normal", "leave Terminal mode", "Ctrl-n",
            Platform::CONTROL, P::Neovim);
        add(U'o', S::TerminalPrefix, A::Unknown, "resync", "one Normal command; resync needed",
            "Ctrl-o", Platform::CONTROL, P::Neovim);
        return result;
    }();
    return bindings;
}

bool OperatorIs(const VimState& state, std::string_view name)
{
    if (!state.operatorKey)
    {
        return false;
    }
    if (name.size() == 1)
    {
        return !state.operatorPrefix && state.operatorKey == static_cast<char32_t>(name[0]);
    }
    return name.size() == 2 && state.operatorPrefix == static_cast<char32_t>(name[0]) &&
           state.operatorKey == static_cast<char32_t>(name[1]);
}

bool BindingAvailable(const VimBinding& binding, const VimState& state)
{
    if (binding.profile && *binding.profile != state.appProfile)
    {
        return false;
    }
    if (!binding.pendingOperator.empty() && !OperatorIs(state, binding.pendingOperator))
    {
        return false;
    }
    if (binding.scope == BindingScope::Terminal || binding.scope == BindingScope::TerminalPrefix)
    {
        const auto wait = binding.scope == BindingScope::Terminal ? WaitingKind::None
                                                                  : WaitingKind::TerminalPrefix;
        return state.mode == VimMode::Terminal && state.confidence != Confidence::Uncertain &&
               state.waiting == wait;
    }
    if (state.mode == VimMode::Terminal)
    {
        return false;
    }
    if (state.appProfile == Platform::AppProfile::Neovim &&
        binding.action == BindingAction::Navigation &&
        (binding.scope == BindingScope::BracketOpen ||
         binding.scope == BindingScope::BracketClose) &&
        (state.mode != VimMode::Normal || state.operatorKey))
    {
        return false;
    }
    if (binding.scope == BindingScope::Editor)
    {
        if (state.waiting == WaitingKind::Literal)
        {
            return false;
        }
        if (binding.value.character == U'c')
        {
            return state.waiting != WaitingKind::CtrlX &&
                   (state.mode != VimMode::Normal || state.operatorKey ||
                    state.waiting != WaitingKind::None || state.context != VimContext::Editor);
        }
        return true;
    }
    if (binding.scope == BindingScope::Ex || binding.scope == BindingScope::ExControl)
    {
        if (state.confidence == Confidence::Uncertain || state.context != VimContext::CommandLine ||
            state.waiting == WaitingKind::Literal)
        {
            return false;
        }
        if (binding.scope == BindingScope::Ex)
        {
            return !state.exOpaque && !state.exArguments && !state.exBang &&
                   ExPrefix(state) == binding.commandPrefix;
        }
        if (binding.value.character == U'!')
        {
            return !state.exOpaque && !state.exArguments && !state.exBang &&
                   CanForceExCommand(ExPrefix(state));
        }
        return true;
    }
    const bool editable =
        state.context == VimContext::Editor || state.context == VimContext::CommandWindow;
    if (state.confidence == Confidence::Uncertain || !editable)
    {
        return false;
    }
    const auto wait = state.waiting;
    using S = BindingScope;
    if (wait != WaitingKind::None)
    {
        switch (binding.scope)
        {
            case S::G:
                return wait == WaitingKind::G &&
                       ((binding.action != BindingAction::Operator &&
                         binding.action != BindingAction::SelectMultiple) ||
                        !state.operatorKey);
            case S::NormalG:
                return wait == WaitingKind::G && state.mode == VimMode::Normal &&
                       !state.operatorKey;
            case S::VisualG:
                return wait == WaitingKind::G && IsVisual(state.mode) && !state.operatorKey;
            case S::Lsp:
                return wait == WaitingKind::Lsp;
            case S::Z:
                return wait == WaitingKind::Z;
            case S::Window:
                return wait == WaitingKind::Window;
            case S::TextObject:
                return wait == WaitingKind::TextObject;
            case S::VisualTextObject:
                return wait == WaitingKind::TextObject && IsVisual(state.mode);
            case S::BracketOpen:
                return wait == WaitingKind::BracketOpen;
            case S::BracketClose:
                return wait == WaitingKind::BracketClose;
            case S::NormalBracketOpen:
                return wait == WaitingKind::BracketOpen && state.mode == VimMode::Normal &&
                       !state.operatorKey;
            case S::NormalBracketClose:
                return wait == WaitingKind::BracketClose && state.mode == VimMode::Normal &&
                       !state.operatorKey;
            case S::VisualBracketOpen:
                return wait == WaitingKind::BracketOpen && IsVisual(state.mode);
            case S::VisualBracketClose:
                return wait == WaitingKind::BracketClose && IsVisual(state.mode);
            case S::UpperZ:
                return wait == WaitingKind::UpperZ;
            case S::LiteralArgument:
                return wait == WaitingKind::Literal;
            case S::CtrlX:
                return wait == WaitingKind::CtrlX;
            case S::Argument:
                return wait != WaitingKind::G && wait != WaitingKind::Z &&
                       wait != WaitingKind::Window && wait != WaitingKind::TextObject &&
                       wait != WaitingKind::BracketOpen && wait != WaitingKind::BracketClose &&
                       wait != WaitingKind::Literal && wait != WaitingKind::Lsp &&
                       wait != WaitingKind::CtrlX && wait != WaitingKind::UpperZ;
            default:
                return false;
        }
    }
    const bool normal = state.mode == VimMode::Normal;
    const bool visual = IsVisual(state.mode);
    switch (binding.scope)
    {
        case S::Motion:
            return normal || visual || IsSelect(state.mode);
        case S::Normal:
            return normal && !state.operatorKey;
        case S::NormalVisual:
            return (normal && !state.operatorKey) || visual;
        case S::Visual:
            return visual;
        case S::Select:
            return IsSelect(state.mode);
        case S::VisualBlock:
            return state.mode == VimMode::VisualBlock;
        case S::Cursor:
            return !state.operatorKey && state.mode != VimMode::Replace;
        case S::Insert:
            return state.mode == VimMode::Insert;
        case S::Replace:
            return state.mode == VimMode::Replace || state.mode == VimMode::VirtualReplace;
        case S::Operator:
            return (normal && state.operatorKey &&
                    (binding.action != BindingAction::Motion ||
                     binding.value.character == state.operatorKey ||
                     !binding.pendingOperator.empty())) ||
                   (visual && binding.action == BindingAction::TextObject);
        default:
            return false;
    }
}

HintId FindBinding(const VimState& state, Platform::KeyValue value, std::uint8_t modifiers)
{
    auto bindings = VimBindings();
    HintId generic = NO_HINT;
    for (HintId i = 0; i < bindings.size(); ++i)
    {
        const auto& binding = bindings[i];
        if (binding.scope == BindingScope::Argument || binding.value != value ||
            binding.modifiers != modifiers || !BindingAvailable(binding, state))
        {
            continue;
        }
        // An operator-specific key (such as ? after g?) wins over the generic binding of that key.
        if (!binding.pendingOperator.empty())
        {
            return i;
        }
        if (generic == NO_HINT)
        {
            generic = i;
        }
    }
    return generic;
}
