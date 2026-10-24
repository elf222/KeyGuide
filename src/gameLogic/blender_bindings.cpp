// gameLogic/blender_bindings.cpp
#include "gameLogic/blender_bindings.hpp"
#include <array>
#include <vector>
#include <tuple>

namespace
{
    bool MacAliasAllowed(Platform::KeyId key, std::uint8_t modifiers)
    {
        using K = Platform::KeyId;
        if (!(modifiers & Platform::CONTROL))
        {
            return false;
        }
        if (!(modifiers & (Platform::SHIFT | Platform::OPTION)))
        {
            switch (key)
            {
                case K::H:
                case K::M:
                case K::Space:
                case K::W:
                case K::Grave:
                case K::Period:
                case K::Tab:
                    return false;
                default:
                    break;
            }
        }
        return !(key == K::Q && (modifiers & Platform::OPTION) && !(modifiers & Platform::SHIFT));
    }
} // namespace

std::span<const BlenderBinding> BlenderBindings()
{
    // Installed Blender 5.2 defaults: km_window, km_screen, km_node_editor, km_edit_curve_legacy.
    // Mac aliases: scripts/modules/bl_keymap_utils/platform_helpers.py
    static const auto bindings = []
    {
        std::vector<BlenderBinding> result;
        using K = Platform::KeyId;
        using S = BlenderScope;
        using A = BlenderAction;
        using M = BlenderMenuKind;
        using namespace Platform;
        const auto add = [&](K key, std::uint8_t modifiers, S scope, A action,
                             std::string_view short_hint, std::string_view full_hint,
                             char32_t mnemonic = 0, bool any = false, M menu = M::Unknown)
        {
            if (!mnemonic && key >= K::A && key <= K::Z)
            {
                const auto lower = static_cast<char>(key) + 32;
                if (short_hint.find(lower) != std::string_view::npos)
                {
                    mnemonic = lower;
                }
                else if (short_hint.find(static_cast<char>(key)) != std::string_view::npos)
                {
                    mnemonic = static_cast<char32_t>(key);
                }
            }
            result.push_back(
                {key, modifiers, scope, action, short_hint, full_hint, mnemonic, any, false, menu});
            if (!any && MacAliasAllowed(key, modifiers))
            {
                auto alias = result.back();
                alias.modifiers = (modifiers & ~CONTROL) | COMMAND;
                alias.macOnly = true;
                result.push_back(alias);
            }
        };
        add(K::F1, 0, S::Editor, A::PreserveMode, "help",
            "open the manual for the interface item under the pointer");
        add(K::F2, CONTROL, S::Editor, A::Rename, "batch", "batch rename selected data");
        add(K::F4, 0, S::Editor, A::Menu, "file", "open the file context menu");
        // Opens nothing without a redoable operation; its panel closes when the pointer leaves.
        add(K::F9, 0, S::Editor, A::PreserveMode, "adjust",
            "adjust the last operation, when available");
        add(K::F11, 0, S::Editor, A::ExecuteUnknown, "render", "show / hide the render result");
        add(K::F11, CONTROL, S::Editor, A::PreserveMode, "play", "play rendered animation");
        add(K::F12, 0, S::Editor, A::ExecuteUnknown, "render", "render the current frame");
        add(K::F12, CONTROL, S::Editor, A::ExecuteUnknown, "animate", "render animation");
        add(K::F12, OPTION, S::Editor, A::ExecuteUnknown, "sequence",
            "render the current frame from the sequencer scene");
        add(K::F12, CONTROL | OPTION, S::Editor, A::ExecuteUnknown, "sequence",
            "render animation from the sequencer scene");
        const std::array<std::pair<std::string_view, std::string_view>, 12> editors{{
            {"files", "switch to File Browser / Asset Browser"},
            {"clip", "switch to Movie Clip Editor"},
            {"nodes", "switch to / cycle Node Editors"},
            {"console", "switch to Python Console"},
            {"3D view", "switch to 3D Viewport"},
            {"graph", "switch to / cycle Graph Editors"},
            {"props", "switch to Properties"},
            {"sequence", "switch to / cycle Video Sequencers"},
            {"outliner", "switch to Outliner"},
            {"image", "switch to / cycle Image Editors"},
            {"text", "switch to Text Editor"},
            {"dope", "switch to / cycle Dope Sheets"},
        }};
        for (std::size_t i = 0; i < editors.size(); ++i)
        {
            const auto key = static_cast<K>(static_cast<int>(K::F1) + i);
            add(key, SHIFT, S::Editor, key == K::F5 ? A::ShowViewport : A::ExecuteUnknown,
                editors[i].first, editors[i].second);
        }
        add(K::A, SHIFT, S::Node, A::Menu, "add",
            "add node; search for Reroute to add a control point", 0, false, M::PreserveMode);
        add(K::G, 0, S::Node, A::Move, "grab",
            "move nodes / reroutes; drop on a link to insert; drag output to more inputs to branch",
            U'g');
        add(K::R, 0, S::Node, A::Rotate, "rotate", "rotate node layout", U'r');
        add(K::S, 0, S::Node, A::Scale, "scale", "scale node layout", U's');
        add(K::D, SHIFT, S::Node, A::Duplicate, "dup", "duplicate nodes, then move", U'd');
        add(K::D, OPTION, S::Node, A::Duplicate, "linked",
            "duplicate with shared node groups, then move");
        add(K::D, CONTROL | SHIFT, S::Node, A::Duplicate, "inputs",
            "duplicate nodes while keeping input links");
        for (K key : {K::X, K::Delete})
        {
            add(key, 0, S::Node, A::Complete, "delete", "delete selected nodes / reroutes");
            add(key, CONTROL, S::Node, A::Complete, "reconnect",
                "delete nodes and reconnect their surrounding links");
        }
        add(K::J, 0, S::Node, A::Complete, "link", "connect selected nodes' compatible sockets");
        add(K::J, SHIFT, S::Node, A::Complete, "replace",
            "make links, replacing existing input links");
        add(K::J, CONTROL, S::Node, A::Complete, "frame", "put selected nodes in a frame");
        add(K::F, 0, S::Node, A::Rename, "frame", "create a frame and enter its name");
        add(K::H, 0, S::Node, A::Complete, "collapse", "collapse / expand selected nodes");
        add(K::H, SHIFT, S::Node, A::Complete, "preview", "toggle node preview");
        add(K::H, CONTROL, S::Node, A::Complete, "sockets", "hide / show unused sockets");
        add(K::M, 0, S::Node, A::Complete, "mute", "mute / unmute selected nodes");
        add(K::A, 0, S::Node, A::Complete, "all", "select all nodes");
        add(K::A, OPTION, S::Node, A::Complete, "none", "deselect all nodes");
        add(K::I, CONTROL, S::Node, A::Complete, "invert", "invert node selection");
        add(K::L, 0, S::Node, A::Complete, "inputs",
            "select nodes connected to the selected nodes' inputs");
        add(K::L, SHIFT, S::Node, A::Complete, "outputs",
            "select nodes connected from the selected nodes' outputs");
        for (std::uint8_t modifiers : {SHIFT, static_cast<std::uint8_t>(SHIFT | CONTROL)})
        {
            add(K::G, modifiers, S::Node, A::Menu, "grouped",
                "select nodes with similar properties", 0, false, M::PreserveMode);
        }
        add(K::G, CONTROL, S::Node, A::Complete, "group", "make a node group");
        add(K::G, CONTROL | OPTION, S::Node, A::Complete, "ungroup",
            "ungroup selected node groups");
        add(K::P, 0, S::Node, A::Menu, "separate", "separate selected nodes from the group", 0,
            false, M::PreserveMode);
        add(K::P, CONTROL, S::Node, A::Complete, "parent",
            "attach selected nodes to the active frame");
        add(K::P, OPTION, S::Node, A::Complete, "unparent",
            "detach selected nodes from their frame");
        add(K::Tab, 0, S::Node, A::Complete, "group", "enter / leave the active node group");
        add(K::Tab, CONTROL, S::Node, A::Complete, "exit", "leave the current node group");
        add(K::Tab, SHIFT, S::Node, A::Complete, "snap", "toggle node grid snapping");
        add(K::B, 0, S::Node, A::BoxSelect, "box", "box select nodes");
        add(K::C, 0, S::Node, A::CircleSelect, "circle", "circle select nodes");
        add(K::W, 0, S::Node, A::Complete, "box tool",
            "activate / cycle the node box-selection tool");
        add(K::C, CONTROL, S::Node, A::Complete, "copy", "copy selected nodes and their links");
        add(K::V, CONTROL, S::Node, A::Complete, "paste",
            "paste compatible nodes from the clipboard");
        add(K::S, SHIFT, S::Node, A::Menu, "swap",
            "replace node type, reconnecting compatible sockets", 0, false, M::PreserveMode);
        add(K::S, CONTROL, S::Node, A::PreserveMode, "save", "save project");
        add(K::Z, CONTROL, S::Node, A::ExecuteUnknown, "undo",
            "undo; a fresh Blender report restores the node context");
        add(K::Z, CONTROL | SHIFT, S::Node, A::ExecuteUnknown, "redo",
            "redo; a fresh Blender report restores the node context");
        add(K::F2, 0, S::Node, A::Rename, "rename", "rename active node");
        add(K::F3, 0, S::Node, A::Search, "search",
            "search node operations, e.g. Add Reroute or Cut Links");
        add(K::F, CONTROL, S::Node, A::Rename, "find", "find and select a node by name");
        add(K::T, 0, S::Node, A::Complete, "tools", "toggle node toolbar");
        add(K::N, 0, S::Node, A::Complete, "sidebar", "toggle node sidebar");
        add(K::Home, 0, S::Node, A::Complete, "frame all", "frame all nodes");
        add(K::NumpadPeriod, 0, S::Node, A::Complete, "frame", "frame selected nodes");
        add(K::Grave, 0, S::Node, A::Menu, "views", "node view pie menu", 0, false,
            M::PreserveMode);
        add(K::Space, CONTROL, S::Node, A::Complete, "maximize", "toggle maximized node editor");
        add(K::Space, 0, S::Node, A::Complete, "play", "toggle animation playback");
        add(K::RightBracket, SHIFT, S::Node, A::Complete, "next",
            "activate next node of the same type");
        add(K::LeftBracket, SHIFT, S::Node, A::Complete, "previous",
            "activate previous node of the same type");
        const auto mouse_hint = [&](K key, std::uint8_t modifiers, std::string_view short_hint,
                                    std::string_view full_hint)
        {
            add(key, 0, S::Node, A::Complete, short_hint, full_hint);
            add(key, modifiers, S::Node, A::Complete, short_hint, full_hint);
        };
        for (K key : {K::Shift, K::RightShift})
        {
            mouse_hint(
                key, SHIFT, "reroute",
                "Shift + right-drag across links adds reroute points; drag their output to branch");
        }
        for (K key : {K::Control, K::RightControl})
        {
            mouse_hint(key, CONTROL, "cut links",
                       "Ctrl + right-drag across links cuts them; Ctrl + left-drag detaches a "
                       "socket link");
            add(key, CONTROL | OPTION, S::Node, A::Complete, "mute links",
                "Ctrl-Alt + right-drag mutes / unmutes crossed links");
        }
        for (K key : {K::Option, K::RightOption})
        {
            mouse_hint(key, OPTION, "detach",
                       "Alt + left-drag moves a node while detaching its links");
            add(key, CONTROL | OPTION, S::Node, A::Complete, "lasso",
                "Ctrl-Alt + left-drag selects nodes with a lasso");
            add(key, CONTROL | OPTION | SHIFT, S::Node, A::Complete, "deselect",
                "Ctrl-Alt-Shift + left-drag deselects nodes with a lasso");
            add(key, 0, S::NodeMove, A::Complete, "attach",
                "hold Alt to toggle automatic insertion into a link", 0, true);
            add(key, 0, S::NodeLink, A::Complete, "swap",
                "hold Alt while dragging a link to swap compatible connections", 0, true);
        }
        for (K key : {K::Shift, K::RightShift})
        {
            add(key, 0, S::NodeTransform, A::Complete, "precision",
                "hold Shift for precise node layout transforms", 0, true);
        }
        for (K key : {K::Control, K::RightControl})
        {
            add(key, 0, S::NodeTransform, A::Complete, "snap",
                "hold Control to invert node grid snapping", 0, true);
            add(key, 0, S::NodeResize, A::Complete, "snap",
                "hold Control to invert node-width snapping", 0, true);
        }
        for (K key : {K::X, K::Y})
        {
            add(key, 0, S::NodeTransform, A::Axis, "axis",
                "constrain node movement / scale to this axis");
        }
        add(K::C, 0, S::NodeTransform, A::ClearConstraint, "free",
            "clear the node transform constraint");
        add(K::Tab, SHIFT, S::NodeTransform, A::Complete, "snap", "toggle node grid snapping");
        add(K::Space, 0, S::NodeTransform, A::Confirm, "confirm", "confirm node transform", 0,
            true);
        add(K::G, 0, S::NodeTransform, A::Move, "move", "switch to moving nodes");
        add(K::R, 0, S::NodeTransform, A::Rotate, "rotate", "switch to rotating node layout");
        add(K::S, 0, S::NodeTransform, A::Scale, "scale", "switch to scaling node layout");
        add(K::F, 0, S::NodeMove, A::Complete, "frame",
            "toggle attaching nodes to the frame under the pointer");
        add(K::T, 0, S::NodeMove, A::Complete, "offset",
            "toggle auto-offset direction when inserting a node into a link");
        add(K::V, 0, S::Compositor, A::Complete, "zoom out", "zoom the compositor backdrop out");
        add(K::V, OPTION, S::Compositor, A::Complete, "zoom in", "zoom the compositor backdrop in");
        add(K::Home, OPTION, S::Compositor, A::Complete, "fit",
            "fit compositor backdrop to the editor");
        add(K::R, CONTROL, S::Compositor, A::Complete, "layers",
            "refresh compositor scene / view-layer information");
        add(K::Z, 0, S::Compositor, A::Complete, "render", "re-render compositor output");
        add(K::G, 0, S::Curve, A::Move, "grab",
            "move control points / handles with the mouse; points can also be click-dragged", U'g');
        add(K::E, 0, S::Curve, A::Extrude, "extr",
            "add connected control points, then move; canceling movement leaves new points", U'e');
        add(K::D, SHIFT, S::Curve, A::Duplicate, "dup",
            "duplicate selected points, then move; canceling movement leaves the duplicate", U'd');
        add(K::A, SHIFT, S::Curve, A::Menu, "add", "add a spline to this curve", 0, false,
            M::PreserveMode);
        add(K::V, 0, S::Curve, A::Menu, "handles",
            "set Bezier handle type: Automatic, Vector, Aligned or Free", 0, false,
            M::PreserveMode);
        for (K key : {K::X, K::Delete})
        {
            add(key, 0, S::Curve, A::Menu, "delete", "delete selected control points / segments", 0,
                false, M::PreserveMode);
            add(key, CONTROL, S::Curve, A::Complete, "dissolve",
                "remove control points while approximating the original curve shape");
        }
        add(K::F, 0, S::Curve, A::Complete, "connect", "connect selected endpoints with a segment");
        add(K::Y, 0, S::Curve, A::Complete, "split", "split selected points from their spline");
        add(K::P, 0, S::Curve, A::Complete, "separate",
            "separate selection into a new curve object");
        add(K::C, OPTION, S::Curve, A::Complete, "close", "toggle an open / closed spline");
        add(K::T, CONTROL, S::Curve, A::Tilt, "tilt", "tilt selected control points", U't');
        add(K::T, OPTION, S::Curve, A::Complete, "untilt", "clear tilt on selected control points");
        add(K::S, OPTION, S::Curve, A::Radius, "radius", "adjust control-point radius");
        add(K::N, SHIFT, S::Curve, A::Complete, "handles", "recalculate curve handles / normals");
        add(K::L, 0, S::Curve, A::Complete, "linked", "select the spline under the pointer", U'l');
        add(K::L, CONTROL, S::Curve, A::Complete, "linked", "select connected control points",
            U'l');
        add(K::L, SHIFT, S::Curve, A::Complete, "unlink", "deselect the spline under the pointer");
        add(K::G, SHIFT, S::Curve, A::Menu, "similar", "select similar control points", 0, false,
            M::PreserveMode);
        add(K::NumpadPlus, CONTROL, S::Curve, A::Complete, "more", "grow control-point selection");
        add(K::NumpadMinus, CONTROL, S::Curve, A::Complete, "less",
            "shrink control-point selection");
        add(K::O, OPTION, S::Curve, A::Complete, "connected",
            "toggle proportional editing affecting only connected points");
        add(K::O, SHIFT, S::Curve, A::Menu, "falloff", "proportional editing falloff pie", 0, false,
            M::PreserveMode);
        add(K::H, CONTROL, S::Curve, A::Menu, "hooks", "hook selected points to an object", 0,
            false, M::PreserveMode);
        add(K::P, CONTROL, S::Curve, A::Complete, "parent",
            "parent selected objects to curve points");
        add(K::F3, 0, S::Curve, A::Search, "search",
            "search curve operations, e.g. Subdivide or Switch Direction");
        for (K key : {K::Control, K::RightControl})
        {
            for (std::uint8_t modifiers : {std::uint8_t{0}, CONTROL})
            {
                add(key, modifiers, S::Curve, A::Complete, "add point",
                    "Ctrl + right click: add a control point; Ctrl + left click: select shortest "
                    "path");
            }
        }
        add(K::Tab, 0, S::Idle, A::ToggleMode, "mode", "toggle Object / Edit Mode");
        add(K::G, 0, S::ObjectOrMesh, A::Move, "grab", "move selection", U'g');
        add(K::R, 0, S::Idle, A::Rotate, "rotate", "rotate selection", U'r');
        add(K::S, 0, S::Idle, A::Scale, "scale", "scale selection", U's');
        add(K::A, 0, S::Idle, A::Complete, "all", "select all", U'a');
        add(K::A, OPTION, S::Idle, A::Complete, "none", "deselect all");
        add(K::I, CONTROL, S::Idle, A::Complete, "invert", "invert selection", U'i');
        add(K::H, 0, S::Idle, A::Complete, "hide", "hide selection", U'h');
        add(K::H, SHIFT, S::Idle, A::Complete, "others", "hide unselected");
        add(K::H, OPTION, S::Idle, A::Complete, "reveal", "reveal hidden geometry");
        add(K::S, CONTROL, S::Idle, A::PreserveMode, "save",
            "save project; the first save may open a file browser");
        add(K::Space, 0, S::Idle, A::Complete, "play", "toggle animation playback");
        add(K::Space, CONTROL | SHIFT, S::Idle, A::Complete, "reverse",
            "toggle reverse animation playback");
        add(K::Space, CONTROL, S::Idle, A::Complete, "maximize", "toggle maximized viewport");
        add(K::Left, 0, S::Idle, A::Complete, "frame -", "previous frame");
        add(K::Right, 0, S::Idle, A::Complete, "frame +", "next frame");
        add(K::Left, SHIFT, S::Idle, A::Complete, "first", "jump to first frame");
        add(K::Right, SHIFT, S::Idle, A::Complete, "last", "jump to last frame");
        add(K::Up, 0, S::Idle, A::Complete, "next key", "next keyframe");
        add(K::Down, 0, S::Idle, A::Complete, "prev key", "previous keyframe");
        add(K::Z, OPTION, S::Idle, A::Complete, "x-ray", "toggle viewport X-ray");
        add(K::Z, SHIFT, S::Idle, A::Complete, "wire", "toggle wireframe / solid shading");
        add(K::Grave, CONTROL, S::Idle, A::Complete, "gizmos", "toggle viewport gizmos");
        add(K::Z, CONTROL, S::Idle, A::ExecuteUnknown, "undo",
            "undo; mode or previous operation may change");
        add(K::Z, CONTROL | SHIFT, S::Idle, A::ExecuteUnknown, "redo",
            "redo; mode or previous operation may change");
        add(K::T, 0, S::Idle, A::Complete, "tools", "toggle toolbar", U't');
        add(K::N, 0, S::Idle, A::Complete, "sidebar", "toggle sidebar");
        add(K::O, 0, S::Idle, A::Complete, "prop.", "toggle proportional editing", U'o');
        add(K::Tab, SHIFT, S::Idle, A::Complete, "snap", "toggle snapping");
        add(K::Slash, 0, S::Idle, A::Complete, "local", "toggle local view");
        add(K::Slash, OPTION, S::Idle, A::Complete, "remove", "remove selection from local view");
        add(K::Home, 0, S::Idle, A::Complete, "frame all", "frame all objects");
        add(K::C, SHIFT, S::Idle, A::Complete, "frame all", "frame all and center 3D cursor");
        add(K::F2, 0, S::Idle, A::Rename, "rename", "rename active object");
        add(K::F3, 0, S::ObjectOrMesh, A::Search, "search", "search Blender operators");
        add(K::Q, 0, S::Idle, A::Menu, "fav", "quick favorites");
        add(K::Z, 0, S::Idle, A::Menu, "shading", "shading pie menu", 0, false, M::PreserveMode);
        add(K::Grave, 0, S::Idle, A::Menu, "views", "view pie menu", 0, false, M::PreserveMode);
        add(K::Period, 0, S::Idle, A::Menu, "pivot", "pivot point pie menu", 0, false,
            M::PreserveMode);
        add(K::Comma, 0, S::Idle, A::Menu, "orient", "transform orientation pie menu", 0, false,
            M::PreserveMode);
        add(K::Tab, CONTROL, S::Idle, A::Menu, "modes",
            "mode pie menu; selected mode needs a baseline");
        add(K::A, SHIFT, S::ObjectOrMesh, A::Menu, "add", "add geometry / object menu");
        add(K::S, SHIFT, S::Idle, A::Menu, "cursor", "selection / 3D cursor snapping pie", 0, false,
            M::PreserveMode);
        add(K::D, SHIFT, S::ObjectOrMesh, A::Duplicate, "dup",
            "duplicate then move; canceling movement can leave the duplicate", U'd');
        add(K::G, OPTION, S::Object, A::Complete, "loc", "clear object location");
        add(K::R, OPTION, S::Object, A::Complete, "rot", "clear object rotation");
        add(K::S, OPTION, S::Object, A::Complete, "scale", "clear object scale");
        add(K::D, OPTION, S::Object, A::Duplicate, "linked", "linked duplicate then move");
        add(K::A, CONTROL, S::Object, A::Menu, "apply", "apply object transforms menu", 0, false,
            M::PreserveMode);
        for (int level = 0; level <= 5; ++level)
        {
            add(static_cast<K>(static_cast<int>(K::Zero) + level), CONTROL, S::Object, A::Complete,
                "subdiv", "set subdivision modifier level from this number key");
        }
        add(K::J, CONTROL, S::Object, A::Complete, "join", "join selected objects", U'j');
        add(K::X, 0, S::Object, A::DeleteConfirm, "delete", "delete objects confirmation");
        add(K::X, SHIFT, S::Object, A::DeleteConfirm, "delete all",
            "delete objects globally confirmation");
        add(K::Delete, 0, S::Object, A::Complete, "delete", "delete selected objects");
        add(K::M, 0, S::Object, A::Menu, "collect", "move to collection menu");
        add(K::P, CONTROL, S::Object, A::Menu, "parent", "set parent menu");
        add(K::G, SHIFT, S::Object, A::Menu, "grouped", "select grouped menu");
        add(K::B, 0, S::Idle, A::BoxSelect, "box", "box select", U'b');
        add(K::C, 0, S::Idle, A::CircleSelect, "circle", "circle select", U'c');
        add(K::One, 0, S::Mesh, A::SelectVertices, "verts", "select vertices");
        add(K::Two, 0, S::Mesh, A::SelectEdges, "edges", "select edges");
        add(K::Three, 0, S::Mesh, A::SelectFaces, "faces", "select faces");
        add(K::One, SHIFT, S::Mesh, A::SelectVertices, "verts", "toggle vertex selection mode");
        add(K::Two, SHIFT, S::Mesh, A::SelectEdges, "edges", "toggle edge selection mode");
        add(K::Three, SHIFT, S::Mesh, A::SelectFaces, "faces", "toggle face selection mode");
        for (const auto modifiers : {CONTROL, static_cast<std::uint8_t>(CONTROL | SHIFT)})
        {
            add(K::One, modifiers, S::Mesh, A::SelectVertices, "verts",
                "convert selection to vertices; Shift also extends selection mode");
            add(K::Two, modifiers, S::Mesh, A::SelectEdges, "edges",
                "convert selection to edges; Shift also extends selection mode");
            add(K::Three, modifiers, S::Mesh, A::SelectFaces, "faces",
                "convert selection to faces; Shift also extends selection mode");
        }
        add(K::E, 0, S::Mesh, A::Extrude, "extr",
            "extrude then move; canceling movement can leave new geometry", U'e');
        add(K::I, 0, S::Mesh, A::Inset, "inset", "inset selected faces", U'i');
        add(K::B, CONTROL, S::Mesh, A::Bevel, "bevel", "bevel selected edges", U'b');
        add(K::B, CONTROL | SHIFT, S::Mesh, A::Bevel, "verts", "bevel selected vertices", U'b');
        add(K::K, 0, S::Mesh, A::Knife, "knife", "knife cutting tool", U'k');
        add(K::K, SHIFT, S::Mesh, A::Knife, "thru", "knife through selected geometry", U'k');
        add(K::R, CONTROL, S::Mesh, A::LoopCut, "loop", "loop cut, then slide the new edge loop");
        add(K::F, 0, S::Mesh, A::Complete, "fill", "make edge / face", U'f');
        add(K::J, 0, S::Mesh, A::Complete, "connect", "connect vertices");
        add(K::Y, 0, S::Mesh, A::Complete, "split", "split selection");
        add(K::L, 0, S::Mesh, A::Complete, "linked", "select linked under the pointer", U'l');
        add(K::L, CONTROL, S::Mesh, A::Complete, "linked", "select all linked geometry", U'l');
        add(K::L, SHIFT, S::Mesh, A::Complete, "unlink", "deselect linked under the pointer");
        add(K::N, SHIFT, S::Mesh, A::Complete, "normals", "recalculate normals outside");
        add(K::X, CONTROL, S::Mesh, A::Complete, "dissolve", "dissolve selection");
        add(K::M, 0, S::Mesh, A::Menu, "merge", "merge selection menu", U'm', false,
            M::PreserveMode);
        add(K::X, 0, S::Mesh, A::Menu, "delete", "delete mesh components menu", 0, false,
            M::PreserveMode);
        add(K::Delete, 0, S::Mesh, A::Menu, "delete", "delete mesh components menu", 0, false,
            M::PreserveMode);
        add(K::P, 0, S::Mesh, A::Menu, "separate", "separate geometry menu");
        add(K::U, 0, S::Mesh, A::Menu, "UV", "UV mapping menu");
        add(K::E, OPTION, S::Mesh, A::Menu, "extrude menu", "extrude options menu");
        add(K::Enter, 0, S::Modal, A::Confirm, "confirm", "confirm operation", 0, true);
        add(K::NumpadEnter, 0, S::Modal, A::Confirm, "confirm", "confirm operation", 0, true);
        add(K::Escape, 0, S::Modal, A::Cancel, "cancel",
            "cancel operation; return to the current Object / Edit Mode", 0, true);
        add(K::Space, 0, S::TransformInput, A::Confirm, "confirm", "confirm transform", 0, true);
        add(K::Space, 0, S::Knife, A::Confirm, "confirm", "confirm cuts", 0, true);
        for (K key : {K::X, K::Y, K::Z})
        {
            add(key, 0, S::Transform, A::Axis, "axis",
                "constrain axis; repeat cycles orientation then clears",
                static_cast<char32_t>(key) + 32);
            add(key, SHIFT, S::Transform, A::Axis, "plane",
                "constrain the plane perpendicular to this axis");
            add(key, 0, S::Knife, A::Axis, "axis", "knife axis constraint");
        }
        add(K::C, 0, S::Transform, A::ClearConstraint, "free", "clear transform constraint");
        add(K::G, 0, S::Transform, A::Move, "move",
            "switch to move; repeated mesh move may invoke slide");
        add(K::R, 0, S::Transform, A::Rotate, "rotate", "rotate; repeat for trackball rotation");
        add(K::S, 0, S::Transform, A::Scale, "scale", "switch to scale");
        add(K::Tab, SHIFT, S::TransformInput, A::Complete, "snap", "toggle snapping");
        add(K::A, 0, S::Transform, A::Complete, "snap", "add snap point");
        add(K::A, CONTROL, S::Transform, A::Complete, "snap", "add snap point");
        add(K::A, OPTION, S::Transform, A::Complete, "unsnap", "remove snap point");
        add(K::PageUp, 0, S::TransformInput, A::Complete, "rad +",
            "increase proportional influence radius");
        add(K::PageDown, 0, S::TransformInput, A::Complete, "rad -",
            "decrease proportional influence radius");
        add(K::PageUp, SHIFT, S::TransformInput, A::Complete, "rad +",
            "increase proportional influence radius");
        add(K::PageDown, SHIFT, S::TransformInput, A::Complete, "rad -",
            "decrease proportional influence radius");
        add(K::B, 0, S::Transform, A::Complete, "snap source", "toggle editing the snap source");
        for (K key : {K::Shift, K::RightShift})
        {
            add(key, 0, S::TransformInput, A::Complete, "precision", "hold Shift for precision", 0,
                true);
        }
        for (K key : {K::Control, K::RightControl})
        {
            add(key, 0, S::TransformInput, A::Complete, "snap", "hold Control to invert snapping",
                0, true);
        }
        for (int digit = 0; digit < 10; ++digit)
        {
            add(static_cast<K>(static_cast<int>(K::Zero) + digit), 0, S::TransformInput, A::Numeric,
                "value", "enter a numeric transform value");
            add(static_cast<K>(static_cast<int>(K::NumpadZero) + digit), 0, S::TransformInput,
                A::Numeric, "value", "enter a numeric transform value");
            for (S scope : {S::Bevel, S::Inset, S::NodeTransform})
            {
                add(static_cast<K>(static_cast<int>(K::Zero) + digit), 0, scope, A::Numeric,
                    "value", "enter a numeric tool value");
                add(static_cast<K>(static_cast<int>(K::NumpadZero) + digit), 0, scope, A::Numeric,
                    "value", "enter a numeric tool value");
            }
        }
        for (K key : {K::Minus, K::Period, K::NumpadMinus, K::NumpadPeriod, K::Backspace})
        {
            for (S scope : {S::TransformInput, S::NodeTransform, S::Bevel, S::Inset})
            {
                if (scope == S::Bevel && key == K::NumpadMinus)
                {
                    continue; // Bevel uses numpad +/- for its segment count.
                }
                add(key, 0, scope, A::Numeric, "value",
                    "edit numeric input; values are not retained");
            }
        }
        for (S scope : {S::TransformInput, S::NodeTransform, S::Bevel, S::Inset})
        {
            for (K key : {K::Slash, K::NumpadDivide, K::NumpadMultiply, K::NumpadPlus})
            {
                if (scope == S::Bevel && key == K::NumpadPlus)
                {
                    continue;
                }
                add(key, 0, scope, A::Numeric, "value", "enter numeric arithmetic");
            }
            add(K::Equal, SHIFT, scope, A::Numeric, "value", "enter numeric addition");
            add(K::Eight, SHIFT, scope, A::Numeric, "value", "enter numeric multiplication");
        }
        for (auto [key, short_hint, full_hint] : {std::tuple{K::A, "offset", "adjust bevel offset"},
                                                  {K::P, "profile", "adjust bevel profile"},
                                                  {K::S, "segments", "adjust bevel segments"},
                                                  {K::M, "width", "cycle bevel width type"},
                                                  {K::C, "clamp", "toggle overlap clamping"},
                                                  {K::V, "affect", "toggle bevel vertices / edges"},
                                                  {K::H, "normals", "toggle hardened normals"},
                                                  {K::U, "seam", "toggle marking seams"},
                                                  {K::K, "sharp", "toggle marking sharp edges"},
                                                  {K::O, "outer", "cycle outer miter"},
                                                  {K::I, "inner", "cycle inner miter"},
                                                  {K::Z, "profile", "cycle profile type"},
                                                  {K::N, "v.mesh", "cycle vertex mesh method"}})
        {
            add(key, 0, S::Bevel, A::Complete, short_hint, full_hint, 0, true);
        }
        add(K::NumpadPlus, 0, S::Bevel, A::Complete, "segments +", "increase bevel segments", 0,
            true);
        add(K::NumpadMinus, 0, S::Bevel, A::Complete, "segments -", "decrease bevel segments", 0,
            true);
        // Inset-specific controls are deliberately bounded; unknown tool keys require a baseline.
        add(K::B, 0, S::Inset, A::Complete, "boundary", "toggle inset boundary");
        add(K::I, 0, S::Inset, A::Complete, "individual", "toggle individual face inset");
        add(K::O, 0, S::Inset, A::Complete, "outset", "toggle inset / outset");
        add(K::Z, CONTROL, S::Knife, A::Complete, "undo cut", "undo last knife point");
        add(K::A, 0, S::Knife, A::Complete, "angle", "toggle knife angle snapping");
        add(K::R, 0, S::Knife, A::Complete, "snap edge", "cycle knife angle reference edge");
        add(K::C, 0, S::Knife, A::Complete, "through", "toggle cut through");
        add(K::S, 0, S::Knife, A::Complete, "measure", "toggle distance / angle display");
        add(K::V, 0, S::Knife, A::Complete, "depth", "toggle depth testing");
        for (K key : {K::Shift, K::RightShift})
        {
            add(key, 0, S::Knife, A::Complete, "midpoints", "hold Shift to snap to midpoints", 0,
                true);
        }
        for (K key : {K::Control, K::RightControl})
        {
            add(key, 0, S::Knife, A::Complete, "no snap", "hold Control to ignore snapping", 0,
                true);
        }
        for (int digit = 0; digit < 10; ++digit)
        {
            add(static_cast<K>(static_cast<int>(K::Zero) + digit), 0, S::LoopCut, A::Numeric,
                "cuts", "enter number of loop cuts");
            add(static_cast<K>(static_cast<int>(K::NumpadZero) + digit), 0, S::LoopCut, A::Numeric,
                "cuts", "enter number of loop cuts");
        }
        add(K::Enter, 0, S::LoopCut, A::Confirm, "cut", "confirm loop cut and start edge slide", 0,
            true);
        add(K::NumpadEnter, 0, S::LoopCut, A::Confirm, "cut",
            "confirm loop cut and start edge slide", 0, true);
        add(K::Space, 0, S::BoxSelect, A::Complete, "move box",
            "hold Space to move the selection box", 0, true);
        add(K::NumpadPlus, 0, S::CircleSelect, A::Complete, "rad +",
            "increase circle selection radius");
        add(K::NumpadMinus, 0, S::CircleSelect, A::Complete, "rad -",
            "decrease circle selection radius");
        add(K::Escape, 0, S::Menu, A::Cancel, "cancel", "dismiss menu", 0, true);
        add(K::Enter, 0, S::Menu, A::ExecuteUnknown, "choose",
            "choose menu item; unrecognized actions may need a mode resync", 0, true);
        add(K::NumpadEnter, 0, S::Menu, A::ExecuteUnknown, "choose",
            "choose menu item; unrecognized actions may need a mode resync", 0, true);
        add(K::Up, 0, S::Menu, A::Complete, "previous", "previous menu item");
        add(K::Down, 0, S::Menu, A::Complete, "next", "next menu item");
        add(K::Left, 0, S::Menu, A::Complete, "parent", "return to parent menu");
        add(K::Right, 0, S::Menu, A::Complete, "submenu", "open submenu");
        add(K::Escape, 0, S::Text, A::Cancel, "cancel", "cancel text input", 0, true);
        add(K::Enter, 0, S::Text, A::Confirm, "accept", "accept text; search results need resync",
            0, true);
        add(K::Escape, 0, S::DeleteConfirm, A::Cancel, "cancel", "cancel deletion", 0, true);
        add(K::Enter, 0, S::DeleteConfirm, A::Confirm, "delete", "confirm object deletion", 0,
            true);
        for (auto [key, short_hint, full_hint] :
             {std::tuple{K::NumpadZero, "camera", "toggle camera view"},
              {K::NumpadOne, "front", "front view"},
              {K::NumpadTwo, "orbit down", "orbit down"},
              {K::NumpadThree, "right", "right view"},
              {K::NumpadFour, "orbit left", "orbit left"},
              {K::NumpadFive, "perspective", "toggle perspective / orthographic"},
              {K::NumpadSix, "orbit right", "orbit right"},
              {K::NumpadSeven, "top", "top view"},
              {K::NumpadEight, "orbit up", "orbit up"},
              {K::NumpadNine, "opposite", "orbit to opposite view"},
              {K::NumpadPeriod, "frame", "frame selection"},
              {K::NumpadDivide, "local", "toggle local view"},
              {K::NumpadPlus, "zoom in", "zoom in"},
              {K::NumpadMinus, "zoom out", "zoom out"}})
        {
            add(key, 0, S::Idle, A::Complete, short_hint, full_hint);
        }
        add(K::NumpadOne, CONTROL, S::Idle, A::Complete, "back", "back view");
        add(K::NumpadThree, CONTROL, S::Idle, A::Complete, "left", "left view");
        add(K::NumpadSeven, CONTROL, S::Idle, A::Complete, "bottom", "bottom view");
        return result;
    }();
    return bindings;
}

