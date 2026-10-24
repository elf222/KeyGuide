# KeyGuide implementation plan

This document records the agreed design for adapting the existing raylib game into KeyGuide. The observer, tracker, keyboard scene, macOS overlay/menu, preview backend and opt-in tests are now implemented. The project rename and pathfinding deletion were already complete. C++20, static raylib 6.0, the existing ECS Registry API, ICommand/CommandRegistry pipeline, frame-loop entry points, directories and desktop presets are preserved. The tables below describe the intended file responsibilities; current usage and implementation limits are in [README.md](README.md), and check results are in [docs/validation.md](docs/validation.md).

Implementation notes: the native window bridge uses raylib **6.0**'s actual `GetWindowHandle()` result, an NSWindow, directly. The initial native layout is US/ABC; unsupported sources report unavailable. The starting size is computed from the keyboard layout: currently 748×243 logical pixels. Hint font assets use IBM Plex Mono with its included SIL license. The default native deployment target is macOS 11, matching the current C++ toolchain minimum. Both Objective-C and Objective-C++ are enabled on native macOS so GLFW's `.m` sources remain Objective-C. The test option and three targets described below now exist.

UI follow-up: the menu item is VT and the default placement is top right. Position
presets, two nudge distances and next-display movement use constants in
`settings/defaults.hpp`. Hint selection requires the held chord exactly; available
modifier icons reflect only compatible combinations. Fixed icon slots and camera
bounds keep icons and enlarged edge keys inside their areas. Make's old `.o`/`.d`
files were removed from `src`; subsequent verification uses CMake only.

Compact UI: Space carries mode, a bounded pending sequence, and inactive/resync/
paused/unavailable status. It follows the same camera zoom as the other keys; mode/status labels are white, with inactive status gray. VT → Status
provides explanations formerly shown around the keyboard; Space has no hover textbox. Header and
footer are removed, gaps are 3 pixels, and modifier keys omit redundant slots.
Expanded hints wrap and can increase the hovered key's height as well as width;
the hovered key draws last and fits the window.

Layout follow-up: Escape is removed from the top row and shares the Caps Lock key's
display, retaining its Caps symbol. Physical Escape observations drive its pressed
highlight and hints; Caps Lock itself is not converted to Escape. The external
short-Caps remap remains responsible for sending Escape. Preview explicitly handles
physical Escape independently of the displayed keys. Up and right Shift exchange
order. Each row now spans 15 units: backtick is 1 unit, and bottom Control/both Option
keys shrink to 1 unit. Available modifier icons are white; held modifiers and Caps
Lock are green. No orange remains in the rendered keyboard.
`Default::ESCAPE_ON_CAPS_LOCK` restores the original Escape/Caps presentation when false.
That profile reduces Backspace to 1.25 units to preserve the same row width.
Outside supported Zed/Ghostty apps, only transient physical key/modifier feedback is maintained; Unicode
is not read, Vim tracking is suspended and all command hints are hidden.

Appearance follow-up: VT adds Hide/Show window and four exclusive transparency
modes: Off, Always, OnHover and WhenNotHovered. Runtime choices live in portable
`S` settings and reset to `Default` values on launch. The semitransparent defaults
are 0.80 key-layer opacity and 0.20 background opacity; hover uses whole-window
pointer containment. A transparent framebuffer is configured at startup. The
existing render texture holds premultiplied key coverage, composited over a
separately faded background. Native hide/show preserves the same window and
placement, keeps the menu/observer running, and preserves overlay focus/click-through
policy. Hidden windows report no pointer hover; switching preview/overlay keeps
the visibility choice. No build-system or input-observer changes are needed.

Ghostty follow-up (2026-10-07): the native foreground allowlist includes
`com.mitchellh.ghostty` and reports a portable `EditorProfile::Neovim` snapshot.
A profile change queues `CommandSetEditorProfile` before input/baseline commands;
it clears continuations and uses the existing generation token to reject stale
observations across apps. `VimBinding::profile` optionally restricts catalog
entries; shared hints and ECS entities remain unchanged. The Neovim profile
separates Normal Y, Insert Ctrl-b, bracket-indent motions, command-line hints,
Insert/Replace editing, and window/buffer navigation from Zed behavior. Ghostty
host navigation follows the Normal-on-entry convention; unsafe paste/UI commands
invalidate inference. Ghostty focus is assumed to contain Neovim: no process,
screen, RPC or editor-state reader is introduced. Pause for shell/other TUI use.
RESYNC now removes all hints and availability markers while keeping physical
pressed feedback and the Space status. Existing source/test files are extended;
no build configuration or dependency changes are required.

Ex/Visual follow-up (2026-10-07): a separate portable `CommandLine` context tracks
only whitelisted Neovim Ex verb prefixes. `vim_ex_commands.hpp` supplies command
names, accepted abbreviations, hints and expected results; generated binding entries
project through the existing Key_Hint candidates and CommandProjectKeyboard.
Space displays COMMAND and the bounded prefix. Hover shows the next physical key
and the complete Ex command; its measured width follows the existing expansion.
Arguments/history/completion are not retained; execution stays conservative when
hidden effects are possible. Known mode changes and argument-free neutral commands
complete locally, while quit commands require resync because Ghostty may enter a
shell. Zed's palette remains a separate context.
Visual I/A (including block insertion), endpoint swaps, selected-line edits and
search-match selections are supported. Zed's documented occurrence-selection
commands and Visual gI/gA use the same tracker/actions, with no cursor-count model.
Tests cover pure transitions and same-frame input/commands/ECS projection. No native
observer, Registry API, dependency or build configuration changes are needed.

The previous proposal to archive the registry and retire the command system is superseded. All six existing `src/registry/` files and `src/gameLogic/command_execution.cpp` stay active. No replacement ECS, UI toolkit, scripting runtime, dependency manager, or new rendering stack is planned.

## 1. Use passive observation and a local state machine

The installed Zed reports version 1.22.0 and bundle identifier `dev.zed.Zed`.

Zed internally tracks Normal, Insert, Replace, and three Visual modes, plus an operator stack and pending input. Its key contexts include `vim_mode` and `vim_operator`; these are internal keybinding contexts, not an external state API. Its mode indicator renders both the base mode and pending-command information. The current extension API does not expose a Vim state subscription.

