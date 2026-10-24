// gameLogic/vim_ex_commands.hpp
#pragma once
#include <array>
#include <string_view>
#include "gameLogic/vim_state.hpp"

enum class ExResult
{
    Complete,
    Normal,
    Insert,
    Replace,
    Terminal,
    LeaveEx,
    Unknown
};

struct ExCommand
{
    std::string_view name, shortHint, fullHint, chord;
    std::size_t minimumLength;
    ExResult result;
};

inline constexpr std::array EX_COMMANDS{
    ExCommand{"write", "write", "write the current file", ":write", 1, ExResult::Complete},
    ExCommand{"wq", "quit", "write and close; may leave Neovim", ":wq", 2, ExResult::Unknown},
    ExCommand{"wall", "all", "write all changed buffers", ":wall", 2, ExResult::Complete},
    ExCommand{"wqall", "all", "write all and quit Neovim", ":wqall", 3, ExResult::Unknown},
    ExCommand{"quit", "quit", "close a window; may leave Neovim", ":quit", 1, ExResult::Unknown},
    ExCommand{"qall", "all", "quit all Neovim windows", ":qall", 2, ExResult::Unknown},
    ExCommand{"xit", "exit", "write if changed and close", ":xit", 1, ExResult::Unknown},
    ExCommand{"exit", "exit", "write if changed and close", ":exit", 2, ExResult::Unknown},
    ExCommand{"edit", "edit", "edit a file", ":edit", 1, ExResult::Complete},
    ExCommand{"enew", "new", "edit a new empty buffer", ":enew", 3, ExResult::Complete},
    ExCommand{"new", "new", "new buffer in a horizontal split", ":new", 3, ExResult::Complete},
    ExCommand{"vnew", "new", "new buffer in a vertical split", ":vnew", 3, ExResult::Complete},
    ExCommand{"split", "split", "split the current window horizontally", ":split", 2,
              ExResult::Complete},
    ExCommand{"vsplit", "split", "split the current window vertically", ":vsplit", 2,
              ExResult::Complete},
    ExCommand{"buffer", "buffer", "switch buffer", ":buffer", 1, ExResult::Complete},
    ExCommand{"bnext", "next", "next buffer", ":bnext", 2, ExResult::Complete},
    ExCommand{"bprevious", "previous", "previous buffer", ":bprevious", 2, ExResult::Complete},
    ExCommand{"bdelete", "delete", "delete the current buffer", ":bdelete", 2, ExResult::Complete},
    ExCommand{"tabnext", "next", "next Neovim tab", ":tabnext", 4, ExResult::Complete},
    ExCommand{"tabprevious", "previous", "previous Neovim tab", ":tabprevious", 4,
              ExResult::Complete},
    ExCommand{"tabnew", "new", "open a new Neovim tab", ":tabnew", 6, ExResult::Complete},
    ExCommand{"tabedit", "edit", "edit in a new Neovim tab", ":tabedit", 4, ExResult::Complete},
    ExCommand{"tabclose", "close", "close the current Neovim tab", ":tabclose", 4,
              ExResult::Complete},
    ExCommand{"update", "update", "write only if the buffer changed", ":update", 2,
              ExResult::Complete},
    ExCommand{"undo", "undo", "undo the last change", ":undo", 1, ExResult::Complete},
    ExCommand{"redo", "redo", "redo the last undone change", ":redo", 3, ExResult::Complete},
    ExCommand{"nohlsearch", "clear", "clear search highlighting", ":nohlsearch", 3,
              ExResult::Complete},
    ExCommand{"help", "help", "open Neovim help", ":help", 1, ExResult::Complete},
    ExCommand{"substitute", "sub", "substitute text; arguments are not tracked", ":substitute", 1,
              ExResult::Unknown},
    ExCommand{"set", "set", "change options; arguments are not tracked", ":set", 2,
              ExResult::Unknown},
    ExCommand{"startinsert", "insert", "start Insert mode", ":startinsert", 4, ExResult::Insert},
    ExCommand{"startreplace", "replace", "start Replace mode", ":startreplace", 6,
              ExResult::Replace},
    ExCommand{"stopinsert", "normal", "stop Insert mode", ":stopinsert", 5, ExResult::Normal},
    ExCommand{"visual", "visual", "leave Ex mode for Normal", ":visual", 2, ExResult::LeaveEx},
    ExCommand{"terminal", "terminal", "open a terminal; Terminal mode", ":terminal", 3,
              ExResult::Terminal},
    ExCommand{"normal", "normal", "execute Normal keys; resync needed", ":normal", 4,
              ExResult::Unknown},
    ExCommand{"lua", "lua", "execute Lua; resync needed", ":lua", 3, ExResult::Unknown},
    ExCommand{"source", "source", "execute a script; resync needed", ":source", 2,
              ExResult::Unknown},
};

inline bool IsExPrefix(std::string_view prefix)
{
    for (const auto& command : EX_COMMANDS)
    {
        if (command.name.starts_with(prefix))
        {
            return true;
        }
    }
    return false;
}

inline const ExCommand* FindExCommand(std::string_view name)
{
    for (const auto& command : EX_COMMANDS)
    {
        if (command.name == name)
        {
            return &command;
        }
    }
    const ExCommand* match = nullptr;
    for (const auto& command : EX_COMMANDS)
    {
        if (name.size() < command.minimumLength || !command.name.starts_with(name))
        {
            continue;
        }
        if (match)
        {
            return nullptr;
        }
        match = &command;
    }
    return match;
}

inline std::string_view ExPrefix(const VimState& state)
{
    if (!state.pendingLength)
    {
        return {};
    }
    return {state.pendingSequence.data() + 1,
            static_cast<std::size_t>(state.pendingLength - 1 - state.exBang)};
}

inline bool CanForceExCommand(std::string_view prefix)
{
    const auto* command = FindExCommand(prefix);
    if (!command)
    {
        return false;
    }
    for (const auto name :
         {"write",  "wq",      "wall",     "wqall",  "quit",        "qall",
          "xit",    "exit",    "edit",     "enew",   "new",         "vnew",
          "split",  "vsplit",  "buffer",   "bnext",  "bprevious",   "bdelete",
          "tabnew", "tabedit", "tabclose", "update", "startinsert", "startreplace"})
    {
        if (command->name == name)
        {
            return true;
        }
    }
    return false;
}
