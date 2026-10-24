# KeyGuide validation

Implementation checks performed on macOS arm64, 2026-10-03; appearance checks and
builds updated 2026-10-05; Ghostty/Neovim and RESYNC checks updated 2026-10-07;
passive Blender and app rename updated 2026-10-10. The KeyGuide menu redesign, AI tips and
AeroSpace config parsing were checked 2026-10-24 (see the AI tips rows); rows that name an
earlier menu item say so.
The optional [Blender state bridge](blender_state.md) was added and checked against
Blender **5.2.2 LTS** on the same date. Headless checks use a disposable factory scene;
the bridge has not yet been tested during a live, permission-granted editing session.
Readability / idle-rendering checks from the same date are recorded in
[performance notes](performance.md).
The app uses C++20,
static raylib 6.0 and the project's original Registry/CommandRegistry interfaces.
This record distinguishes executed checks from the manual checks still needed.

| Check | Result |
| --- | --- |
| Blender state bridge | Existing tracker/pipeline suites verify mode and selection correction, stale-report rejection, known/unknown modal tools, automatic tool-end recovery, conservative popup recovery, unsupported contexts without hints, lost connection, observation loss and preview isolation. |
| Function layer | Native/portable command-pipeline fixtures cover `1`–`0`, `-`, `=` to F1–F12 display aliases in Blender and Zed, exact modifier selection, availability markers, original-number tracking, Fn disabled after Fn+number types a number, actual F-key feedback, layer recovery after a lost F-key release, macOS-reserved F11 hiding, hover locking, RESYNC/inactive suppression and input-loss reset. Blender tracker fixtures cover F9 without a menu, ignored macOS Ctrl-F2/F11/Ctrl-F3 including during tool recovery, Cmd-F2 rename and Shift-F5 to the 3D Viewport. Native callback fixtures verify Fn snapshots, false Function flags on navigation/F keys, preview forwarding, loss recovery and unchanged event pointer/flags/keycode. Real-renderer frames verify five-slot icon spacing, normal/Fn/Shift-Fn hints, bounded hover, identical pixels across a frozen context change and idle redraw suppression. Live physical Fn behavior remains a keyboard/OS-dependent manual check. |
| Hover presentation lock | Command-pipeline checks verify entry over a window margin in the same batch as a mode change, stable mode/hint/description snapshots, live key/modifier feedback, input loss and recovery, app-profile switches, continued Blender tool tracking without redraw requests, immediate release and initial startup under the pointer. Actual renderer checks compare identical pixels while a Node-to-Mesh mode change is hidden by the hover lock, then verify the latest mode after exit. |
| Node editors (2026-10-12) | Native/portable tracker and command-pipeline checks cover Geometry/Shader/Compositor and legacy NODE_EDITOR, independent scene modes, group Tab behavior, exact modifier hints, unique active chords, reroute/cut/mute release, link/resize cancellation, layout transforms, unsupported headers/sidebars and return to viewport hints. Seven add-on unit scenarios pass, including graph types/UI regions. Actual Blender 5.2.2 RNA and UDP checks verify node editor identifiers and unchanged fixture nodes/links. The background API fixture selects a real area directly; pointer hit testing is checked separately. Authored real-renderer screenshots verify complete modifier hover hints and bounded expansion; static redraw/hide/show checks pass. Live interactive node editing remains a manual check. |
| Curve Edit | Native and portable suites verify `EDIT_CURVE` hints, adding/deleting/connecting points, safe handle/add/delete menus, Tilt/Radius numeric controls without axis hints, Ctrl/Command right-click addition, observed drag/extrude/duplicate, remembered edit type and Vim resync isolation. The real command pipeline verifies Curve Edit ECS hints and Alt-S/bare-S display changes. Blender 5.2.2 LTS factory-scene checks verify Curve Edit reports over UDP without changes to point/handle coordinates. Live interactive curve editing remains a manual check. |
| Blender add-on / native IPC | Seven Python unit scenarios pass; real loopback UDP / Core Foundation fixture verifies ordering, session tokens, focus/loss epochs, silent heartbeats, timeout and cleanup. Actual Blender 5.2.2 LTS factory-scene checks verify registration, UDP handshake, real Object/Edit/Sculpt/selection reads, reload subscriptions and cleanup. Add-on does not execute operators; controlled scene changes occur only in the isolated API test. Live viewport/Retina/multiple-window behavior and combined Energy Impact remain manual checks. |
| Native macOS Debug / Release | Built and linked as KeyGuide; release app bundle assembled. |
| Passive Blender tracker | Four native and fallback CTest suites pass. Blender checks physical chords despite Unicode/Caps differences; Object/Edit, constraints and numeric-input markers, G/R/S, extrude/duplicate, inset/bevel/knife/loop-cut stages, selection mouse rules, menu/search/rename, aliases and explicit resync. Search/rename/numeric text is not retained. Escape recovers interrupted tools when the base mode is still known, but not unknown modes or observation gaps. |
| Less frequent Blender RESYNC | Authored defaults checked against the installed 4.1 keymap/UI menus: save/playback/frame navigation, shading/gizmos, subdivision, Ctrl/Shift selection types, numeric arithmetic, bevel segment keys, numpad loop counts, safe menu clicks/Enter/number choices and popup cancellation. Real input → commands → ECS tests confirm hints return after safe menu completion or Escape from a focus-interrupted tool; hints stay hidden after lost input. Unknown mode/menu/search results remain uncertain, and later tool interruptions cannot downgrade unknown-mode uncertainty. No Blender state API or OS input posting was used. |
| Blender input / ECS integration | Authored Platform events through real input.cpp and commands verify profile switches, stale-generation rejection, input loss, idle-mode retention, interrupted-operation resync, Caps/Command matching, hidden hints in RESYNC and state-preserving function/numpad rebuild. |
| Native Blender / passive callback | A temporary Objective-C++ fixture exercised actual bundle/PID classification and normalization: Blender/F3/number-row/numpad/Option+Caps/mouse-release mapping, generation changes and inactive text exclusion. It called the real callback with a synthetic event object and verified identical pointer/flags/keycode return. It also exercised actual KeyGuide Object/Edit selectors. No tap was created and no events were posted. |
| Native section resize | A Cocoa fixture called the actual resize function at top-right with a nudge: right/top edges were retained, the larger layout fit the screen and foreground focus stayed unchanged. No tap or input posting was used. |
| Blender render | Real renderer with authored state/pointer inputs produced inspected Object/Edit, Control hints, MOVE constraints, RESYNC and widened extrude-hover frames, including optional function row/numpad. No app state reader, global observer or OS input injection was used. |
| Live Blender | Needs hands-on verification with Input Monitoring granted and the add-on enabled. Real-state API checks pass in a headless factory scene; viewport geometry, popup interaction and Energy Impact remain unmeasured during live editing. |
| Ghostty / Zed foreground classification | Authored native bundle/PID fixtures recognize Zed, Zed Preview and `com.mitchellh.ghostty`; profile switches advance generations, repeat snapshots do not, and unrelated/nil apps remain inactive. Inactive key normalization retains no logical text. Known Command shortcuts remain classifiable when Unicode text is absent. No event tap or global input was used by this fixture. |
| Default Neovim profile | Portable tests cover counts/operators, `ciw`, Visual/Replace, supported Insert/Replace Control edits, register waits, command-line/search cancellation, narrowed Ex continuation hints, known Ex results, hidden Ex uncertainty, and repeated Escape. Profile-specific `Y`, Insert `Ctrl-b` and bracket-indent hints are checked. Ghostty host navigation resets assumed Normal; Neovim window/buffer navigation preserves temporary Insert return. |
| Ex / Visual / multiple-cursor regressions | Pure tests and real input → commands → ECS tests pass for Ctrl-v + jjj + I/A, Visual gI/gA, Zed gl/gL/ga/g>/g<, representative default macOS cursor chords, gn/gN and operator completion. Visual gI/gA hints are hidden in Normal and in Neovim. Ex hints narrow after :w (q → :wq); exact Control availability remains separate. Known :w/:write!/:bn/:noh and mode-changing Ex commands complete locally; quit/script/argument/edited commands become uncertain. Enter cannot clear an observation loss. Prefix erasure, cancellation, literal Escape and privacy of arguments are tested. |
| Ex / Visual render | The real renderer with authored events/pointer positions produced inspected colon-start, :w continuation, hovered q → :wq, quit-to-RESYNC, block selection/Insert and Visual gA screenshots. Space showed white COMMAND with the prefix, and hovered explanations wrapped inside the widened key. No native event tap, global input or personal typing was used. |
| Editor switching / RESYNC projection | Actual input → CommandRegistry → ECS tests select profiles before same-frame keys, discard stale Zed events on Ghostty entry, clear prefixes, and switch back to Zed hints. RESYNC has no hints or modifier availability on any key, including Caps/Esc, while physical pressed feedback remains. |
| Clean Neovim comparison | The installed Neovim 0.12.5 was started with `--clean --headless -i NONE` in an isolated scratch process. Its default `Y` maps to `y$`, modern buffer mappings exist, and Zed indent mappings do not. Authored input sent only to that disposable instance confirmed Insert remains Insert after Ctrl-b, Ctrl-o + Ctrl-w h, Ctrl-o + ]b, Ctrl-PageDown and Ctrl-r a; Visual replacement and colon cancellation return Normal. Further isolated comparisons confirmed Visual/Visual Line/Visual Block I/A enter Insert, Visual increments exit to Normal, :star/:startinsert enter Insert, :startr/:startreplace enter Replace, and Ctrl-o + :stopinsert ends in Normal rather than returning to Insert. No OS keys or input to the user's Neovim/Ghostty/Zed sessions were sent. |
| Neovim / RESYNC render | Real renderer with authored platform/profile/pointer values produced inspected Normal, Insert-with-Control, inactive Ghostty, RESYNC pressed-key and Space-hover screenshots. RESYNC hides even the dim per-key availability slots and Space hover label; the white RESYNC label, key outlines and physical feedback remain. |
| macOS app bundle | 2026-10-24: `package-macos` rebuilt `/Applications/KeyGuide.app` (executable, plist, `KeyGuide.icns`, assets and font license) and signed it with the detected Apple Development identity; `codesign --verify --strict` passes and the designated requirement is certificate-based. `NSWorkspace` shows the new icon. A throwaway build into a folder with a space installed correctly and left no staging copy; a deliberately unknown identity failed the build and left the previous install intact and valid. A `;` in `KEYGUIDE_APP_DIR` is rejected at configure time. |
| Legacy Make on macOS | Previously built with C++20 and `-Werror`. Its 32 source-adjacent `.o`/`.d` files were removed; subsequent verification uses CMake only. |
| Fallback backend on macOS | Built with `KEYGUIDE_NATIVE_MACOS=OFF`; all four portable CTest suites passed, including Blender. |
| Windows x64 fallback | Cross-compiled and linked with MinGW, including the Windows resource; not executed on Windows. |
| Native launch / focus | NSWorkspace and CGWindowList metadata confirmed a visible floating-layer window, no foreground acquisition, original foreground retained, and clean exit. No keys were injected. |
| Compact keyboard / hover / Insert rendering | Real renderer exercised with authored Platform events/pointer positions; screenshots inspected. All rows span 15 units and fit the 748×243 window. Backtick matches number-key width; bottom Control and both Options are 1 unit. Space shows mode/pending/status; hovering Space adds no textbox, and explanations remain in the status lines at the top of the KeyGuide menu. Space follows the keyboard camera during zoom; mode/status text is white and inactive status is gray. Available modifier icons are white; held compatible modifiers and Caps Lock are green. Insert hides Control commands until Control is held; incompatible Shift+Control dims their icons. Caps/Esc, Up, Backspace and bottom-right arrow hover kept stable targets and fitted highlighted bounds throughout animation. An inactive fixture showed key presses/releases and held modifiers with no command hints, including on Caps/Esc. No system pointer/key events were generated. |
| Native window placement / shutdown | A temporary Cocoa fixture called the real placement functions: top-right default, seven presets, nudge directions and bounds, reset, and clean shutdown passed without changing foreground focus. One display was attached; next-display is a no-op here, multi-display behavior remains a manual check. |
| Native status submenu (earlier menu) | A Cocoa fixture called the former menu delegate with authored state: mode and `d2` matched Space, entries were read-only, and foreground changes refreshed the label. That submenu has been replaced by the status lines at the top of the KeyGuide menu, which have not been re-checked. Full hands-on menu interaction remains a manual check. |
| Native appearance menu / visibility | A Cocoa fixture exercised the actual menu selectors and window functions: the former Hide/Show item updated both visibility and its title (now the Show Keyboard checkmark), retained the KeyGuide status item and placement, preserved the foreground app and click-through/nonactivating overlay policy, and reported no pointer hover while hidden. All four transparency modes updated their exclusive checkmarks (now under Appearance ▸ Transparency). Preview/overlay switches retained a hidden window. No tap or input injection was used. |
| AI tips (unit) | 2026-10-24: all six CTest suites pass. `KeyGuideGuideTests` covers prompts, JSON extraction (prose, fences, bare arrays, string/oversized ids), grounding (own keys as whole chords, punctuation and space-separated chords, per-app Zed/Neovim allowlists, mouse gestures), sanitising, linear noise stripping, the catalog and AeroSpace bindings. `KeyGuideAeroSpaceConfigTests` covers key notation, hints, `mode -- service`, `;` sequences, options before arguments and the real `~/.aerospace.toml` (48 bindings, no warnings). Two review rounds by Sol 6.1 (job cancellation and epochs, restart after Clear/Stop, hover retry backoff, chord false positives in prose) are fixed; the worker scheduling itself has no unit test because it needs a mocked provider. |
| AI tips (live) | 2026-10-24, M1 Pro: the default Ollama `qwen3.5:4b` explained 81 of 85 AeroSpace shortcuts in 73 s with no grounding rejections (`--explain aerospace`, isolated `HOME`). Earlier, `deepseek-r1:8b` produced grounded AeroSpace tips headlessly (`--explain aerospace`, 16 cached before the test server stopped) after switching to the native `/api/chat` with a 16k context; the OpenAI-compatible route truncated its reasoning (`truncated = 1`). `qwen:4b` returned no usable tips (rejected). DigitalOcean: `inference.do-ai.run` reached; a dummy key produced `HTTP 401 - the API key was rejected (Unable to authenticate you)` and cancelled the run. A real DigitalOcean key, the Keychain dialog, menu clicks and Input Monitoring for the new bundle id were not exercised. |
| AI tips (render) | 2026-10-24: with Vim tips from `qwen3.5:4b` (564 of 565 in 7.6 min, none rejected), `--preview --pointer` screenshots of the real renderer show the hovered key expanding exactly as in KeyboardCheatsheet and the tip in a separate opaque panel: below `w` (`screenshots/keyguide_tip.png`), right of `w`, flipped left of `;` near the right edge, flipped above `v` in the lower row, and no panel with **Off**. Space never shows a tip. Earlier, tips were requested lazily by hovering (about 4 minutes with `deepseek-r1:8b`). The menu tree was checked with `--print-menu`. |
| Independent opacity / hover rendering | A fixture used the real frame renderer and read actual OpenGL framebuffer RGBA: opaque mode had full alpha; semitransparent background alpha was 51/255, with an 80% key layer composited above it. Background and key RGB matched premultiplied composition. Always, OnHover and WhenNotHovered produced the expected alpha on both pointer states; hovering the empty window margin counted as hover. Off restored full opacity without recreating the window. No personal input was observed. |
| Tracker tests | Passed: modes, operator/count/argument progression, text objects, prefixes, temporary Normal, literals, registers, unsupported commands, context cancellation and distinct Escape recovery. Pending sequences include counts, shifted arguments, registers and Ctrl-w; completion/cancellation/focus/invalidation clear them. The buffer stays bounded and excludes Insert, literal, search and palette text. Space status priority was checked. |
| Queue tests | Passed: empty/full, FIFO, wrap, overflow/loss and 100,000-record SPSC handoff. |
| Command pipeline tests | Passed: real input/commands/ECS, same-frame sequences, generation/loss rejection, registered components, exact held/released modifier hints, incompatible combinations, Caps Lock, persistent entities, widened key center/edges, icon slot separation and camera bounds for every key at compact/minimum window sizes. Observed Escape leaves Insert and lights/releases the Caps/Esc display; ordinary Caps flags leave Insert unchanged. Inactive physical input updates pressed/modifier feedback without advancing tracker revision or changing mode, and clears all hints. Held keys survive native focus changes; queued releases from the prior foreground generation clear them. Loss clears feedback and rejects stale records; pause rejects presses. Returning to Zed assumes Normal. Real tick checks verify fixed original hitboxes when an expanded edge key covers a neighbor or a gap. Space follows row reflow and hover zoom; its original hitbox stays fixed and its hovered drawing bounds fit the window. CPU font fixtures verify glyph advance, inter-letter spacing, scaled text, zero-advance fallback, wrapping and ellipsis fitting. |
| Preview input after layout changes | A temporary raylib API fixture exercised the real preview backend with authored input: Escape press/release/repeat remained available independently of the displayed layout, `i` then Escape returned to Normal, Caps Lock emitted only a modifier snapshot, and Up retained its physical mapping. No window or OS events were created. |
| Native inactive normalization | A temporary Objective-C++ fixture called the real normalizer on authored records: inactive downs/ups/flags retained physical identity, modifiers and Caps state, with empty logical values even for Escape and Control/Command chords. Foreground Zed normalization and unsupported-value handling remained intact. The callback skips Unicode reads and command-producing mouse clicks when the foreground gate is inactive; passive pointer movement over the overlay remains available for hover. No tap or native events were created by this normalization fixture. |
| Passive input source review | One tap creation path, `kCGEventTapOptionListenOnly`; callback always returns the original event. No OS keyboard/mouse posting, mutation, filtering, injection or hotkey registration. The callback performs bounded reads, a queue write or hover-region check, and a coalesced run-loop signal, with no sleeps, logs or ECS calls. An internal GLFW application event wakes the UI from the main thread; it does not enter the OS input stream. |
| Idle rendering / wake | Static frames and settled hover stop rendering; hidden windows continue commands without drawing. Native/fallback tests check dirty input and finite animation completion. Renderer/native scratch fixtures check hide/show, transparency, worker-to-UI wake, unchanged event return, mouse filtering and blocked-loop shutdown. Warmed native idle measured 0.170% CPU / 2.066 interrupt wakeups per second with observation unavailable; Energy Impact and live permission-granted input still need measurement. |
| Portable input boundary | input.hpp/.cpp has no native headers, native keycodes, native calls or platform preprocessor branches. |
| Native Input Monitoring / live Zed typing | Not exercised with permission granted. |
| Native observer recovery | Standalone macOS fixture passes denied → granted recovery without a menu click, revocation/cleanup, bounded retries, immediate explicit retry, separate creation/source diagnostics, disabled-tap recovery, pause/preview/Secure Input gates and worker termination when its run loop finishes. It verifies listen-only creation options and identical callback event/flags/keycode return. Native permission/tap APIs are replaced with fixtures and a local Mach port; no global tap, permission prompt or OS input posting occurs. |
| Fullscreen / Spaces / click-through / menu interactions | Implemented, need hands-on verification on the intended desktop. |
| Linux / Web / Android / MSVC / fresh-clone IDEs | Sources/build routes retained; corresponding SDK/runtime checks not performed. |

