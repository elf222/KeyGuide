// guide/binding_scopes.hpp
#pragma once
#include <string_view>
#include "gameLogic/blender_bindings.hpp"
#include "gameLogic/vim_bindings.hpp"

// Where a Vim binding applies, phrased for a learner
// (e.g. "Normal mode, after an operator such as d/c/y").
inline std::string_view VimScopeDescription(BindingScope scope)
{
    switch (scope)
    {
        case BindingScope::Motion:
            return "Normal, Visual, and Select modes (cursor movement)";
        case BindingScope::Normal:
            return "Vim Normal mode, with no operator pending";
        case BindingScope::NormalVisual:
            return "Vim Normal and Visual modes";
        case BindingScope::Visual:
            return "Vim Visual modes, with a selection active";
        case BindingScope::VisualBlock:
            return "Vim Visual Block mode (Ctrl-v)";
        case BindingScope::Select:
            return "Vim Select mode (typing replaces the selection)";
        case BindingScope::Insert:
            return "Vim Insert mode";
        case BindingScope::Replace:
            return "Vim Replace mode (typing overwrites characters)";
        case BindingScope::Operator:
            return "after an operator such as d, c, or y, before its motion";
        case BindingScope::G:
            return "typed after g (Normal, Visual, or after an operator)";
        case BindingScope::NormalG:
            return "typed after g in Normal mode";
        case BindingScope::VisualG:
            return "typed after g in Visual mode";
        case BindingScope::Lsp:
            return "typed after gr, the language server (LSP) prefix";
        case BindingScope::Z:
            return "typed after z (folds, scrolling, line positioning)";
        case BindingScope::Window:
            return "typed after Ctrl-w (window and pane commands)";
        case BindingScope::TextObject:
            return "the object after an operator plus i or a, e.g. diw";
        case BindingScope::BracketOpen:
            return "typed after [ (previous section, method, or comment)";
        case BindingScope::BracketClose:
            return "typed after ] (next section, method, or comment)";
        case BindingScope::Argument:
            return "the single character typed after f, t, r, m, or q";
        case BindingScope::LiteralArgument:
            return "the key typed after Ctrl-v or Ctrl-q, inserted as-is";
        case BindingScope::Editor:
            return "Escape, Ctrl-[, or Ctrl-c to leave the current mode";
        case BindingScope::Ex:
            return "a command name typed on the : command line";
        case BindingScope::ExControl:
            return "editing keys on the : command line (Ctrl-u, Ctrl-w, Enter)";
        case BindingScope::Cursor:
            return "multi-cursor keys such as Cmd-d (not in Replace mode)";
        case BindingScope::Terminal:
            return "Neovim Terminal mode, the Ctrl-\\ prefix";
        case BindingScope::TerminalPrefix:
            return "one key after Ctrl-\\ in Neovim Terminal mode";
        case BindingScope::VisualTextObject:
            return "Visual mode, after i or a (treesitter node objects)";
        case BindingScope::CtrlX:
            return "typed after Ctrl-x in Insert mode (completion keys)";
        case BindingScope::UpperZ:
            return "typed after Z (write and close)";
        case BindingScope::NormalBracketOpen:
            return "typed after [ in Normal mode (diagnostics, hunks, lines)";
        case BindingScope::NormalBracketClose:
            return "typed after ] in Normal mode (diagnostics, hunks, lines)";
        case BindingScope::VisualBracketOpen:
            return "typed after [ in Visual mode (treesitter node selection)";
        case BindingScope::VisualBracketClose:
            return "typed after ] in Visual mode (treesitter node selection)";
    }
    return "";
}

// Where a Blender binding applies (e.g. "3D Viewport, Edit Mode (mesh)").
inline std::string_view BlenderScopeDescription(BlenderScope scope)
{
    switch (scope)
    {
        case BlenderScope::Editor:
            return "Blender 3D Viewport or Node Editor, no tool running";
        case BlenderScope::Idle:
            return "Blender 3D Viewport, no tool running";
        case BlenderScope::Object:
            return "Blender 3D Viewport, Object Mode, no tool running";
        case BlenderScope::Mesh:
            return "Blender 3D Viewport, mesh Edit Mode, no tool running";
        case BlenderScope::ObjectOrMesh:
            return "Blender 3D Viewport, Object Mode or mesh Edit Mode";
        case BlenderScope::Curve:
            return "Blender 3D Viewport, curve Edit Mode, no tool running";
        case BlenderScope::Node:
            return "Blender Node Editor, no transform or tool running";
        case BlenderScope::NodeTransform:
            return "Blender Node Editor, while moving, rotating, or scaling";
        case BlenderScope::NodeMove:
            return "Blender Node Editor, while moving nodes or attaching them";
        case BlenderScope::NodeLink:
            return "Blender Node Editor, while dragging a link between sockets";
        case BlenderScope::NodeResize:
            return "Blender Node Editor, while resizing a node";
        case BlenderScope::Compositor:
            return "Blender Compositor node tree, no tool running";
        case BlenderScope::Transform:
            return "Blender 3D Viewport, while moving, rotating, or scaling";
        case BlenderScope::TransformInput:
            return "Blender 3D Viewport, typing a value or axis during a transform";
        case BlenderScope::Modal:
            return "Blender while a modal operation runs (Enter confirms, Esc cancels)";
        case BlenderScope::Knife:
            return "Blender 3D Viewport, Knife tool running";
        case BlenderScope::Bevel:
            return "Blender 3D Viewport, Bevel tool running";
        case BlenderScope::Inset:
            return "Blender 3D Viewport, Inset tool running";
        case BlenderScope::LoopCut:
            return "Blender 3D Viewport, Loop Cut tool running";
        case BlenderScope::BoxSelect:
            return "Blender, while a box select drag is in progress";
        case BlenderScope::CircleSelect:
            return "Blender, while circle select is in progress";
        case BlenderScope::Menu:
            return "Blender while a popup or pie menu is open";
        case BlenderScope::Text:
            return "Blender text field for search or rename";
        case BlenderScope::DeleteConfirm:
            return "Blender delete confirmation popup is open";
    }
    return "";
}
