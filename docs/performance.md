# Readability and power checks

The optional [Blender state bridge](blender_state.md) adds a 250 ms Blender-side
timer while actively observing Blender, with a two-second inactive interval.
State changes use loopback messages and a sleeping native socket source; unchanged
heartbeats do not dirty the keyboard. This has not yet been measured in a live
editing session. Measure Blender as well as the overlay when comparing energy use.

## Changes implemented

- `.clang-format` expands definitions, indents all namespaces, keeps include order
  and uses four spaces and aligned braces. All project C++ and Objective-C++ files
  in `src` and `tests` were formatted; dependencies and generated files were excluded.
- `input.cpp` reports whether a batch/status/pointer actually changed. Moving the
  pointer outside the window does not dirty the keyboard.
- `game_handling.cpp` runs the existing projection/tick/render path on changes or
  during hover animation. Hidden windows still process commands but skip graphics.
- `animations.hpp` snaps tiny residual differences to their targets, so hover can
  finish instead of continuously drawing an imperceptible change.
- raylib's event pump runs on idle iterations. Rendered iterations retain
  EndDrawing's event pump; polling twice would erase preview key edges.
- macOS input signals a coalesced main-run-loop source. Its main-thread handler
  posts an internal GLFW application event to wake the UI. The callback still
  returns the identical event and never changes OS input delivery.
- Passive mouse-move observation wakes the UI only over the overlay or on leaving
  it. These records do not enter the command queue or affect editor inference.
- The observer thread blocks on its run loop without a 250 ms timer. Shutdown
  queues a stop block and wakes that loop before joining it.
- Expensive permission/accessibility/context queries leave the 60 FPS path. A
  coalescible one-second timer checks health; received batches also recheck layout
  and Secure Input. Window notifications request redraws after exposure, resize,
  backing-scale or occlusion changes.

The ECS Registry, ICommand pipeline, C++20, raylib, font, layout and visual
transparency defaults remain. Native APIs stay in `platform/mac`; `input.cpp`
contains no Apple types or platform conditionals. CMake configuration is unchanged.

## Measurements on 2026-10-10

macOS 26.6.2, Apple M1 Pro, release build, resting native window at 748 × 243 logical
pixels / 1496 × 486 backing pixels. The old and new processes were compared over
20 seconds. A separate 15-second new-build run excluded startup with a three-second
warmup. The observer was unavailable in the isolated new executable's permission
context; a live permission-granted typing session still needs measurement.

| Process / condition | CPU, percent of one core | Interrupt wakeups / second |
| --- | ---: | ---: |
| Earlier running release, comparison | 30.952 | 54.686 |
| New release, comparison including startup | 3.903 | 13.597 |
| New release executable, warmed static window | 0.170 | 2.066 |
| Rebuilt release bundle, warmed static window | 0.192 | 1.067 |

These rows cover the resting window only. They do not cover AI tip generation (requests to
Ollama or DigitalOcean), which needs its own measurement if power use matters.

The earlier process also reported approximately 43% instantaneous CPU at inspection.
Sampling it showed continual draw/present/sleep work. The new warmed sample spent
most main-thread samples waiting in GLFW/AppKit; permission-health checks accounted
for most remaining sampled work. Measurements are CPU/wakeup counters, **not
Activity Monitor's Energy Impact score**. Apple describes that score as relative
energy consumption; it depends on actual workload. A value well under 1 must be
checked separately after restarting the rebuilt app and allowing startup to settle.
[Apple's Activity Monitor explanation](https://support.apple.com/guide/activity-monitor/view-energy-consumption-actmntr43697/mac).

To repeat a low-overhead measurement of your own running app, get its PID from
Activity Monitor or `pgrep -x KeyGuide`, then leave its window static:

```sh
python3 scripts/measure_idle.py PID --seconds 30
python3 scripts/measure_idle.py OLD_PID NEW_PID --seconds 30 --json
sample PID 3 -file /tmp/keyguide_sample.txt
```

`measure_idle.py` uses macOS `proc_pid_rusage`, converts Mach ticks with the actual
timebase (Apple Silicon is not necessarily 1:1), and reads only counters. It
requires no root privileges, screenshots, permission prompts or event injection.
The application has no Python runtime dependency.

Compare idle, ordinary typing, hover animation and hidden-window tracking
separately. Do not benchmark with `--frames` or `--screenshot`, which deliberately
force drawing. For deeper investigation use Instruments Time Profiler for CPU
work and System Trace for scheduling/wakeups; inspect GPU/presentation costs while
hovering. Keep the Release build and display/layout settings consistent.

## Checks performed

All four native and fallback CTest suites pass. Existing command-pipeline checks
now verify unchanged input, inside/outside pointer changes, settings/input-loss
updates and exact animation completion. Queue tests check the empty predicate.
A temporary actual-renderer fixture verifies no redraw while static, redraw after
transparency changes and hide/show, and retained Blender hint/hover frames.
A native fixture exercises the actual sleeping GLFW loop, a worker-thread raw
key observation, main-run-loop wake, unchanged event return, pointer-region
filtering and shutdown of a blocked observer loop. It creates event objects for
the callback but never posts OS keyboard or mouse input and creates no global tap.

## Suggested next maintenance work

1. Split the large Vim transition function into named mode/context handlers and
   split `command_pipeline_tests.cpp` into named scenario functions. Preserve
   the existing tracker interfaces and the ordered command pipeline.
2. Separate native keycode normalization from observer/run-loop lifecycle within
   `platform/mac`, so mapping changes can be reviewed independently.
3. Cache hint text layout by hint/chord/font/width. Measure before changing it:
   wrapping, measurement and string allocation now happen only on rendered frames,
   but can still cost work during hover and typing.
4. Split hint availability projection from hover geometry projection. Animation
   should reuse the latest hints instead of reevaluating every modifier combination.
5. Move shared runtime globals into a small explicit application context gradually;
   keep the Registry API compatible with other projects. This makes reset/shutdown
   and fixture setup easier to reason about.

If measurements still show significant presentation cost during continuous typing
or hover, an optional macOS AppKit/Core Animation renderer is a possible later
backend. It could retain the C++ tracker/ECS and keep raylib for other platforms.
That is a larger rendering change, and should follow profiling; idle redraw
elimination currently gives the largest reduction without replacing the stack.