The default native deployment target and bundle minimum are macOS 11, the minimum
supported by the current C++ toolchain and Apple Silicon. The permission APIs
began in macOS 10.15, but an older deployment target is not validated here. Existing
explicit CMake deployment choices are respected. Main app builds select one backend;
the CTest programs never create a tap or window.

The native window focus policy preserves the existing GLFW/AppKit class identity.
Replacing the window's class caused an observer-cleanup exception when the placement
fixture drained its autorelease pool; the revised policy passed that same fixture
and the nonactivating-launch check. It still refuses key/main status in overlay mode
and allows ordinary focus in preview mode; overlay → preview → overlay also passed
the same fixture. No keyboard observation behavior changed.

## Ghostty manual check

Live user typing in Ghostty was not exercised by the automated checks. Restart the
rebuilt overlay, enable observation if needed, then run `nvim --clean` in Ghostty
and focus its editing buffer. Check `i`/Escape, `R`/Escape, `v`/`V`/Ctrl-v, `dw`,
`ciw`, and Insert Ctrl-o + `dw`. Confirm Ghostty receives every key normally and
the status lines at the top of the KeyGuide menu identify Ghostty/default Neovim. Check terminal tab/split shortcuts,
returning from Zed, and stale pending prefixes. Press Escape twice to recover
RESYNC and confirm all hints disappear while desynced but pressed feedback remains.