bool IsBlenderTransform(BlenderOperation operation)
{
    switch (operation)
    {
        case BlenderOperation::Move:
        case BlenderOperation::Rotate:
        case BlenderOperation::Trackball:
        case BlenderOperation::Scale:
        case BlenderOperation::Extrude:
        case BlenderOperation::Duplicate:
        case BlenderOperation::EdgeSlide:
            return true;
        default:
            return false;
    }
}

bool IsBlenderNodeTransform(BlenderOperation operation)
{
    using O = BlenderOperation;
    return operation == O::Move || operation == O::Rotate || operation == O::Scale ||
           operation == O::Duplicate || operation == O::NodeDetach;
}

bool IsBlenderNodeMouseTool(BlenderOperation operation)
{
    using O = BlenderOperation;
    return operation == O::NodeLink || operation == O::NodeCut || operation == O::NodeReroute ||
           operation == O::NodeMuteLinks || operation == O::NodeResize || operation == O::NodeLasso;
}

bool MacSystemShortcut(Platform::KeyId key, std::uint8_t modifiers)
{
    using K = Platform::KeyId;
    // macOS defaults: Show Desktop, and keyboard navigation focus on Control-F1...F8.
    return (key == K::F11 && modifiers == 0) ||
           (key >= K::F1 && key <= K::F8 && modifiers == Platform::CONTROL);
}

