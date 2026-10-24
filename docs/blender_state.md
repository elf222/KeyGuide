# Blender state bridge

The optional add-on reports Blender's real mode, mesh selection type, editor under
the pointer and running modal operator. Keyboard input still uses the existing
listen-only observer. The add-on registers no operators, keymaps or modal input
handlers and never executes editing commands or changes scene data.

## Enable it once

1. Run the rebuilt app. Choose **Keybindings ▸ Reveal Blender Add-on…** to reveal
   `keyguide_bridge.py`.
2. In Blender, open **Edit → Preferences → Add-ons**, open the top-right menu and
   choose **Install from Disk…**. Select that `.py` file. Blender 4.1 calls this
   **Install…**. This legacy single-file add-on needs no online extensions service.
3. Enable **KeyGuide State Bridge**. Keep **Keybindings ▸ Read Blender State** checked.
   With Input Monitoring enabled, return to a supported Blender editor and allow a few
   seconds to connect. The status lines at the top of the **KeyGuide** menu confirm the connection.

Blender's preference-saving rules control whether the add-on remains enabled after
restarting. Rebuilding the overlay does not update an installed copy: reinstall
the `.py` file when the add-on itself changes.

## Recovery and limits

- Object / Mesh Edit / Curve Edit Mode and mesh vertex/edge/face selection follow Blender itself,
  including mouse actions, undo/redo and mode menus.
- Geometry, Shader and Compositor node editors have their own hints. Node group
  navigation does not change the tracked object edit mode. Bridge **1.1** identifies
  the graph type; the older bridge reports a generic **NODES** context.
- On versions with `Window.modal_operators`, tools started from menus are detected.
  When an unknown tool finishes, its empty modal list restores the real base mode.
- Other modes, such as Sculpt and Pose, show their actual mode name. Other editors
  show their editor name. Unsupported contexts have no command hints; pressed keys still
  appear. Returning to supported mesh or curve work restores its hints.
- A fresh valid report recovers supported contexts after connection loss or an
  interface lock. Pausing observation also pauses state reading.

Axis constraints, numeric entry and popup navigation remain inferred. An empty
modal list does not prove that a menu/search/rename popup has closed. An uncertain
popup can therefore still show **RESYNC** until Escape is pressed over Blender.
Only pressed keys appear while uncertain. Blender 4.1 confirms the base mode but
cannot report running tools; Escape clears remaining tool uncertainty. API
availability is checked directly instead of assumed from a version number.

Editor detection uses native window bounds and Blender area/region rectangles,
including Retina backing pixels. Viewport and node editor headers/sidebars suppress hints.
Hovering the overlay retains the last Blender location and freezes its displayed
mode, pending state, hints and descriptions. The live tracker continues to read
Blender; moving the pointer out immediately displays the latest state. Physical
keys and modifier inspection remain live. Pointer-only updates use
the existing one-second health check; keyboard/mouse-button input requests the
current pointer immediately. Unmatched window layouts report an unknown editor.

Without the add-on, or with **Read Blender State** disabled, passive inference and
the **Resync Mode** submenu remain available. Local preview never uses a live bridge.

## Curve Edit controls

`EDIT_CURVE` now displays **CURVE EDIT** and uses the default Bezier / NURBS curve
bindings checked against the installed Blender **5.2.2 LTS** keymap. The existing
add-on already reports this mode; no add-on reinstall is needed for these hints.
Without state reading, choose **Resync Mode ▸ Curve Edit Mode · 3D Viewport**.
`Tab` remembers the most recently observed/assumed edit mode for the next entry.
Selecting another object can change its actual edit type; the next fresh Blender
report corrects the passive prediction.