Ghostty focus alone cannot establish the running TUI or its mode. Pause for shell,
tmux/other TUI, terminal-job buffers or custom maps; resume and resync in Neovim.
Check `:` → `w` hints (q suggests :wq), known `:w`/`:noh` completion, `:startinsert`/
`:startreplace` mode changes, cancellation and Ctrl-u erasure. Quit commands,
arguments, history/completion, cursor editing, scripts, paste and Ghostty UI prompts
require resync. For Zed, verify Ctrl-v → jjj → I/A, occurrence selection gl/gL/ga,
skipping g>/g<, Visual gI/gA and the macOS cursor shortcuts against a matching buffer.
The supported grammar is bounded and profile semantics do not prove actual mode.

## Automated checks

```sh
cmake --preset debug -DKEYGUIDE_BUILD_TESTS=ON
cmake --build --preset debug
cmake --build build/debug --target KeyGuideTests
ctest --test-dir build/debug --output-on-failure
cmake --preset release
cmake --build --preset release
cmake --build build/release --target package-macos   # installs /Applications/KeyGuide.app
```

All expected sequences are authored fixtures. No personal input recorder or event
replay into Zed is part of the test workflow. Queue and tracker tests use no graphics.

The observer recovery regression is macOS-only and can run separately without
changing CMake targets or privacy permissions:

```sh
clang++ -std=c++20 -fobjc-arc -Isrc -Itests tests/mac_input_observer_tests.mm \
  -framework Cocoa -framework Carbon -framework ApplicationServices \
  -o /tmp/keyguide_observer_tests
/tmp/keyguide_observer_tests
```

On 2026-10-10, a live INPUT OFF report was traced to `tccd` rejecting the rebuilt
app's code requirement for `kTCCServiceListenEvent`: the saved and current executable
hashes differed. The app had no registered event tap. The existing bundle was
linker/ad-hoc signed and `security find-identity -v -p codesigning` reported no valid
identities. Automatic recovery cannot grant permission or repair a stale macOS
permission entry. See the README's refresh steps and the [signing
commands](build_and_run.md#packaging-and-signing-macos); verification with a real
grant remains a manual check.
Command tests link raylib for existing types and use a stub Platform, without calling
InitWindow or requesting permission. Assertions stay enabled in Release tests.
The existing command-test target also compiles `tick_logic.cpp` for actual hover
selection checks. Authored font metrics supply its text measurements without loading
a texture or opening a graphics context.

Optional fallback verification:

```sh
cmake -S . -B build/portable -G Ninja -DCMAKE_BUILD_TYPE=Debug -DKEYGUIDE_NATIVE_MACOS=OFF -DKEYGUIDE_BUILD_TESTS=ON
cmake --build build/portable
ctest --test-dir build/portable --output-on-failure
```

## Native checks to perform with a scratch document

1. Launch the native app above Zed. Confirm the insertion cursor/focus stays in Zed
   on launch and while hovering. Clicking the overlay must reach the application
   underneath. Confirm behavior on another display, in fullscreen and across Spaces.
2. Use **Grant Input Monitoring…** (or **Retry Observation**) in the KeyGuide menu. Test denial, approval (including
   any restart required by macOS), revocation, enable/retry, pause/resume and quit.
   Unavailable/paused status must be visible in Space; observation gaps must clear continuations.
   Switch to another app while holding a key: its highlight should remain until release.
   While inactive, type/hold/release keys and modifiers: expect physical feedback with
   no command hints, including on Caps/Esc, and no changes to the tracked Vim mode.
3. In a scratch Zed document with default Vim bindings, type ordinary Vim sequences
   such as `i` → text → Escape, `R` → text → Escape, `v/V/Ctrl-v`, `dw`, `2d3w`,
   `2d0`, `ciw`, `dfa`, `ra`, `gg` and Insert `Ctrl-o` → `dw`.
   Verify the document receives all keys normally and the hints follow each state.
4. Verify plain `u` and shifted `U` availability changes with mode. Modifier icons
   indicate chords compatible with those held; active held modifiers use a distinct color.
   In Insert, `u` has no command hint until Control is held, and Shift+Control dims
   incompatible icons. Inactive keys
   retain outlines. Hover must reveal the actual key and expanded hint; widening
   must keep the target steady, and long descriptions wrap inside the expanded key.
   Hitboxes stay in their original unzoomed locations: an expanded edge key must
   release hover at its original boundary. Check noninitial mnemonics such as the
   `o` in Insert's Ctrl-o `normal` hint; the tinted letter and underline must align.
5. Click/select inside Zed: expect RESYNC. Focus the editor and press/release Escape
   twice within one second: expect assumed Normal. Hold Escape: it must not count as
   two presses. Interrupt the pair with another key or wait longer: no forced baseline.
6. Switch files/panes with supported shortcuts: expect assumed Normal. This tests
   the agreed leave-files-in-Normal convention; it is not verification of Zed's mode.
7. Open search or `:` palette. Typed text must not become Normal-mode Vim commands.
   Palette execution, `.`, macro playback, digraphs and unsupported actions require
   resync. In Insert literal `Ctrl-v`, the first Escape is a literal argument.
8. Switch input source away from US/ABC or enable Secure Input in another app:
   expect unavailable/uncertain. Return to the supported source/editor and resync.
9. Try hover with macOS Reduce Motion enabled. Zoom/width changes should snap instead
   of animate. Local preview also respects the system preference on macOS.
10. Check the top-right launch position and the Appearance ▸ Position and Nudge submenus. Exercise all
    presets, both nudge distances at screen edges, Reset Settings, and Move to Next Display. Moving
    the overlay must preserve editor keyboard focus. Hover Escape, Backspace, and
    bottom-row edge keys; the highlighted key must remain inside the keyboard area.
11. Space should show only mode plus the current small pending sequence, such as
    `d2`, `ci`, or `Ctrl-w`. Completing/canceling clears the sequence. Space zooms
    with the keyboard; hovering it shows no textbox and the status lines at the top
    of the KeyGuide menu show the explanations. An unrelated foreground app shows INACTIVE; switching between Zed and Ghostty selects the new profile.
12. Use Show Keyboard and all Appearance ▸ Transparency choices over light and dark editor content.
    Hiding the keyboard must leave KeyGuide available and observation running; showing the overlay must
    preserve placement and editor focus. Check whole-window hover, including margins,
    and readability on Retina/non-Retina displays. Opacity applies to keys and their
    text/icons together, with the background underneath faded separately.

These checks cannot prove a passive tracker knows editor focus, buffer content,
custom keymaps, search success or hidden asynchronous effects. All Normal-on-entry,
navigation, manual and double-Escape baselines remain explicitly assumed.

## References

[Apple's listen-only event-tap option](https://developer.apple.com/documentation/coregraphics/cgeventtapoptions/listenonly)
defines the passive observation API. The implemented subset was checked against
[Zed's default Vim bindings](https://github.com/zed-industries/zed/blob/main/assets/keymaps/vim.json);
this catalog is a bounded model, not a full copy of Zed's binding engine.

Screenshots use authored fixture state: [Normal](../screenshots/keyguide_normal.png),
[hover](../screenshots/keyguide_hover.png), [Insert](../screenshots/keyguide_insert.png),
[pending sequence](../screenshots/keyguide_pending.png),
[inactive physical feedback](../screenshots/keyguide_inactive.png).
The original `screenshot000.png` remains as the game/template reference.


## Blender manual check

1. Launch the rebuilt KeyGuide app; enable Input Monitoring from KeyGuide if needed.
2. In Blender's default keymap, select a mesh and keep the pointer in the 3D Viewport.
   Check its actual Object/Edit selector and choose the matching Resync Mode entry.
3. Check Tab, Edit 1/2/3, G → X → 3 → Enter, Shift-axis planes, R/R and S;
   precision uses Shift, snapping inversion uses Control rather than Command.
4. Check E/Shift-D cancel behavior, I, Ctrl-B/Cmd-B, K, Ctrl-R/Cmd-R and their
   distinct mouse completion stages. Knife right click remains in Knife. Box drag
   completes on release; Circle remains until explicit completion/cancellation.
5. Check menu/search cancellation, an unknown execution result, undo/redo and RESYNC.
   Confirm RESYNC hides hints, still shows pressed keys, and Escape twice cannot
   recover unknown mode/history results. Explicit Object/Edit baseline should recover
   the overlay only. Shading/view/pivot/orientation/snapping and Apply/Merge/Mesh Delete
   choices should preserve the assumed mode after click, Enter or numbered selection.
6. Switch apps while idle: the assumed Blender mode should remain. Switch during
   a transform: return should show RESYNC; a fresh Escape in the 3D Viewport should
   restore the previous assumed mode. Further input before Escape must require a
   manual baseline. Unknown menu results, input loss and pause/resume must still
   require a manual baseline even after Escape.
7. Toggle Show Function Row/Show Numpad under Appearance; window sizing should follow the selected layout,
   preserve mode and stay within the current display. Distinguish top-row digits from
   numpad views; hidden F3/numpad input should still reach the tracker.
8. Switch to Zed/Ghostty and verify their existing Vim/Ex hints and Escape recovery.

UI mode/tool/editor changes cannot be detected reliably by this passive implementation.
Use a manual baseline or pause for those changes, other object types/modes, custom
keymaps, Numpad Emulation, a different editor or other unsupported workflow.