bool BlenderBindingAvailable(const BlenderBinding& binding, const BlenderState& state)
{
    if (state.confidence == Confidence::Uncertain || state.editor == BlenderEditor::Unsupported ||
        (state.editor == BlenderEditor::Viewport && state.mode == BlenderMode::Unsupported) ||
        (state.observation.connected &&
         (ClassifyBlenderEditor(state.observation.editor.data()) == BlenderEditor::Unsupported ||
          state.observation.interfaceLocked)) ||
        (binding.macOnly && !state.macCommandAliases) ||
        (state.macCommandAliases && MacSystemShortcut(binding.key, binding.modifiers)))
    {
        return false;
    }
    const bool canvas = state.context == BlenderContext::Editor;
    const bool viewport = canvas && state.editor == BlenderEditor::Viewport;
    const bool nodes = canvas && state.editor == BlenderEditor::Nodes;
    const bool idle = viewport && state.operation == BlenderOperation::None;
    using S = BlenderScope;
    using O = BlenderOperation;
    using C = BlenderContext;
    switch (binding.scope)
    {
        case S::Editor:
            return canvas && state.operation == BlenderOperation::None;
        case S::Idle:
            return idle;
        case S::Object:
            return idle && state.mode == BlenderMode::Object;
        case S::Mesh:
            return idle && state.mode == BlenderMode::MeshEdit;
        case S::ObjectOrMesh:
            return idle &&
                   (state.mode == BlenderMode::Object || state.mode == BlenderMode::MeshEdit);
        case S::Curve:
            return idle && state.mode == BlenderMode::CurveEdit;
        case S::Node:
            return nodes && state.operation == O::None;
        case S::Compositor:
            return nodes && state.operation == O::None &&
                   std::string_view(state.observation.editor.data()) == "CompositorNodeTree";
        case S::NodeTransform:
            return nodes && IsBlenderNodeTransform(state.operation) &&
                   ((binding.action != BlenderAction::Axis &&
                     binding.action != BlenderAction::ClearConstraint) ||
                    state.operation != O::Rotate);
        case S::NodeMove:
            return nodes && (state.operation == O::Move || state.operation == O::Duplicate ||
                             state.operation == O::NodeDetach);
        case S::NodeLink:
            return nodes && state.operation == O::NodeLink;
        case S::NodeResize:
            return nodes && state.operation == O::NodeResize;
        case S::Transform:
            return viewport && IsBlenderTransform(state.operation);
        case S::TransformInput:
            return viewport && (IsBlenderTransform(state.operation) || state.operation == O::Tilt ||
                                state.operation == O::Radius);
        case S::Modal:
            return canvas && state.operation != O::None &&
                   (binding.action == BlenderAction::Cancel ||
                    (state.operation != O::LoopCut && state.operation != O::BoxSelect &&
                     !IsBlenderNodeMouseTool(state.operation)));
        case S::Knife:
            return viewport && state.operation == O::Knife;
        case S::Bevel:
            return viewport && state.operation == O::Bevel;
        case S::Inset:
            return viewport && state.operation == O::Inset;
        case S::LoopCut:
            return viewport && state.operation == O::LoopCut;
        case S::BoxSelect:
            return canvas && state.operation == O::BoxSelect;
        case S::CircleSelect:
            return canvas && state.operation == O::CircleSelect;
        case S::Menu:
            return state.context == C::Menu;
        case S::Text:
            return state.context == C::Search || state.context == C::Rename;
        case S::DeleteConfirm:
            return state.context == C::DeleteConfirm;
    }
    return false;
}

HintId FindBlenderBinding(const BlenderState& state, Platform::KeyId key, std::uint8_t modifiers)
{
    const auto bindings = BlenderBindings();
    for (HintId id = 0; id < bindings.size(); ++id)
    {
        if (bindings[id].key == key &&
            (bindings[id].anyModifiers || bindings[id].modifiers == modifiers) &&
            BlenderBindingAvailable(bindings[id], state))
        {
            return id;
        }
    }
    return NO_HINT;
}