| Keys / mouse | Action |
|---|---|
| `G`, or click-drag a selected point | Move control points / handles; `R` rotates and `S` scales. |
| `E` / `Shift-D` | Extrude connected points / duplicate, then move. Canceling movement can leave the new points. |
| `Ctrl` + right click | Add a control point at the pointer. The Control key reveals this mouse hint. |
| `Shift-A` | Add a spline to the current curve. |
| `X` / `Delete` | Delete points or segments menu. |
| `Ctrl-X` / `Ctrl-Delete` | Dissolve points while approximating the original curve shape. |
| `F` / `Y` / `P` | Connect endpoints / split / separate into another curve object. |
| `Alt-C` | Toggle open / closed spline. |
| `V` / `Shift-N` | Bezier handle type / recalculate handles. |
| `Ctrl-T` / `Alt-T` / `Alt-S` | Tilt / clear tilt / adjust control-point radius. |
| `A` / `Alt-A` / `Ctrl-I` | Select all / none / invert. |
| `L` / `Shift-L` / `Ctrl-L` | Select spline under pointer / deselect it / select linked points. |
| `Ctrl` + left click / `Shift-G` | Shortest-path selection / select similar points. |
| `Ctrl-Numpad+` / `Ctrl-Numpad-` | Grow / shrink selection. Enable the optional numpad to see these keys. |
| `H` / `Shift-H` / `Alt-H` | Hide selected / hide others / reveal. |
| `O` / `Alt-O` / `Shift-O` | Proportional editing / connected-only influence / falloff pie. |
| `Shift-Tab` / `Shift-S` | Toggle snapping / selection and 3D cursor snap pie. |
| `F3` | Search operations such as Subdivide or Switch Direction. These have no direct default key binding. |

Mouse bindings assume default left-click selection; keyboard hints also include
Blender's Mac Command aliases where defined. Transform hints include numeric entry,
Shift precision, Control snapping and confirmation/cancellation. Tilt and radius
have no axis constraints, so axis/plane hints stay hidden there. Mesh-only hints
such as inset, bevel and vertex/edge/face selection stay hidden in Curve Edit.
The separate modern `EDIT_CURVES` mode is not covered by these bindings.

The bridge identifies Tilt, curve Extrude/Duplicate and mouse-started Move from
their real operator IDs. Radius uses Blender's generic `transform.transform`
operator: its radius subtype is inferred from `Alt-S`, then cleared when the real
operator ends. Another generic tool is not guessed to be Radius. Popup choices
remain inferred as above. No additional state polling or redraw timer was added.