Sources: [Vim implementation](https://github.com/zed-industries/zed/blob/main/crates/vim/src/vim.rs), [mode definitions](https://github.com/zed-industries/zed/blob/main/crates/vim/src/state.rs), [mode indicator](https://github.com/zed-industries/zed/blob/main/crates/vim/src/mode_indicator.rs), [extension API](https://github.com/zed-industries/zed/blob/main/crates/extension_api/src/extension_api.rs), [extension documentation](https://zed.dev/docs/extensions/developing-extensions).

The selected design observes keys and relevant mouse events through macOS and maintains a local tracker. It does not read Zed's Accessibility tree, inspect its UI, install an extension, alter settings, or send commands. Foreground-application detection through macOS remains useful for pausing tracking outside Zed; it does not reveal which control or file is focused inside Zed.

Use Normal as the assumed baseline when entering an editor after a recognized file/pane navigation action. This is the user's workflow convention: leave files in Normal mode. It is not a claim that switching files forces Zed into Normal. Known shortcuts and navigation sequences provide evidence of a likely switch, not confirmation that it succeeded. Arbitrary mouse clicks cannot reliably distinguish tab switches, cursor movement, selection, or panel focus; mark ambiguous mouse changes uncertain and provide a simple reset.

All mode state is inferred or manually assumed. Show uncertainty after unsupported commands, ambiguous context changes, or observation gaps. Do not add OCR, process-memory inspection, or a Zed state reader to this implementation.

Repeated Escape is the practical resync convention. With default bindings and the document editor focused, Escape leaves Insert/Replace/Visual or cancels a pending operator. An initial Escape may instead dismiss a menu, search field, or literal-character wait. Two or three presses are useful in ordinary editing, but no fixed count guarantees Normal from every Zed UI context, terminal, custom binding, or macro outcome.

Model supported Escape transitions normally. Two consecutive distinct Escape presses can establish an assumed Normal baseline for the overlay and clear counts/prefixes/operators, unless the tracker knows input is still in a non-editor context. Do not count key-repeat events as separate presses. The user should focus the document editor before using this recovery gesture. This resets only the overlay; it cannot prove Zed's state or repair unsupported hidden effects.

Sources for Escape/context behavior: [default Vim bindings](https://github.com/zed-industries/zed/blob/main/assets/keymaps/vim.json) and [default macOS bindings](https://github.com/zed-industries/zed/blob/main/assets/keymaps/default-macos.json).

## 2. Establish passive input and a window that preserves focus

Use `CGEventTapCreate` with `kCGEventTapOptionListenOnly`, initially at the session event level, to observe keyboard presses/releases, modifier changes, and mouse clicks used for invalidation. Never fall back to an active/filtering tap.

- The callback reads a minimal event record, attempts a bounded nonblocking queue write, and returns the original event.
- No event mutation, posting, injection, suppression, hotkey registration, remapping, replay into Zed, sleeping, blocking I/O, rendering, or Accessibility queries in the callback.
- Queue overflow drops only the overlay's observation and marks tracking uncertain. It never holds up input to recover an event.
- Handle tap disablement, denied/revoked Input Monitoring, and gaps in observation. Re-enable a passive tap when appropriate and invalidate predictions after a gap.
- Keep raw input transient in memory. Insert/Replace text is discarded after normalization unless a pending command requires a character. Do not retain a typing log.
- Gate Vim tracking on Zed being frontmost and suspend for non-editor contexts recognized from supported input sequences. Outside Zed, observe physical key downs/ups and flags for transient pressed feedback, without reading Unicode, tracking commands or retaining a typing log. Keys alone cannot reliably establish internal editor focus; expose that uncertainty.
- Include key code, translated character, modifiers, repeat flag, timestamp, and focus generation. Match the installed Zed's binding behavior; separate physical keyboard geometry from logical Vim characters.
- Begin with a QWERTY keyboard presentation, but isolate layout mapping. Treat unsupported layouts, IME/dead-key composition, and unusual remaps as uncertain instead of assuming every key code is an ASCII letter.

[Apple's listen-only option](https://developer.apple.com/documentation/coregraphics/cgeventtapoptions/listenonly) is the API-level passive observation mechanism. Check Input Monitoring through `CGPreflightListenEventAccess`; the app can explain and request that permission when the user enables observation. This design does not include an Accessibility state reader.

Prototype the native window early:

- Small floating window, approximately 720 by 280 logical pixels as a starting point, with resizing and HiDPI support.
- It remains above ordinary Zed windows, including the intended Spaces/fullscreen use cases, without becoming the keyboard target on launch, hover, or click.
- Start from raylib's desktop window flags, then add macOS native behavior where needed. An initially unfocused flag is not proof that the window cannot later take focus.
- Verify the bundled raylib backend and its actual native handle contract before obtaining a Cocoa window. A raylib/GLFW handle must not be blindly cast to `NSWindow *`.
- Prototype a nonactivating native host for the raylib surface if existing-window configuration cannot preserve focus reliably. Keep native window lifecycle work on the appropriate thread and preserve the existing rendering loop.
- Make the keyboard surface click-through by default. Sample global pointer position and convert it to window coordinates for hover; do not depend on ordinary raylib mouse events reaching a click-through window.
- Use a small macOS menu-bar menu for pause, quit, and manual state selection, so clicks on the sheet continue to reach Zed. A portable preview can expose equivalent local controls.
- System dialogs and protected OS surfaces are outside the ordinary always-on-top requirement.

Native focus preservation and passive input are first-milestone acceptance requirements.

### Input implementation: native capture and ownership

The macOS input backend owns a dedicated observation thread and CFRunLoop, the listen-only tap, and a preallocated single-producer/single-consumer ring buffer. The main application thread owns CommandRegistry and every ECS mutation. Native workspace notifications publish foreground-app information without reading Zed UI state. The tap callback never constructs ICommand objects or touches registry data.

Startup sequence:

1. Create the ordinary raylib resources/scene and configure the native overlay window.
2. Check Input Monitoring with `CGPreflightListenEventAccess`. If missing, display observation-unavailable status and offer the normal macOS permission request. Permission denial leaves the preview working; no active tap fallback is attempted.
3. Initialize the foreground-app gate from macOS and subscribe to activation/deactivation/session notifications. Publish an atomic gate containing whether Zed is eligible and a generation number. Reconcile with the frontmost application on the main thread so a missed notification does not leave tracking enabled indefinitely.
4. On the observation thread, create a `kCGSessionEventTap` at `kCGTailAppendEventTap` with `kCGEventTapOptionListenOnly`. Add its source to that thread's run loop and enable it. Creation failure is ordinary observation status.
5. Record keyDown, keyUp, flagsChanged, and the small mouse-button/drag subset needed for invalidation. Pointer motion for hover is sampled separately; it does not flood the input queue.

Callback work is limited to event type inspection, one coherent gate snapshot, fixed-size reads, and a nonblocking queue write. A raw record contains:

| Field | Why it exists |
| --- | --- |
| Event kind | Key down/up, modifier change, or mouse invalidation; disabled-tap notifications are handled separately. |
| Native key code | Later converted into a portable physical key ID for the correct keycap. |
| Raw flags | Shift/Control/Option/Command plus layout-related flags such as Caps Lock. |
| Bounded UTF-16 data and length, if supplied | A candidate logical character; never an assumed complete text insertion. Truncation/unsupported composition becomes uncertain. |
| Repeat flag | Distinguish system key repeat from a fresh physical press. |
| Monotonic event timestamp and sequence | Preserve order and support reset gestures and diagnostics without retaining typed-text history. |
| Foreground generation | Prevent queued input from an earlier app activation from updating current state. |
| Observation-loss generation | Detect discontinuities even when a full queue could not accept a loss marker. |

Return the original event without mutation. No event-posting API, callback sleep, mutex wait, heap-backed strings, log output, Objective-C application query, or command allocation occurs here. A passive tap is the API choice that keeps observation separate from event modification; the app adds no intentional wait for editor input delivery. The rendered response naturally appears on a later frame, without holding the actual keystroke.

### Observation queue, foreground boundaries, and failures

Start with a named capacity of 1024 fixed-size records. The callback is the only ring producer; the application main thread is the only consumer. Use lock-free atomic producer/consumer indices with release/acquire publication, verified for the supported macOS target. Main-thread workspace/menu notifications use a separate main-thread path or atomic control state, not a second concurrent ring producer. This preserves the SPSC assumption.

- If the ring is full, skip that observation, increment the loss generation, and return immediately. Never wait for room.
- `UpdateInputInfo` drains into a bounded transient batch. Loss, pause, unavailability and preview focus changes clear physical feedback. Stale foreground records may update native physical feedback, but cannot advance Vim tracking. Stale loss-generation records are discarded for both paths; continuation state is invalidated before processing a new baseline.
- Check the loss generation around draining; a partially observed sequence must not be presented as reliably understood. Loss notification does not depend on finding room in the full ring.
- On leaving Zed, disable Vim eligibility, increment its generation, and queue a context command on the main thread. Physical pressed state survives app switches and clears on observed releases. The callback skips Unicode reads and mouse observations outside Zed. On returning, initialize the selected assumed-Normal baseline; only events from the current foreground generation can advance it.
- Activation notifications and keyboard events are not a perfect editor-focus oracle. At ambiguous activation boundaries discard observations rather than claim a guaranteed target. Only the foreground application is known; terminal/search/panel focus inside Zed remains inferred from supported inputs.
- On tap disablement, publish a loss status, restore the passive tap when appropriate, and keep the inferred state uncertain until resync. If permission is revoked or observation cannot resume, report unavailable.
- Suspend for lock/sleep or reported secure-input conditions where detectable; never bypass protected input. Missing events cannot always be distinguished from no typing, so an inactivity timer cannot certify synchronization.
- On shutdown, disable the gate/tap, stop and join the observation thread, remove notifications/run-loop resources, then clear command/ECS/window resources.

### Normalize keys on the main thread

Convert raw records into small portable `InputEvent` values declared in `src/platform/input_event.hpp`. This conversion runs inside the selected platform backend while its drain function is called on the main thread. Running on the main thread does not mean putting native translation in `gameLogic/input.cpp`. The ECS/gameLogic layer receives already-normalized values and sees no `CGEventRef`, Cocoa objects, or macOS key-code constants.

`src/gameLogic/input.hpp` and `input.cpp` remain ordinary portable C++. They contain no Apple/Windows headers, Objective-C syntax, native key-code tables, foreground-application API calls, or platform-selection preprocessor branches. Portable raylib types/helpers are acceptable here; the source of observations is supplied through a platform-independent header. This lets the same input orchestration compile with the macOS backend, a future Windows/Linux backend, or a test backend.

Proposed public interface in `src/platform/platform.hpp`:

```cpp
namespace Platform
{
    InputBatchInfo ReadInputEvents(std::span<InputEvent> destination);
    PointerPosition ReadPointerPosition();
}
```

The names and payloads are illustrative until implementation. InputBatchInfo reports event count, generic observation/foreground status, generations, and losses; PointerPosition is simple portable coordinate data. No native handles or Objective-C objects appear in the public signatures. The caller supplies a bounded buffer, so input reads do not need an unbounded event container.

The macOS implementation drains its raw ring and performs native-to-portable conversion internally. The fallback implementation supplies local/manual preview input. Tests link a small fake implementation that supplies authored events. Build-time source selection chooses the implementation; `input.cpp` does not choose the OS at runtime or include the implementation file.

Preserve two identities:

- **Physical key:** identifies which drawn keycap is pressed.
- **Logical key/chord:** identifies the supported Vim binding or a character argument. It includes modifier information and translated printable data where available.

Examples: the D key produces `d` or `D`; number-row 1 can produce `1` or `!`; Ctrl-D is a chord, not a control character that should be parsed as ordinary `d`. Escape/arrows/Enter/Backspace are special-key values. Caps Lock remains separate from a physically held Shift key. Keep left/right modifier key IDs for drawing while folding them into shared chord flags for binding lookup.

Use a cached input-source/layout snapshot outside the callback. Normalize according to the supported Zed binding profile, and use event Unicode/layout translation only to the extent validated for that profile. A raw event is not a notification that Zed inserted a particular Unicode string. Dead keys, IME composition, unsupported layouts, and uncertain remapping behavior trigger uncertainty rather than pretending the overlay sees Zed's final committed text. No global keyboard layout is changed.

`flagsChanged` updates held modifier state, even when no printable key is pressed. KeyUp updates pressed-key visuals and helps distinguish physical Escape presses; it does not normally advance the Vim grammar. KeyDown advances the grammar exactly once per observed down event, including repeat events where the binding treats them as input. Never synthesize additional repeats in the overlay.

Repeated Escape recovery requires two unmodified, non-repeat Escape presses with an intervening release, no intervening other nonmodifier key press, and a configurable interval initially one second. Each press first follows supported context cancellation rules. Then the explicit recovery convention can establish assumed Normal unless a known non-editor context remains. A held Escape cannot trigger this two-press gesture. Pressing a third time is harmless for the overlay's already-Normal baseline; actual Zed behavior still depends on focus and bindings.

### UpdateInputInfo and commands

The native observer never polls raylib's local key state. `IsKeyDown` is useful for an active local preview, but cannot supply global Zed input to the actual overlay. Keep the existing `UpdateInputInfo` entry point and replace its old player-control body with:

```text
Call Platform::ReadInputEvents with a bounded portable buffer
Validate generic status/generations in the normalized batch
Queue context/loss commands at their known boundaries
Queue CommandTrackKey for each normalized key record
Call Platform::ReadPointerPosition for the next view update
Return to MainLoop; execute queued commands before drawing
```

All native events and local menu baseline changes enter this same main-thread command path. Do not call `cmd_registry.clear()` to recover a native ring overflow: other queued scene/menu commands must survive. Invalidation is an explicit command. Keep ordered state transitions and view projection in separate command batches as described below.

`CommandTrackKey::execute()` obtains `Tracked_State` from the tracker entity, applies the pure transition against that current value, stores the result, and updates its revision. When a key changes only pressed visuals, update that bookkeeping without inventing a Vim action. The input command never executes an editor command in Zed.

Tracker interpretation uses this priority:

1. Supported global/context shortcuts and mode cancellation, with literal-input exceptions respected.
2. A pending literal character/register/mark/text-object argument, which consumes the key within the local grammar.
3. A pending multi-key prefix or operator, preserving its count and return mode.
4. A base-mode binding from the shared catalog.
5. Plain Insert/Replace text: no ordinary Vim command transition and no text retention after the event is processed.
6. An unsupported or context-ambiguous action: mark affected state uncertain.

The word 'consume' here means interpret one copied observation once; it never means consume the macOS event. Unsupported input is distinguished from a known no-op/plain text. Waiting states are reasoned states, not a generic bucket: waiting for an `f` argument, a register, and the second half of `g` need different rules and hints.

| Observed sequence | Local interpretation |
| --- | --- |
| `i`, `d`, Escape | Enter Insert, treat `d` as text, return to Normal. |
| `d`, `w` | Start delete operator, complete supported word motion, return to Normal. |
| `c`, `i`, `w` | Start change, request inner object, complete word object, predict Insert. |
| `d`, `f`, `a` | Keep delete pending, wait for find argument, interpret `a` as the argument; predict Normal. |
| `r`, `i` | Wait for replacement character; `i` is a replacement argument, not Insert entry. |
| `2`, `d`, `3`, `w` | Preserve pre/post counts; clear them when the supported operation completes. |
| `:`, typed palette text, Escape | Enter inferred palette context; do not parse its text as Normal commands; apply supported cancellation. |
| Recognized file-switch chord | Clear pending state and apply the chosen assumed-Normal baseline; it is still an inferred navigation outcome. |
| Unsupported macro playback or ambiguous mouse action | Invalidate affected tracking and show uncertainty until resync. |

At most one final view projection is needed for a stable completed input batch; the model still processes every event in order. Immediate input handling updates the header/available hints by the next render frame. Dirty/revision flags avoid rebuilding all hint candidates on every key release or idle frame.

### Input validation

Use authored input fixtures, never a recorder of personal typing. In addition to ordinary mode/operator tests, check keyDown/keyUp ordering, repeated-key events, held vs double Escape, modifier changes while idle, shifted punctuation, stale foreground generations, loss halfway through `dfa`, and multiple commands queued in one frame. Exercise the ring's full/empty/wrap-around behavior and overflow signaling independently of the native window.

Native checks verify key order/repeat/characters and keyboard focus with the observer enabled and disabled. Pause a test consumer to force observation overflow and verify that only KeyGuide tracking degrades. Check denied/revoked permission, app switching, shutdown/restart, and unsupported composition. Never inject events into Zed as a reset or a validation shortcut.

API references: [listen-only tap](https://developer.apple.com/documentation/coregraphics/cgeventtapoptions/listenonly), [permission preflight](https://developer.apple.com/documentation/coregraphics/cgpreflightlisteneventaccess()), [key-repeat field](https://developer.apple.com/documentation/coregraphics/cgeventfield/keyboardeventautorepeat), and [workspace activation notification](https://developer.apple.com/documentation/appkit/nsworkspace/didactivateapplicationnotification).

## 3. Replace gameplay with a small, testable Vim tracker

Keep `InitialiseGame`, `InitialiseWorld`, `MainLoop`, `TickOnce`, `UpdateInputInfo`, `DrawFrame`, and `CloseGame` as familiar entry points during adaptation. Their responsibilities become startup, keyboard-entity creation, observation draining, command execution, view updates, drawing, and shutdown.

### Preserve the registry and make the keyboard an ECS scene

Keep `Registry`, `ComponentArray<T>`, entity IDs, tag vectors, masks, component lookup, and swap-removal behavior. Keep `registry` and `cmd_registry` in `registry/global.hpp`. `registry.hpp` should remain unchanged. In `registry.cpp`, replace only the component registrations in `Registry::Registry()`; the generic storage/entity/tag functions remain the same.

Each physical key is one persistent entity, even when it has no active hint. One additional entity owns the tracked Vim state. This avoids storing competing copies of the mode in globals, key components, and renderer state.

| Component | Proposed contents and responsibility |
| --- | --- |
| `Entity_Position` | Initialized `Vector2 pos`; current top-left location in keyboard coordinates. Replaces the game's `World_Position` role. |
| `Entity_Size` | Initialized `Vector2 size`; current key width/height in the same coordinates, including animated expansion for a wider hovered hint. |
| `Key_Layout` | Canonical position/size and row/order information so an expanded hovered key can reflow its neighbors and restore their original layout. |
| `Colour` | Preserve the existing `Color col` component and constructors for base keycap color. |
| `Key_Values` | Stable portable physical-key identity and `std::array<KeyValue, 2>` for unshifted/shifted logical values and legends, such as `d`/`D`, `1`/`!`, or `Escape`. OS-specific key codes stay in the platform mapper. |
| `Key_Hint` | `std::array<std::vector<HintId>, 16>` indexed by exact Shift/Control/Option/Command combination; references candidate hints in a shared immutable catalog. Each candidate also has mode/pending-context conditions. |
| `Key_Display` | Derived current hint ID, available modifier combinations, pressed/hovered flags, and display status. Updated by commands after input; read by drawing. |
| `Tracked_State` | One `VimState` value containing the current inferred mode, pending context, counts, uncertainty, modifiers, and resync information. |

Use `TagName::Key` and `TagName::Tracker` initially; do not add an entity for every hint or modifier glyph. Keep the existing `ComponentType`, `ComponentCount`, `TagCount`, and `ComponentID<T>` registration convention. Every new component is initialized and appears in the enum, specialization, and constructor registration list, as in the original project.

The above components are now implemented; `KeyValue` must represent special keys as well as printable values, so it is not just a `char`. `HintId` is a stable catalog index, not an ECS component pointer; component storage can move when entities are added or removed.

Use a four-bit modifier mask: Shift=1, Control=2, Option=4, Command=8. Index 0 means no modifiers, 1 means Shift, 2 Control, and 3 Shift+Control. The other combinations follow the same pattern. A flat 16-slot array fits all modifier combinations and is easier to inspect than a binary tree. It does not replace command-sequence parsing: `d`, `D`, `Ctrl-d`, the `d` continuation in `gd`, and a literal `d` after `f` have different context rules. Each slot can contain several candidates whose conditions select the current action. Physical key values, actual translated characters, Caps Lock, and required modifiers remain separate concepts.

Store hint text, full chord/sequence, short/expanded labels, optional mnemonic index, context condition, and transition metadata once in `gameLogic/vim_bindings.*`. The key's `Key_Hint` owns the modifier-indexed references. The tracker and hint projection use the same binding definitions; neither duplicates a second command table. Begin with simple tables and explicit prefix/argument state; add a sequence trie only if the actual supported grammar warrants it.

For a D key entity, an unmodified Normal candidate can display `del` with emphasized `d`; its Shift candidate describes deletion to the end of the line. After an observed `d`, unmodified `d` instead describes the supported repeated-operator continuation. During a character wait it is an argument. In Insert neither ordinary `d` nor `D` gets a Vim action hint.

### Preserve deferred commands and their execution files

Keep `ICommand::execute()`, `CommandRegistry::add<T>()`, `executeAll()`, `clear()`, and the existing `unique_ptr<ICommand>` queue. Preserve `CommandDestroyEntity` and `DestroyEntity`. Replace the player/enemy/particle command payloads and factory functions with overlay payloads and factories:

- `CommandAddTracker`: create the one tracker entity and initialize its `Tracked_State` through the registry.
- `CommandAddKey`: create a key entity and attach geometry, values, hint candidates, color, and display data.
- `CommandTrackKey`: consume one normalized observed key event and advance the tracker against the state at execution time.
- `CommandSetTrackedMode`: apply a manual baseline selection to the overlay only.
- `CommandSetTrackingContext`: update foreground/paused/inferred navigation context.
- `CommandInvalidateTracking`: handle observation loss or an ambiguous change.
- `CommandUpdateKeyboardView`: project the resulting tracked state and pointer snapshot into the key display components. Resize/layout changes can use the same command family.

`registry/commands.hpp` declares those types; `registry/commands.cpp` keeps the queue implementation and free enqueue functions; `gameLogic/command_execution.cpp` keeps the execute definitions and ECS mutations. Add direct `<memory>` and `<utility>` includes to `commands.hpp` rather than depending on another file to include them first. This is a header hygiene change, not a queue redesign.

`CommandTrackKey::execute()` calls a portable pure transition function, then stores its result in `Tracked_State`. Do not pre-resolve every input event to a mode change when draining the observer: two events in one frame must resolve in sequence. `d` followed by `w` must execute as delete-pending then completed motion; `d f a` must interpret `a` as the argument to `f`, not as Insert entry.

Commands do not enqueue more commands into the same queue while `executeAll()` iterates its vector. Execution applies the result directly. Keep all queue allocation and registry access on the main thread; the passive event callback only writes minimal records to a bounded observation queue. Do not retain component pointers across entity creation/removal.

The frame order becomes:

```text
UpdateDeltaTime()
UpdateInputInfo()             -> queue ordered input/context commands
cmd_registry.executeAll()    -> update Tracked_State in event order
UpdateToNewWindowSize()       -> refresh resources/settings if necessary
CommandProjectKeyboard       -> refresh hints while preserving current hover geometry
cmd_registry.executeAll()
TickOnce()                   -> update hover; queue view projection
cmd_registry.executeAll()    -> update key geometry/display components
CameraLogic()                -> zoom and fit the completed hovered geometry
DrawFrame()                  -> read the completed ECS scene
```

Initialization queues the tracker and keyboard entities, executes those commands, then projects their initial display. Shutdown stops and joins observation before destroying platform/window resources or clearing the ECS. The render step never advances the Vim tracker. This keeps the existing command pattern while avoiding the old draw-before-execute latency for mode changes.

Represent state in layers:

| Field | Meaning |
| --- | --- |
| Base mode | Normal, Insert, Replace, Visual Character, Visual Line, Visual Block |
| Pending context | None, operator, prefix, character, text object, register, mark, macro register, search/palette |
| Continuation | Operator, sequence prefix, counts before/after operator, return mode, temporary Normal state |
| Tracking status | Active, suspended, uncertain; inferred or manual baseline |
| Target identity | Foreground application and observation generation; internal editor/file identity is unknown |

Operator-pending and waiting are not replacements for the base mode: `df` is still based in Normal, retains delete, and waits for a character. The display can show `Normal · delete · waiting for character`.

Implement a deliberately bounded grammar based on Zed's default Vim bindings:

- Normal motions and single commands: `hjkl`, word motions, line motions, undo/redo, paste, insert/append/open, delete character, visual entry, and Replace entry.
- Operators: `d`, `y`, `c`; repeated operators such as `dd`; counted motions such as `2d3w`; `0` distinguished from a count digit.
- Multi-key prefixes such as `g`, supported bracket motions, and `Ctrl-w` continuations.
- Character waits: `f/F/t/T`, `r`, marks, and register selection. In `fa` or `ra`, `a` is an argument, not entry to Insert.
- Text objects: `iw`, `aw`, and an explicit initial list of quotes/brackets. `ciw` finishes in Insert; `diw` finishes in Normal.
- Visual operators, Visual Line/Block variants, and temporary Normal from Insert via supported `Ctrl-o` behavior.
- Search and `:` command palette get separate input contexts. Typing inside them is not parsed as Normal-mode commands. Unsupported execution results trigger resync.
- Escape cancels pending state or leaves Insert/Visual/Replace when the inferred context supports that interpretation. Escape in a dialog may only close the dialog. Repeated Escape provides an explicit assumed-baseline recovery gesture after focusing the document editor.
- Support macro recording state sufficiently to distinguish its register argument and stopping `q`. Macro playback and buffer-dependent repeats can change mode without globally observed physical keys; invalidate rather than simulate unseen effects.

Commands whose outcome depends on buffer contents or custom bindings remain predictions. A configured profile declares supported bindings and overrides; arbitrary Zed keymaps are not automatically understood. Unsupported input is not automatically a no-op.

Keep `VimState`, normalized input types, binding definitions, and the pure transition function independent of raylib and macOS. `Tracked_State` wraps this value for ECS use. Test the pure tracker and the command-to-ECS integration without opening windows or capturing live input. Pure tracker code can return a new state and a hint-update revision; the ECS command remains responsible for applying it.

## 4. Draw the keyboard and hover details

Store physical key rectangles in `Entity_Position`/`Entity_Size`, values in `Key_Values`, candidate catalog references in `Key_Hint`, and the resolved view in `Key_Display`. A command definition contains its sequence, applicable contexts, required modifiers, short hint, expanded hint, and optional mnemonic character index. Use one catalog for lookup, hints, and supported transitions so those do not disagree. Iterate existing registry tags/component arrays to draw and hit-test the keys. Keys are created once, not destroyed/recreated when modes change.

- All keyboard key outlines remain in place in every mode, including keys without a relevant command.
- Top of each key reserves consistent slots for Shift `⇧`, Control `⌃`, Option `⌥`, and Command `⌘`. Light a slot when a matching modified command is available in the current context.
- Availability and modifiers currently held need distinct appearances. A lit Shift slot does not mean Shift is pressed.
- Modified combinations remain distinct actions. Hover lists the exact chord for each; separately lit Control and Shift slots must not imply that every combination works.
- Bottom of the key contains the brief hint for the exact held modifier combination. Without modifiers, commands requiring modifiers have no hint; compatible modifier icons still advertise them.
- Emphasize a mnemonic only when that command's actual character is present in the hint. Draw text runs to color/weight that character; never invent a matching letter. `yank` emphasizes `y`; `del` emphasizes `d`; an unrelated hint has no mnemonic emphasis.
- If the hint does not show the command character, omit its compact legend and reveal the literal key/chord on hover. Empty keys retain their outlines.
- Your `u`/`U` example is a display rule: only if the command catalog contains an available Shift-U action does the U key show a lit Shift arrow. Ordinary Insert-mode `u` and `U` are typed text, so neither receives a Vim command hint by default.
- When waiting for a literal character, show that the next character is an argument. Do not label those characters with unrelated Normal commands.
- Use Space for mode and a small pending sequence. Inactive/resync/paused/unavailable status replaces the mode when applicable. Open VT → Status for the explanation. Suspend mode-specific highlights when the state is unknown.

On pointer entry, smoothly magnify the keyboard around the pointer anchor inside the same window using raylib's Camera2D. Allow the hovered key to widen when measured expanded-hint text needs room. Keep canonical geometry in Key_Layout and animate Entity_Position/Entity_Size as the row reflows to preserve gaps and avoid overlapping neighbors. Clamp the expanded layout/view to the available window; wrap or cap long text rather than grow keys indefinitely.

Hit-test only canonical `Key_Layout` rectangles through the unzoomed base camera.
Hitboxes stay at their original screen positions and sizes while drawing zooms,
widens and reflows keys. Do not retain hover based on enlarged drawing bounds:
moving into a neighbor's original area selects that neighbor immediately, and
moving into an original gap clears hover. Preserve the pointer's relative position
inside the key when adjusting the drawing camera anchor. Leaving the window
restores canonical layout and compact zoom. Use a floating camera transform and
existing helpers without integer camera rounding.

Reveal larger text for the hovered key: `yank` can remain `yank`; `del` becomes `delete`; show the full command character or chord even when omitted in the compact view. The hint panel is clipped/clamped to the window and does not intercept clicks. Leaving the window restores the compact view. Allow reduced-motion settings and make text/glyphs crisp on Retina displays. Bundle a suitably licensed font with the required glyphs if necessary; simple modifier icons can be drawn directly with raylib.

## 5. Use workflow assumptions and explicit resynchronization

| Trigger | Response |
| --- | --- |
| Startup or returning to Zed | Start from an assumed Normal baseline under the user's workflow convention; display it as inferred. |
| Leaving Zed | Suspend tracking and discard incomplete continuations. |
| Recognized file/pane navigation | Clear pending state and assume Normal on inferred return to the document editor. |
| Ambiguous click or drag | Invalidate pending state and mark uncertain; a mouse event does not identify a file switch or resulting mode. |
| Search, command palette, terminal, dialog recognized from input | Suspend ordinary editor command parsing; use supported context-exit rules or explicit resync to resume. |
| Unsupported sequence, remap, macro playback, ambiguous repeat | Mark affected fields uncertain until a supported reset or manual selection. |
| Event gap, tap disablement, queue overflow | Clear partial sequences and mark uncertain until resynced. |
| User presses Escape in an inferred editor context | Apply the supported cancellation/exit rule. |
| Two distinct consecutive Escape presses | Establish assumed Normal and clear counts/prefixes/operators, unless a known non-editor context remains. The user focuses the document editor first. |
| User chooses a mode from the overlay menu | Set the overlay's local baseline only and clear pending state. Never send that mode change to Zed. |

Normal on file navigation is an explicit user-selected assumption, not observed state. Do not apply it to every mouse event. Manual selection and repeated Escape alter only the overlay baseline; they never send a mode change to Zed. A timer can mark a continuation stale but cannot establish Zed's mode. No passive tracker, even without implementation bugs, can infer every hidden editor/context change from keys alone.

## 6. Keep platform boundaries and existing build workflows

The renderer, catalog, tracker, normalized events, and replay fixtures are ordinary portable C++. Put Cocoa, event taps, and foreground-app tracking behind a narrow platform interface. Implement those in macOS `.mm` files; provide a non-macOS backend that reports global observation unavailable and supports a manual/replay preview.

Retain Windows/Linux desktop CMake presets, the `run` target, static raylib dependency, assets copying, and compilation database. Preserve Web/Android scaffolding and isolate unsupported global-observation functionality. Full desktop overlay behavior is initially validated on macOS; other platforms retain a buildable preview where their existing toolchains support it.

Build changes will be needed, and must be documented under the repository's build-awareness convention:

- Enable Objective-C++ only on macOS, with the same language standard as C++.
- Select exactly one platform backend and include `.mm` files explicitly; the current recursive source glob does not include them.
- Link the macOS frameworks required by the implementation, including AppKit and ApplicationServices/CoreGraphics; add keyboard-layout framework support only if used.
- Add a macOS app bundle with a stable new bundle identifier, consistent executable/plist values, and appropriate agent/nonactivating behavior. Keep a consistent app identity for permissions.
- Use KeyGuide consistently for project, target, executable, window/app display name, presets, editor paths, and packaging. The coordinated rename has been applied; the desktop output is `build/<configuration>/KeyGuide/KeyGuide` (plus `.exe` on Windows), while the Web entry remains `index.html` under its KeyGuide output directory.
- Add tracker tests without requiring graphics or platform permissions.

### What .mm means

`.cpp` is an ordinary C++ implementation file. `.m` is Objective-C. `.mm` is Objective-C++, which permits C++ and Objective-C syntax in one implementation file. It is useful for calling Cocoa objects such as NSWorkspace and NSWindow while still using C++ data structures and the existing project interfaces.

Only macOS bridge implementations need this language. Their corresponding public headers stay valid C++. CoreGraphics event APIs have C interfaces and can be called from a macOS-specific `.cpp` when no Cocoa syntax is needed; a file does not need `.mm` merely because it is platform-dependent. The planned input_observer bridge uses Cocoa foreground notifications as well as C event-tap APIs, so `.mm` is appropriate there.

Apple's existing Clang compiler builds these files; this does not require Swift, a new UI framework, or replacing C++ code. CMake enables its OBJCXX language only for the native macOS build and applies C++20 to the C++ portion. Headers do not leak Objective-C ownership/ARC details. If ARC is used, apply its compiler option only to the macOS bridge sources rather than all targets or raylib sources.

Reference: [source-file language suffixes](https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html) and [CMake enable_language](https://cmake.org/cmake/help/v3.25/command/enable_language.html).

### Build changes already made versus proposed

Completed changes are only the coordinated KeyGuide rename and deletion of the empty pathfinding pair: CMake project/resource references, preset target names, editor launch names/paths, Make/Android names/source lists, Visual Studio solution/project/resource references, and native/Web metadata. Existing C++20, raylib version/static linking, Ninja presets, and dependencies remain the same. Debug/Release CMake caches were regenerated because they originally belonged to the source game's directory.

During overlay implementation, make these additional changes:

1. **Select implementation sources.** In `src/CMakeLists.txt`, keep portable sources in the common list and remove backend implementations from recursive discovery. Explicitly add the macOS `.mm` files for a native macOS target or fallback `.cpp` for the other targets. Exactly one implementation supplies each Platform function; compiling both would create duplicate definitions. Host OS alone must not select Cocoa for a Web/Android cross-build.
2. **Enable native compilation.** In the root CMake file, call `enable_language(OBJCXX)` only for native macOS and set `OBJCXX_STANDARD` to 20 with the standard required. Existing `.cpp` files continue using `cxx_std_20`; no need to rename input.cpp or any portable module to `.mm`.
3. **Link required OS libraries.** Add only the frameworks used by the bridge: AppKit/Cocoa for native application/window APIs and ApplicationServices/CoreGraphics for event observation. The root already links Cocoa, IOKit, and OpenGL; reuse existing linkage where sufficient. Carbon/input-source support is added only if the chosen layout or secure-input implementation actually uses it. These are macOS SDK libraries, not new third-party downloads. Keep them off fallback and pure-test targets.
4. **Package the native app.** Add a macOS bundle target or a packaging target with the existing renamed Info.plist. Preserve the current desktop command-line target/run paths where practical; a separate packaging target can build `KeyGuide.app` without changing every editor command. Define agent/nonactivating behavior only after the window prototype establishes the required settings. No signing/notarization/deployment flow is added to ordinary local builds.
5. **Keep alternate build routes consistent.** Make gets `.mm` source/object rules and native frameworks only for macOS; Android and the VS2022 project get portable sources/fallback only. Keep C++20 and current target/platform conventions. Editor tasks continue invoking the same build routes; update only actual changed paths/source metadata. Backend changes must not require platform conditionals inside input.cpp.
6. **Add opt-in tests.** Introduce `KEYGUIDE_BUILD_TESTS`, initially OFF to preserve ordinary builds. On a supported desktop build, enabling it registers small test executables through CMake/CTest. Normal run targets continue launching KeyGuide, not tests. Web/Android test execution is not promised without a separate runner.

These proposed changes are documented here for build awareness. This planning turn changes only this document; native compiler/backend/test setup has not yet been applied.

### What the tests are and how they run

A test is a small C++ program that calls the real code with known input and checks independently specified expected results. It prints a useful mismatch and returns a nonzero exit code on failure; success returns zero. Use always-active checks, such as the existing project ASSERT where appropriate, rather than ordinary `assert` checks that disappear under NDEBUG. No new test framework is needed.

CTest, which comes with CMake, runs the registered test executables and reports their exit status and captured output. CMake compiles the test programs; CTest does not compile them or create the expected results. Build success confirms code compiles/links, while behavioral tests exercise what the code does.

| Test program | Real code exercised | Inputs and independent expectations |
| --- | --- | --- |
| `KeyGuideTrackerTests` from `tests/vim_tracker_tests.cpp` | VimState, portable bindings, and pure tracker functions. | Begin in Normal, feed `i` then Escape, expect Insert then Normal; feed `ciw`, expect operator/text-object progression then predicted Insert; verify `dfa` keeps `a` as an argument; check counts, temporary Normal, unsupported actions, double Escape vs held repeat, and navigation assumptions. Check pending context/confidence as well as mode. |
| `KeyGuideInputQueueTests` from `tests/input_queue_tests.cpp` | The actual bounded SPSC observation queue and loss/status mechanisms, independent of Cocoa. | Use a small capacity to force full/empty/wrap-around; check FIFO order, no overwrite of unread records, overflow signaling, and recovery. Include producer/consumer handoff checks without relying on precise timing or capturing real input. |
| `KeyGuideCommandPipelineTests` from `tests/command_pipeline_tests.cpp` | The actual input orchestration, ICommand/CommandRegistry, Registry, execute methods, and hint projection. | A fake Platform backend supplies portable events. Call UpdateInputInfo, execute queued commands, then inspect ECS state and key display. Queue `d`, `f`, `a` before one execution to catch stale-state pre-resolution; check stale generations/loss invalidation, registration completeness, and exact modifier/context hint selection. |

`tests/platform_stub.cpp` implements the same portable Platform declarations as the production backend and feeds authored sequences into real input.cpp. It neither replaces the tracker/registry with copies nor requires platform conditionals in production input code. It reports pointer/foreground/observation status explicitly, making those branches reproducible. Fixture expectations must not be calculated from the same binding table under test.

`tests/CMakeLists.txt` lists sources per test target explicitly, sets C++20/include paths, and registers each executable with `add_test`. Tracker and queue tests use no native backend or window. The command-pipeline target uses the stub, not `.mm` observer/window files or application main.cpp. It may need raylib headers/linkage for existing ECS math types/helpers, but does not initialize a window or request Input Monitoring.

Add a build-only aggregate target `KeyGuideTests` depending on the three test executables. Planned commands after these targets exist:

```sh
cmake --preset debug -DKEYGUIDE_BUILD_TESTS=ON
cmake --build build/debug --target KeyGuideTests
ctest --test-dir build/debug --output-on-failure
```

These commands now build and run the implemented test targets. Once enabled, the option persists in that build directory's cache until changed to OFF. Debug/Release/run preset names stay the same, and normal target builds need not build the tests.

Automated tests do not establish macOS permission behavior, real keyboard focus, event-tap passivity, layout translation, fullscreen behavior, or hover appearance. Validate those separately using the actual app and documented manual/native checks. Never use live Zed input injection or global recording merely to supply test fixtures. Passing tests supports the declared subset and assumptions; it cannot make inferred editor state exact.

References: [CMake add_test](https://cmake.org/cmake/help/v3.25/command/add_test.html) and [CTest](https://cmake.org/cmake/help/v3.25/manual/ctest.1.html).

## 7. File-by-file adaptation

The intent is to change the project's domain while preserving its organization. Existing files stay at their current paths except the coordinated KeyGuide project/resource renames. The empty pathfinding header/source have been deleted with their explicit source-list entries. The ECS and command files remain active.

### Registry and commands

| File | Keep | Edit |
| --- | --- | --- |
| `src/registry/registry.hpp` | Entire registry and component-array implementation; APIs, masks, tags, and storage strategy. | No overlay-specific change planned. |
| `src/registry/registry.cpp` | Entity create/destroy/clear and all tag operations. | Constructor registration list: replace game components with the overlay components listed above. |
| `src/registry/ecs_components.hpp` | `Entity` type, enum/count/specialization structure, component registration comments, and `Colour`. | Replace `World_Position` with `Entity_Position`; make `Entity_Size` rectangular; replace `Entity_Hitbox`/`Patrol_Path` and game tags with key/state/display components and `Key`/`Tracker` tags. Include portable state/binding types instead of random/patrol dependencies. |
| `src/registry/commands.hpp` | `ICommand`, `CommandRegistry`, `CommandDestroyEntity`, `DestroyEntity`, and the declaration/factory style. | Replace mover/player/enemy/particle subclasses with AddTracker/AddKey/TrackKey/SetTrackedMode/SetTrackingContext/InvalidateTracking/UpdateKeyboardView. Explicitly include the standard-library headers already needed by the queue. |
| `src/registry/commands.cpp` | `executeAll`, `clear`, and free-function wrappers that enqueue commands. | Replace AddPlayer/AddEnemy/AddParticle wrappers with corresponding overlay wrappers. Keep FIFO execution and clear-after-execution semantics. |
| `src/registry/global.hpp` | Both `inline Registry registry` and `inline CommandRegistry cmd_registry`, plus their existing ownership arrangement. | No overlay-specific change planned. |
| `src/gameLogic/command_execution.cpp` | The existing home of execute methods, entity creation/destruction, component attachment, and registry mutations. | Replace game execute bodies with tracker/key creation, ordered state transitions, context/resync handling, and key-display projection. Store the tracker entity ID after creation. Never call event-posting or editor-control APIs. |

### Entry point, lifecycle, logic, and coordinates

| File | Planned contents |
| --- | --- |
| `src/main.cpp` | Keep Debug/Release selection, `InitialiseGame`, `MainLoop`, `CloseGame`, `SetTargetFPS`, desktop loop, and Emscripten branch. Update displayed app description/diagnostics. Route unsupported platforms to preview through the platform interface. |
| `src/gameSkeleton/game_handling.hpp` | Preserve lifecycle and resize function signatures; expose existing initialization responsibilities only as needed. Avoid renaming the file/directory/function family solely because the app is no longer a game. |
| `src/gameSkeleton/game_handling.cpp` | Keep ConfigureFlags/SetFlags/InitialiseWorld/InitialiseGame/UpdateDeltaTime/UpdateToNewWindowSize/MainLoop/CloseGame. `InitialiseWorld` queues the tracker and physical keys instead of enemies/particles. `InitialiseGame` initializes raylib, render resources, immutable catalog, scene, and platform observation. Change frame ordering to execute input before view projection and drawing. Shutdown releases observer/menu/window/render resources in ownership order. |
| `src/gameSkeleton/game_state.hpp` | Replace unused Logo/Title/Gameplay/Ending values with coarse app lifecycle values such as Starting/Observing/Paused/ObservationUnavailable. This describes application operation, while `VimState` describes inferred editor state. |
| `src/gameLogic/input.hpp` | Keep `UpdateInputInfo` and familiar `Global::Input` namespace where useful for transient snapshots. Use portable event/status types only; no native headers/types/platform conditionals. Actual tracked mode lives only in the tracker component. |
| `src/gameLogic/input.cpp` | Remove A/D/Space/restart polling and screen-half/wall touch regions. Call portable Platform event/pointer functions, validate generic status/generations, and enqueue tracker/context commands. Native raw-event normalization and pointer queries are performed inside the backend, not here. The exact same source compiles with macOS, fallback, and test implementations. |
| `src/gameLogic/tick_logic.hpp` | Keep `TickOnce` and adapt `CameraLogic` to keyboard framing/zoom. Remove declarations for difficulty/spawn timers and game entity factories. Add declarations only for meaningful view projection responsibilities. |
| `src/gameLogic/tick_logic.cpp` | Keep frame progression and update structure. Replace player/enemy patrol, collision, score, and spawning with board fit, pointer hit testing, zoom interpolation, and scheduling display projection after input commands have run. No second Vim parser here. Use revisions/dirty flags so binding filtering runs when state/modifiers/layout change rather than recomputing all combinations unconditionally. |
| `src/gameLogic/pathfinding.hpp` | Deleted: unused empty placeholder. |
| `src/gameLogic/pathfinding.cpp` | Deleted: unused empty placeholder; removed from Make/Android/Visual Studio explicit source lists. |
| `src/gameWorld/conversions.hpp` | Keep the coordinate-helper role and useful `PositionWorldToScreen`/`PositionScreenToWorld` signatures. Implement consistent keyboard-to-window transforms and their inverse, incorporating board fit, zoom, and viewport. Remove motion-range and player-radius/world-height helpers. Canonical keyboard positions use x-right/y-down; document this instead of mixing in the game's y-axis convention. |
| `src/gameWorld/conversions.cpp` | Keep as the corresponding implementation entry; small inline conversion helpers can remain in the header under the existing conventions. |
| `src/gameWorld/traversal.hpp` | Keep the existing empty template. No gameplay traversal logic is introduced. |
| `src/global/global.hpp` | Keep `Global`, timing/frame fields, and render-resource lifetime helpers. Replace player/collided-enemy IDs with `trackerEID`; remove score/death/difficulty/spawn fields. Keep useful `Global::Render` texture ownership and use a raylib `Camera2D` view transform for fractional zoom. Hold pointer/view resources here without duplicating tracked mode or per-key ECS data. |

The reusable custom `Camera2` classes stay intact in their utility file. The overlay uses raylib's existing `Camera2D`. Key_Layout retains canonical geometry; Entity_Position/Entity_Size represent the current animated expanded geometry. Hit testing uses canonical geometry through the unzoomed base camera. Projection commands apply drawing-layout changes through the same ECS mutation path; Space participates in row reflow and draws through the same camera as the other keys.

### Rendering and settings

| File | Planned contents |
| --- | --- |
| `src/render/render.hpp` | Preserve `DrawFrame` declaration. |
| `src/render/render.cpp` | Keep BeginTextureMode/EndTextureMode and BeginDrawing/EndDrawing composition. Draw the keyboard ECS scene with Space mode/status in the same camera. Hovering Space adds no textbox. Draw zoomed content at suitable render resolution for HiDPI. |
| `src/render/helpers_render.hpp` | DrawKeyboard includes Space mode/status. The separate DrawStatus helper is removed with its hover textbox. |
| `src/render/helpers_render.cpp` | Reuse the current registry-array iteration and asserted component lookup approach. Build rectangular bounds from Entity_Position/Entity_Size, look up selected hint IDs, draw Colour/text/icons, and keep empty keys visible. It reads state/display components and does not mutate the tracker. Adapt debug output to inferred mode, pending state, hovered entity, queue loss, and frame timing; do not display typed text logs. |
| `src/render/shapes.hpp` | Preserve centered-text helpers. Replace `DrawText(TextFormat(text), ...)` with literal-text drawing so `%` in a command hint is handled safely. Add simple rectangular-keycap, modifier-icon, and measured text-run helpers for mnemonic emphasis where needed. |
| `src/render/animations.hpp` | Fill the currently empty file with small delta-time-based hover/zoom/key-width interpolation helpers. Use no animation framework; reduced-motion setting snaps to the target. |
| `src/settings/defaults.hpp` | Retain generic TARGET_FPS and DEBUG_RANDOMISATION_SEED. Adapt screen dimensions; define board spacing, key sizes/maximum expanded width, font scales, colors, hover zoom/timing, queue capacity, Escape reset interval, and passive-observer defaults. Remove player/enemy/particle/patrol/difficulty/Path/GW game defaults once their callers have changed. |
| `src/settings/settings.hpp` | Preserve `S`, screen dimensions/half-size, and `UpdateSettingsToNewWindowSize`. Replace playerRadius/wallDistance/worldHalfHeight/cameraPlayerOffset with overlay scale, font size, hover settings, reduced-motion preference, and layout/profile selection. Derived key geometry is applied through commands. |
| `src/settings/constants.hpp` | Preserve generic math/vector constants and file organization. Add named modifier/count constants here only if they are not already defined alongside portable input types; avoid competing definitions. |

The whole keyboard, including inactive keys, is always drawn from persistent key entities. Modifier availability comes from the exact-combination candidates in Key_Hint, while pressed state comes from observed input. Projection chooses the short label; hover uses its expanded label and exact key/chord. Mode changes update the key view rather than replacing entities or storing another keyboard definition in the renderer.

### Shared utility files

| File | Planned treatment |
| --- | --- |
| `src/elfTypesRL/IntVector2.hpp` | Preserve its arithmetic/conversion API; continue using it for screen dimensions where appropriate. |
| `src/elfTypesRL/Camera2.hpp` | Preserve both reusable camera classes and their APIs. Use the raylib camera for zoom rather than refactoring these shared types. |
| `src/elfTypesRL/raylib_extensions.hpp` | Preserve reusable vector/array arithmetic and formatting utilities. |
| `src/elfTypesRL/bit_masks.hpp` | Preserve its API; use the established bit-mask style for modifier flags. No broad utility rewrite. |
| `src/elfTypesRL/integer_math.hpp` | Preserve unchanged. |
| `src/elfTypesRL/macros.hpp` | Preserve LOG convention; add direct required includes only if needed to make it independently includable. Do not log global keystrokes. |
| `src/errorHandling/asserts.hpp` | Preserve the existing assertion mechanism; reuse it for registry/component invariants. Expected permission denial/unsupported input is ordinary status, not an abort. |
| `src/random/random.hpp` | Preserve streams, seed generation, APIs, and deterministic Debug behavior. Keep DEBUG_RANDOMISATION_SEED in defaults so this module remains usable. The overlay does not need random behavior for state inference. |
| `src/random/colors.hpp` | Preserve Hash32/ColorFromInt/general color helpers. Remove only enemy/particle color helpers that depend on retired gameplay defaults. |

### Build, editor, packaging, and documentation files

| File | Planned treatment |
| --- | --- |
| `CMakeLists.txt` | Keep CMake minimum, current raylib version/fetch/static linking, Debug definitions, runtime settings, asset copy, `run`, and Web branch. Project/target and Windows resource references now use KeyGuide. Add conditional Objective-C++/frameworks/macOS bundle and small test targets during overlay implementation. |
| `src/CMakeLists.txt` | Keep C++20, existing include root, and convenient source discovery. Exclude platform backend implementation files from the common recursive list, then explicitly add exactly one backend; explicitly select `.mm` files on macOS. This avoids compiling macOS and fallback implementations together. |
| `CMakePresets.json` | Preserve Debug/Release/run presets, Ninja, and binary directories; no project-wide build-system migration. Only add a test preset if useful. |
| `src/Makefile` | Keep supported targets/toolchain conventions and existing filenames. Update explicit common source lists with tracker/bindings and select macOS `.mm` files or portable fallback; teach its existing compile/link rules Objective-C++ only for native macOS. Preserve the first-party CMake workflow as the primary route. |
| `src/Makefile.Android` | Preserve the legacy workflow. Update its explicit source list with portable tracker/bindings/fallback only; keep Cocoa/event taps out. Document validation limits rather than claim it was tested without the SDK/NDK. |
| `.clangd` | Keep pointing to `build/debug/compile_commands.json`; the generated database supplies `.mm` compile flags. |
| `.gitignore` | Preserve generated-file exclusions; add only new generated test/package artifacts if necessary. |
| `.zed/tasks.json` | Preserve Configure/Build/Run task labels and commands. Optional test task uses the existing build directory. |
| `.zed/debug.json` | Keep platform debug adapters; program/working-directory paths and labels now identify KeyGuide. |
| `projects/VSCode/main.code-workspace` | Preserve root folder/CMake Tools setup and existing extensions; use generated CMake configuration rather than introduce another build system. |
| `projects/VSCode/.vscode/settings.json` | Preserve editor preferences/exclusions. |
| `projects/VSCode/.vscode/tasks.json` | Align stale legacy Make task paths with the actual repository root or label them explicitly as legacy; add CMake preset tasks only as needed for consistency with README. |
| `projects/VSCode/.vscode/launch.json` | Replace stale `../../src/raylib_game` launch/cwd references with actual KeyGuide Debug output during tooling cleanup; preserve supported debugger choices. |
| `projects/VSCode/.vscode/c_cpp_properties.json` | Correct stale C++14/root-relative configuration where used; prefer the existing CMake provider/compilation database for C++20. Preserve platform configurations. |
| `projects/VSCode/README.md` | Explain the same CMake/Debug/run workflow and any legacy-task updates. |
| `projects/VS2022/KeyGuide.sln` | Preserve solution structure and project references. |
| `projects/VS2022/KeyGuide/KeyGuide.vcxproj` | Update explicit source lists with portable tracker/bindings/fallback. Keep existing Windows project/toolchain; verify its language standard supports the project's existing C++20 requirement. Do not add macOS implementation files. |
| `projects/VS2022/raylib/raylib.vcxproj` | Preserve the third-party raylib project configuration; no overlay-specific edits anticipated. |
| `packaging/mac/Info.plist` | Update overlay display name/bundle ID and consistent executable metadata; configure agent/nonactivating app behavior as established by the native-window prototype. Set a minimum macOS version compatible with the APIs actually used. |
| `packaging/mac/KeyGuide.icns` | KeyGuide icon (keycap + AI tip bubble), generated from `packaging/icon/keyguide.svg`. |
| `packaging/windows/KeyGuide.rc` | Preserve resource structure; update user-visible product text if present while retaining paths/internal target names. |
| `packaging/windows/KeyGuide.ico` | Same icon for Windows, generated by `packaging/icon/make_icons.sh`. |
| `packaging/web/build-web.sh` | Preserve Emscripten activation, asset copying, and ZIP workflow; target output and archive paths now use KeyGuide. Web builds will use the preview backend. |
| `packaging/web/minishell.html` | Preserve raylib/Emscripten loading integration; update game-facing labels/instructions to keyboard preview where needed. |
| `assets/README.txt` | Describe overlay asset organization, font licenses, and resource-loading expectations. |
| `screenshots/screenshot000.png` | Preserve as the original template/game reference; add an overlay screenshot separately once the UI exists. |
| `README.md` | Replace game overview with passive overlay behavior, supported Vim subset, ECS/command architecture, user resync convention, permissions, and platform capability matrix. Preserve the familiar build/run/debug instructions and update only changed details. |
| `PLAN.md` | Maintain this concrete design as decisions are implemented; identify which milestones are actually verified. |
| `CONVENTIONS.md` | Preserve naming, file headers, include order and component registration conventions. |
| `LICENSE` | Preserve ownership/license terms. Added third-party font assets carry their own license files. |

The KeyGuide rename and pathfinding source-list cleanup have been applied to the build/editor/package files. Their scope is naming and removal of empty sources; no toolchain/dependency upgrade has been introduced. Observer/framework/backend/test integration above remains proposed work, with its purpose and scope documented before implementation.

### New files and why they are needed

Keep new domain code under existing directories. Add only the platform directory as a new source responsibility; a separate cheatsheet framework/directory is unnecessary.

| New file | Contents and boundary |
| --- | --- |
| `src/gameLogic/vim_state.hpp` | Small portable enums/structs: mode, Visual subtype, operator, wait reason, counts, context, temporary mode, uncertainty, baseline source, and VimState. Tracked_State in ecs_components wraps this type. |
| `src/gameLogic/vim_ex_commands.hpp` | Portable Ex catalog, bounded-prefix helpers and conservative execution classifications; generates keyboard continuation hints. |
| `src/gameLogic/keyboard_status.hpp` | Shared portable Space/menu status, small pending-command text and full explanations. No native or graphics dependency. |
| `src/gameLogic/vim_tracker.hpp` | Pure transition/invalidation/reset function declarations. Receives a state and normalized event; exposes no registry/platform/window dependency. |
| `src/gameLogic/vim_tracker.cpp` | Bounded grammar implementation, argument/prefix/operator handling, file-navigation assumptions, Escape recovery, and unsupported-event rules. Registry commands call this code. Resync lives here initially instead of adding another subsystem. |
| `src/gameLogic/vim_bindings.hpp` | HintId, hint/binding/predicate types, exact modifier combinations, and catalog-query declarations. Independent of ECS storage. |
| `src/gameLogic/vim_bindings.cpp` | Immutable Zed/default-Neovim profiles, short/expanded hints, mnemonic metadata, and shared transition definitions. Override profiles can be added deliberately without inventing a full Zed-keymap interpreter. |
| `src/gameWorld/keyboard_layout.hpp` | Initial canonical keyboard geometry and unshifted/shifted values, including wide/special keys. Small static definitions can live in this standalone header. `InitialiseWorld` passes these definitions into AddKey commands. Add a `.cpp` only if the implementation actually needs one. |
| `src/render/keyboard_geometry.hpp` | Base keyboard fit, key-to-screen rectangles, icon slots and selected-key camera bounds. |
| `src/render/text_layout.hpp` | Font measurement, complete hint wrapping, ellipsis fitting and actual key/chord labels. Uses existing raylib. |
| `src/platform/input_event.hpp` | Portable physical-key identifiers, logical/special key values, modifier flags, key/repeat/timestamp data, and foreground/observation-loss events. Contains no Cocoa or raylib types. |
| `src/platform/event_queue.hpp` | Small preallocated SPSC queue with explicit overflow/loss status. Independent of Cocoa/raylib so wrap-around/overflow/order can be verified without capturing input. |
| `src/platform/platform.hpp` | Narrow startup/shutdown, capability/permission, event-draining, pointer-position, foreground-app, and overlay-window interface. Pointer coordinates use simple portable data converted to Vector2 by the existing input layer. |
| `src/platform/mac/input_observer.mm` | Listen-only tap, bounded queue, layout-aware event normalization, foreground-app detection, observation-loss reporting, and permission/lifecycle handling. Never accesses the ECS or command vector from the callback. |
| `src/platform/mac/overlay_window.mm` | Native floating/nonactivating/click-through integration, verified raylib handle bridge, pointer coordinate conversion, and teardown. |
| `src/platform/mac/overlay_menu.mm` | VT Status, pause/quit/manual baselines and placement controls. Status reads the current main-thread snapshot when opened; control selections become ordinary application commands when drained. No commands go to Zed. |
| `src/platform/fallback/platform.cpp` | Capability reporting and local/replay preview support for Windows/Linux/Web/Android. All preview state changes use the same application command path. |
| `tests/vim_tracker_tests.cpp` | Pure grammar, Escape, uncertainty, and navigation-baseline regression cases. |
| `tests/command_pipeline_tests.cpp` | Actual CommandRegistry-to-Registry behavior: ordered same-frame transitions, entity/component initialization, and projected hints. No native window or global observation required. |
| `tests/input_queue_tests.cpp` | Observation queue order, wrap-around, bounded capacity, overflow/loss signaling, and recovery. |
| `tests/platform_stub.cpp` | Fake implementation of the portable Platform API for input/command integration tests; supplies authored events/status/pointer values without native observation. |
| `tests/CMakeLists.txt` | Explicit small test targets, C++20/include/link requirements, opt-in aggregate build target, and CTest registration. |
| `tests/fixtures/` if useful | Small synthetic sequences with expected outcomes. These are authored fixtures, not recordings of personal typing. |
| `docs/validation.md` | Native focus/input checks, passive-tracking limits, meaningful test results, and a platform verification matrix. |
| `assets/fonts/` if required | Readable font asset and its license if default raylib text cannot supply the required glyphs/quality. Modifier icons can be drawn directly with raylib. |

All new headers use the existing file comment, naming/include conventions, and initialized fields. No new test framework is needed for the initial tracker/pipeline checks.

### Content to remove, without deleting the framework

- Player/Enemy/Particle tags and World_Position/Entity_Hitbox/Patrol_Path gameplay definitions in ecs_components; replace them with the keyboard/state schema.
- CommandAddMover/CommandAddPlayer/CommandAddEnemy/CommandAddParticle and their wrappers/execute bodies; keep the command base, queue, destruction command, and file ownership.
- Player collision, enemy/particle spawning, difficulty, patrol, score/death/restart logic and their corresponding fields/defaults/render calls.
- Game normalized-world assumptions tied to wall height/player radius; replace them with keyboard coordinate transforms.
- RandomEnemyColour/RandomParticleColour and their retired gameplay-only defaults; preserve generic random/color APIs.
- Logo/Title/Gameplay/Ending enum values; use the existing file for application lifecycle.

Keep the registry, command execution file, world conversions, helpers, and compatibility configurations. The unused pathfinding pair has been removed; the empty traversal skeleton remains. The original game remains recoverable from Git; retaining a separate legacy copy is unnecessary for this adaptation.

## 8. Implementation order and acceptance

1. **Feasibility:** passive observer, permissions, nonactivating/click-through floating raylib window, global-pointer hover detection, and foreground-app detection. Demonstrate typing in Zed with the overlay visible.
2. **Static ECS keyboard:** update component registrations and AddTracker/AddKey commands; construct the board through InitialiseWorld and draw it through existing registry helpers. Preserve the existing lifecycle and build/run workflow. Establish the portable preview backend.
3. **Command-driven state tracking:** add ordered TrackKey commands, the pure tracker, modifier/context-indexed hints, explicit uncertainty, and manual resync. Add tracker and real command-pipeline tests.
4. **Resynchronization:** implement inferred file-navigation Normal baselines and repeated-Escape recovery; cover ambiguous mouse changes, panels, unsupported actions, and lost observations.
5. **Visual behavior:** modifier availability, mnemonic highlighting, held-chord selection, Camera2D pointer-anchored zoom, expanded key widths/row reflow, stable hover targeting, and Retina/reduced-motion behavior.
6. **Cleanup and packaging:** remove retired gameplay contents from their existing files, review explicit legacy source lists, document build changes and permissions, verify macOS Debug/Release, and check available portable builds. Preserve the skeleton/registry/command framework.

Meaningful automated cases include `i → Escape`, `R → Escape`, `v/V/Ctrl-v`, `d → w`, `2d3w`, `c → i → w`, `d → f → a`, `r → a`, prefix cancellation, `Ctrl-o` from Insert, palette/search input, unsupported macros, leaving Zed, file-navigation Normal assumptions, repeated Escape, Escape used as a literal argument, known non-editor contexts, and queue overflow. Tests must check mode AND preserved/cleared continuation and confidence, not merely whether a key matches a table entry.

Command-pipeline checks queue an entire sequence before executeAll, verify each event resolves against the preceding result, then project and inspect the expected key hint. Validate that every physical key survives mode changes, that all declared components are registered, and that displayed modifier availability corresponds to exact valid chords. No generic registry algorithm rewrite or duplicate tests for unchanged storage internals are planned.

Native validation uses user typing or a controlled input test fixture, with the overlay running and stopped: keyboard focus stays in Zed; input characters/order/repeats are unchanged; no synthetic events or active taps appear; tap permission failures and callback gaps affect only overlay accuracy. Add source review for event-posting/mutation/control APIs. Verify pointer hover at window edges, click-through selection, zoom stability, fullscreen/Spaces, moving between Retina/non-Retina displays, and shutdown resource cleanup.

Success means a small passive sheet helps with supported Zed Vim commands, makes uncertainty visible, and recovers simply. State is inferred under the selected workflow assumptions and is never presented as guaranteed.

## 9. Verification completed for the rename and cleanup

- Project, target, window title, presets, Zed debug profiles, native package metadata, Web output/archive names, and legacy Visual Studio/Make references use KeyGuide.
- Windows resource and Visual Studio solution/project paths have been renamed consistently; solution project references resolve.
- Both empty pathfinding files and their Make/Android/Visual Studio source-list entries are removed.
- The copied Debug/Release CMake caches referenced the original game's directory. Regenerating them with `cmake --fresh --preset debug` and `cmake --fresh --preset release` established this workspace's source paths.
- `cmake --build --preset debug` and `cmake --build --preset release` succeeded on macOS, producing `build/debug/KeyGuide/KeyGuide` and `build/release/KeyGuide/KeyGuide`.
- JSON/plist/Visual Studio XML, renamed solution references, Web script shell syntax, and whitespace checks passed. The plan inventory covers every remaining source-controlled/project file.
- These results validate the renamed inherited scaffold. Native passive-input/focus/hover behavior remains unimplemented and unverified; Windows/Web/Android build execution has not been validated here.


## KeyGuide / passive Blender implementation — 2026-10-10

The app expands to Zed, Ghostty/Neovim and Blender. Executable/target/package names
use KeyGuide and the menu item is KeyGuide. C++20, static raylib, Registry API,
ICommand pipeline, directories, presets and fallback backend remain. CMake option
names and macOS bundle identifier stay compatible with the established project.

- Added shared tracking/hint metadata and separate Blender state, bindings and tracker.
- Added native Blender bundle detection, physical function/numpad identities and
  mouse button press/release identity through the existing listen-only tap.
- Added Object/Mesh Edit assumptions, transform and bounded modeling/selection tools,
  menu/search/rename contexts, exact modifiers and Blender's Mac aliases.
- Added app-specific focus behavior and manual Blender baselines. Escape cancels
  operations, but never establishes Object Mode or recovers Blender RESYNC.
- Ordinary viewport clicks preserve the assumption; unsupported actions, unknown
  menu/search results, undo/redo, input loss and interrupted operations need resync.
- Added optional function row/numpad, state-preserving keyboard rebuild, selectable
  previews and window sizing from the displayed layout.
- Added Blender grammar tests and extended the existing input/command/ECS tests.

Scope: default Blender 4.1 keymap and a manually assumed mesh 3D Viewport. No screen
reading, Blender state access/add-on, input modification, blocking or injection.
Mouse-driven editor/mode/tool changes remain a documented inference limitation.