References: [control-point editing](https://docs.blender.org/manual/en/latest/modeling/curves_new/editing/control_points.html),
[snapping](https://docs.blender.org/manual/en/latest/editors/3dview/controls/snapping.html).

## Node editor controls

Move the pointer into the node canvas. Space shows **GEOMETRY NODES**, **SHADER
NODES**, or **COMPOSITOR** with bridge 1.1, and **NODES** with the old bridge or a
manual **Resync Mode ▸ Node Editor** selection. Hover the Shift,
Control and Option keys for the full mouse instructions. The aliases follow
Blender's default Mac keymap; Alt means Option on Mac.

The curved connections are **links**. Their extra control points are **Reroute
nodes**. To branch a connection, drag from an output socket (including a reroute
output) to another input socket; keep existing output connections. Inputs normally
accept one link, with exceptions for multi-input sockets. Reroutes route data and
organize links; their input accepts one connection and their output can fan out.

| Keys / mouse | Action |
|---|---|
| `Shift` + right-drag across a link | Insert reroute points. Also available through `Shift-A` → search Reroute. |
| `Ctrl` + right-drag across links | Cut crossed connections. |
| `Ctrl-Alt` + right-drag | Mute / unmute crossed connections. |
| Drag from an output to another input | Create another branch of data flow. |
| `Ctrl` + left-drag a socket link | Detach and reconnect that link. |
| `Alt` + left-drag a node | Move it while detaching its links. |
| `G` / `R` / `S` | Move / rotate / scale the selected node layout. |
| Drop a node on a link | Insert it into the pipeline. Hold Alt while moving to toggle auto-attachment. |
| `F` / `T` while moving | Toggle frame attachment / auto-offset direction. |
| `Shift-D` / `Alt-D` / `Ctrl-Shift-D` | Duplicate / duplicate linked node groups / duplicate keeping input links. |
| `X` / `Delete` | Delete selected nodes or reroutes. |
| `Ctrl-X` / `Ctrl-Delete` | Delete while reconnecting the surrounding links. |
| `J` / `Shift-J` | Connect compatible selected sockets / replace existing links. |
| `M` / `H` / `Shift-H` / `Ctrl-H` | Mute nodes / collapse nodes / previews / unused sockets. |
| `Ctrl-G` / `Ctrl-Alt-G` | Group / ungroup nodes. |
| `Tab` / `Ctrl-Tab` | Enter / leave a node group. |
| `Ctrl-J` / `F` / `Ctrl-P` / `Alt-P` | Create frame / named frame / attach to frame / detach from frame. |
| `Shift-S` / `P` | Swap node type / separate nodes from a group. |
| `A` / `Alt-A` / `Ctrl-I` | Select all / none / invert. |
| `L` / `Shift-L` / `Shift-G` | Select linked inputs / outputs / similar nodes. |
| `B` / `C` / `Ctrl-Alt` + left-drag | Box / circle / lasso selection. Add Shift to lasso-deselect. |
| `Shift-Tab` | Toggle node grid snapping; Control temporarily inverts snapping during transforms. |
| `F2` / `F3` / `Ctrl-F` | Rename / operator search / find a node. |
| `Ctrl-C` / `Ctrl-V` | Copy / paste nodes. |
| `Home` / `Numpad .` | Frame all / selected nodes. |
| `T` / `N` / `Ctrl-Space` | Toolbar / sidebar / maximize editor. |
| Compositor `V` / `Alt-V` / `Alt-Home` | Backdrop zoom out / in / fit. |

Cut/reroute/mute gestures finish on mouse release. Linking and resizing finish on
left-button release; Escape/right click cancels those tools. These tools are read
from actual modal operator IDs when available; plain clicks are not guessed to be
socket drags. Menus and text-entry completion remain inferred. Properties and text
fields inside a node are not exposed as active UI controls by this bridge; Escape
out of text/value editing before using node shortcuts. Node Wrangler and custom
keymaps are not modeled by these default bindings.

The bridge still sends only context identifiers and flags, never node names,
socket values, graph contents or entered text. Polling and idle redraw behavior
are unchanged. Restart Blender after replacing its installed bridge to load 1.1;
rebuilding the app alone updates only the bundled `.py` copy.

References: [editing nodes](https://docs.blender.org/manual/en/5.2/interface/controls/nodes/editing.html),
[reroutes](https://docs.blender.org/manual/en/latest/interface/controls/nodes/types/layout/reroute.html).

## Timing and power

The add-on checks four times per second while observation is active in Blender.
Otherwise it checks for a connection every two seconds and skips Blender state
reads. State is sent when it changes, with a two-second heartbeat. Messages contain
only identifiers, flags and session/order metadata: no names, paths, search text,
scene contents or numeric input values.

The native receiver sleeps on a Core Foundation socket source. It adds no polling
thread or frame timer. Unchanged heartbeats do not request GPU redraws, and native
window geometry is cached between the existing health checks. Communication uses
IPv4 loopback UDP port **49538**, without an external server or filesystem polling.
One overlay instance owns the port; close duplicates and restart the overlay if
it cannot connect.

Requests are numbered after input, focus changes and observation loss. The add-on
waits one 250 ms interval after a changed request so Blender can handle the input
before reporting state. Old requests, duplicate/malformed packets and messages
from another process or session are rejected. Reports cannot overwrite newer input.
Active updates normally arrive within roughly 250–500 ms after input settles.
Initial reconnection is slower, and operations blocking Blender's main thread can
delay reports. A four-second heartbeat timeout invalidates observed state; the
next fresh report recovers supported contexts. The instance token separates stale
sessions; it is not a security boundary against other processes under the same user.

Measure **both Blender and KeyGuide**: the Python timer runs inside
Blender. No Activity Monitor Energy Impact target is claimed. See
[power measurements](performance.md) for the existing measurement tool.

## Files and verification

- `assets/blender/keyguide_bridge.py`: installable Python add-on.
- `src/platform/blender_observation.hpp`: portable state value.
- `src/platform/blender_protocol.hpp`: bounded/versioned decoder.
- `src/platform/mac/blender_bridge.hpp`: native socket lifecycle and geometry,
  used by the existing `input_observer.mm` compilation unit.
- `CommandObserveBlender` → `ObserveBlenderState`: portable command/tracker integration.

C++20, raylib, ECS, dependencies, compiler options and CMake files are unchanged.
The existing assets step copies the add-on into the app. The overlay has no Python
runtime dependency. Windows/Linux preview remains compatible. The protocol and
add-on are portable; other native receivers/coordinate adapters can be added later.

Existing CTest suites cover real-state-to-tracker/ECS behavior, ordering, tool
recovery, popup safety, unsupported contexts, observation loss and preview isolation.
Additional checks:

```sh
python3 tests/blender_addon_tests.py
clang++ -std=c++20 -fobjc-arc -Isrc -Itests tests/mac_blender_bridge_tests.mm \
  -framework Cocoa -framework ApplicationServices \
  -o /tmp/keyguide_blender_bridge_tests
/tmp/keyguide_blender_bridge_tests
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tests/blender_api_tests.py
```

Python unit checks use fake RNA/sockets. The native fixture uses real loopback UDP
and Core Foundation callbacks, without global input taps or OS input posting.
The Blender API fixture changes only a disposable factory scene to verify Object,
Mesh Edit, Curve Edit, Sculpt and selection reports over UDP; it does not save
preferences/projects. Curve tests cover hints, modal numeric controls, safe menus,
mouse addition/dragging, Mac aliases and remembered edit type. The command pipeline
checks that Curve Edit reports produce ECS hints and exact-modifier displays.
Node checks cover editor switching independent of mesh/curve modes, unique active
chords, mouse gesture release/cancellation, known node operators, numeric layout
controls, group navigation, legacy bridge compatibility and unsupported UI regions.
The real Blender fixture reads Geometry/Shader/Compositor editor types from RNA
using a real test area selected explicitly in its background process. Fake
area/region tests cover pointer hit testing separately. It does not verify a live
mouse drag. An authored renderer fixture checks modifier hover descriptions,
bounded expansion, idle redraw suppression and hide/show without OS input posting.

References: [Blender context API](https://docs.blender.org/api/5.2/bpy.context.html),
[running modal operators](https://docs.blender.org/api/5.2/bpy.types.Window.html),
[message bus](https://docs.blender.org/api/5.0/bpy.msgbus.html),
[add-on installation](https://docs.blender.org/manual/en/latest/editors/preferences/addons.html).

## Compact function-key layer

In every profile, number-row `1`–`9`, `0`, `-`, `=` carry `F1`–`F12` as a second display
layer; Blender supplies its F-key hints. Those keys always draw an `f` marker; it turns white when a shortcut is compatible with the held modifiers;
Fn reveals function hints and turns the available marker green. Fn changes display
selection only. The tracker continues to receive the original physical key, so an
observed number remains a number unless the keyboard/OS delivers an actual F key.
A number typed while Fn is held shows that Fn does not select F keys on this keyboard,
so Fn stops revealing the layer until restart. Actual F-key presses highlight both the corresponding number position and the
optional separate function row, if shown. Fn stays live while hover freezes the
application context. Unsupported/inactive contexts do not acquire new hints.

Alongside F2 rename and F3 search, the supported idle viewport/node contexts include:

| Shortcut | Hint |
| --- | --- |
| F1 | Context-sensitive manual |
| Ctrl-F2 | Batch rename |
| F4 | File context menu |
| F9 | Adjust last operation, when available |
| F11 / Ctrl-F11 | Render result / play rendered animation |
| F12 / Ctrl-F12 | Render frame / animation |
| Alt-F12 / Ctrl-Alt-F12 | Render frame / animation from the sequencer scene |
| Shift-F1–F12 | Switch editor type, following Blender's default keymap |

Editor switches and render actions can leave the supported context; the live Blender
bridge supplies the resulting editor/mode. In passive inference, an unknown resulting
context uses the existing RESYNC behavior. Shift-F5 always yields a 3D Viewport with
the previous mode; Shift-F3 can cycle to an unsupported node tree, so it needs a report.
The F4 file menu uses the existing menu tracking, and cancellation returns to the
previous context. F9 opens nothing without a redoable operation, and its panel closes
when the pointer leaves, so it keeps the editor context. On macOS, Ctrl-F2 and F11
are hidden and ignored: by default, the system uses them for menu-bar focus and Show
Desktop (also Ctrl-F1, F3–F8). Cmd-F2 still offers batch rename. These shortcuts come
from `km_window` and `km_screen` in the installed Blender 5.2 default keymap. No scene
or operator is executed by the overlay.
