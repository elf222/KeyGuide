# Build and run

Commands for building, running, testing and packaging KeyGuide. For what
the overlay does and how to use it, see the [README](../README.md).

## Prerequisites

- CMake 3.25+, Ninja, and a C++20 compiler.
- Internet access on first configure to download raylib 6.0, nlohmann/json 3.12.0 and
  toml++ 3.4.0 (both header-only). CMake fetches all three.
- No separate raylib installation: CMake builds it statically.
- libcurl for AI tips; see [AI tips and libcurl](#ai-tips-and-libcurl).

### macOS

Install Apple's command-line tools, then CMake and Ninja through Homebrew:

```sh
xcode-select --install
brew install cmake ninja
```

macOS builds use the libcurl that ships with the SDK.

### Linux

Install libcurl development files, for example `sudo apt install libcurl4-openssl-dev`.
Without them CMake prints a warning and AI tips are disabled. Linux builds run in preview
mode because global observation has no backend yet.

### Windows x64

1. Install Visual Studio 2022 or Build Tools 2022.
2. Select **Desktop development with C++**, including MSVC and a Windows SDK.
3. Include **C++ CMake tools for Windows**, or install CMake and Ninja separately.
4. Open **Developer PowerShell for VS 2022** with an x64 environment.

With Visual Studio Community in its default location, you can load the same
environment into a normal PowerShell with:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" `
    -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
```

Run `cl` to check that the compiler is available. Keep this PowerShell open for
building or launching an editor.

Use Ninja with MSVC. The Visual Studio CMake generator does not produce the
compilation database used by clangd.

CMake downloads and builds curl 8.23.0 from source with Schannel for AI tips.
This Windows path is untested.

## Build and run from the terminal

Terminal commands are the main workflow. Run them from the repository root.

Use separate `build/debug` and `build/release` directories. These paths also match
the editor and breakpoint-debugging configurations.

### macOS Terminal

Release:

```sh
cmake -S . -B build/release -Wno-author --log-level=WARNING -G Ninja -DCMAKE_BUILD_TYPE=Release &&
cmake --build build/release &&
(cd build/release/KeyGuide && ./KeyGuide)

# or build, sign and install the app bundle, then open it

cmake --build build/release --target package-macos &&
open /Applications/KeyGuide.app
```

Debug:

```sh
cmake -S . -B build/debug -Wno-author --log-level=WARNING -G Ninja -DCMAKE_BUILD_TYPE=Debug &&
cmake --build build/debug &&
(cd build/debug/KeyGuide && ./KeyGuide)
```

The parentheses launch from the asset directory and leave your terminal at the
repository root when the app closes.

### Windows PowerShell

In **Developer PowerShell**, run each line after the previous one succeeds.

Release:

```powershell
cmake -S . -B build/release -Wno-dev --log-level=WARNING -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
cmake --build build/release --target run
```

Debug:

```powershell
cmake -S . -B build/debug -Wno-dev --log-level=WARNING -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
cmake --build build/debug --target run
```

- Works with Windows PowerShell 5.1; `&&` requires PowerShell 7+.
- The `run` target launches the executable from its asset directory.
- To launch directly: `Push-Location build/debug/KeyGuide`, then
  `.\KeyGuide.exe`, then `Pop-Location` after closing the app.

### Outputs and shortcuts

| Build | Output directory | Executable |
| --- | --- | --- |
| Debug | `build/debug/KeyGuide/` | `KeyGuide` (macOS), `KeyGuide.exe` (Windows) |
| Release | `build/release/KeyGuide/` | `KeyGuide` (macOS), `KeyGuide.exe` (Windows) |

- Each output directory includes `assets/`.
- Close the app to finish a run command.
- `cmake --build build/release --target package-macos` builds and signs
  `/Applications/KeyGuide.app` (bundle identifier `com.elf222.keyguide`); see
  [packaging and signing](#packaging-and-signing-macos). The normal executable path stays the same.
- raylib's macOS OpenGL deprecation warning is expected.

Optional presets perform the same configure/build steps:

```sh
cmake --preset debug
cmake --build --preset run-debug
```

Replace `debug` with `release` and `run-debug` with `run-release` for Release.
Run presets build changed sources before launching.

## Preview and command-line options

To try the keyboard without global permission:

```sh
build/debug/KeyGuide/KeyGuide --preview
```

Windows/Linux builds use preview automatically.

| Option | Effect |
| --- | --- |
| `--preview` | Ordinary local window instead of the global overlay; no Input Monitoring needed. |
| `--profile blender` / `--profile nvim` | Selects the preview app profile; Zed otherwise. Also available under **Preview Window**. |
| `--function-row`, `--numpad` | Show the optional keyboard sections at startup. |
| `--pointer X,Y` | Preview only: pins the preview pointer at window coordinates `X,Y`, for authored screenshots with `--frames` or `--screenshot`. |
| `--frames N` | Smoke run: exit after N frames. |
| `--screenshot path.png` | Save only KeyGuide's own rendered framebuffer on exit. |
| `--explain` with `vim`, `blender` or `aerospace` | Headless: generates every missing AI tip for that keymap with the saved provider and model, writes its study guide, prints progress and exits. Needs no window or permissions. |

Command-line flags override saved menu settings for one launch.

`--frames` and `--screenshot` keep continuous rendering for deterministic captures.

```sh
build/debug/KeyGuide/KeyGuide --preview --profile blender --function-row --numpad
```

## Packaging and signing (macOS)

`cmake --build build/release --target package-macos` assembles a fresh bundle (executable,
`Info.plist`, `KeyGuide.icns`, assets) in `build/release/package-macos`, signs and verifies it,
then replaces `/Applications/KeyGuide.app` (`packaging/mac/install_bundle.cmake`). The previous
copy is removed only after the new one is in place, so a signing or copy failure leaves it
untouched. Quit a running KeyGuide first and relaunch it afterwards. Two cache variables
control this:

| Variable | Default | Meaning |
| --- | --- | --- |
| `KEYGUIDE_APP_DIR` | `/Applications` | Folder the bundle is installed into, e.g. `-DKEYGUIDE_APP_DIR=/tmp/keyguide` for a throwaway copy. Must not contain `;`. |
| `KEYGUIDE_CODESIGN_IDENTITY` | detected | First Apple Development / Developer ID Application identity from `security find-identity -v -p codesigning`, stored as its SHA-1 in the build's cache only; `-` when none exists. |

A certificate gives a stable identity, so Input Monitoring stays granted across rebuilds.
Ad-hoc (`-`) signatures change with every rebuild, so permission refreshes can recur.
[Apple explains why ad-hoc signing causes privacy-permission problems](https://developer.apple.com/forums/thread/125438).
The first signing may show a Keychain prompt for `codesign`; choose **Always Allow**.
Bundle assets live in `Contents/Resources/assets`, because code signing rejects data in
`Contents/MacOS`. To check a build:

```sh
codesign --verify --strict --verbose=2 /Applications/KeyGuide.app
```

The icon is drawn in `packaging/icon/keyguide.svg` with the overlay's colours (a keycap and
an AI tip bubble). `packaging/icon/make_icons.sh` regenerates `packaging/mac/KeyGuide.icns`
and `packaging/windows/KeyGuide.ico` from it with macOS's own tools.

## AI tips and libcurl

AI tips use libcurl through the `KEYGUIDE_AI_NETWORK` CMake option, which is ON by default.
Set it to `OFF` to build without the HTTP client; AI tips are then unavailable.

- macOS: uses the libcurl in the SDK.
- Linux: install libcurl development files (`libcurl4-openssl-dev` on Debian/Ubuntu). If they
  are missing, CMake warns and AI tips are disabled.
- Windows: builds curl 8.23.0 from source with Schannel. Untested.
- Web and Android do not build the HTTP client.

Provider setup, the grounding rules and the study guide format are in
[docs/ai_providers.md](ai_providers.md).

## Tests

Tests are opt-in and do not run during normal app startup:

```sh
cmake --preset debug -DKEYGUIDE_BUILD_TESTS=ON
cmake --build build/debug --target KeyGuideTests
ctest --test-dir build/debug --output-on-failure
```

CTest runs six small executables; a failed assertion returns a nonzero exit code:

- **Tracker** (`KeyGuideTrackerTests`): feeds authored keys to the real pure tracker; checks modes,
  intermediate operators/arguments, counts, contexts, uncertainty, Escape recovery,
  bounded pending sequences and Space/status priority.
- **Blender tracker** (`KeyGuideBlenderTrackerTests`): feeds physical chords and mouse button events to the passive
  tracker. Checks Object/Edit transitions, transform constraints, numeric input without
  retaining values, tool stages, menus/text entry, Mac aliases, focus handling and
  safe menu completion, common default controls and explicit resync. Escape recovers
  interrupted tools, but cannot establish an unknown base mode or repair observation loss.
- **Input queue** (`KeyGuideInputQueueTests`): forces full/empty/wrap/overflow and runs a producer/consumer
  handoff to check FIFO order without recording any real input.
- **Command pipeline** (`KeyGuideCommandPipelineTests`): a stub implements Platform and feeds real input.cpp,
  CommandRegistry and Registry. Checks same-frame `dfa`/`ciw`, stale focus/loss
  events, modifier hints, entity persistence and widened geometry. Checks also cover
  held/released Control in Insert, incompatible combinations, Caps Lock, icon slot
  separation, and expanded-key camera bounds at normal/minimum sizes. It links raylib
  and the real tick logic to check fixed hitboxes after edge-key expansion, plus
  CPU-only font metrics to check highlighted-letter spacing, wrapping and truncation.
  Space follows row reflow and camera zoom while its original hover hitbox stays fixed.
  Blender checks cover profile switching, stale events, observation loss, physical
  matching with Caps Lock, modifier hints, and rebuilding optional keyboard sections
  without losing tracked state. It never opens a window or requests permission.
- **AeroSpace config** (`KeyGuideAeroSpaceConfigTests`): reads `main` and `service` binding
  modes from sample configs, counting valid bindings and skipping unsupported ones.
- **Guide** (`KeyGuideGuideTests`): AI tip grounding, the shortcut catalog and AeroSpace bindings.

For a native-free macOS build using the same fallback selected on other platforms:

```sh
cmake -S . -B build/portable -G Ninja -DCMAKE_BUILD_TYPE=Debug -DKEYGUIDE_NATIVE_MACOS=OFF
cmake --build build/portable
```

## Formatting

Project code follows [`.clang-format`](../.clang-format): four-space namespace
indentation, expanded functions/structs/enums and braced control flow. Existing
names, directory layout, Registry and command interfaces are retained.
The optional formatter runs only on project sources and tests:

```sh
python3 scripts/format_code.py
python3 scripts/format_code.py --check
```

It requires clang-format 16+ on PATH; `--clang-format /path/to/clang-format` also
works. CMake does not require Python or clang-format.

## IDEs and breakpoint debugging

Build Debug from the terminal first. IDE build/run tasks are optional alternatives.

| Editor | Open | Debugging |
| --- | --- | --- |
| Zed | Repository root | Shared CodeLLDB profiles in `.zed/debug.json`. |
| VS Code | `projects/VSCode/main.code-workspace` | CMake Tools with a configured C++ debugger. |
| Visual Studio 2022 | Repository folder | Built-in C++ debugger with the Debug CMake preset. |

### Zed

On Windows, run these commands from Developer PowerShell in the repository root:

```powershell
cmake --preset debug
zed .
```

The first command creates `build/debug/compile_commands.json`, which tells
Zed's C++ language server where to find raylib. Starting Zed from the same shell
also gives its Build and Run tasks access to MSVC.

To debug, open a source file, set a breakpoint, run **debugger: start** from the
command palette, then select **KeyGuide Debug (Windows x64)**. On macOS or Linux,
select **KeyGuide Debug (macOS / Linux)** instead.

- Debug profiles rebuild Debug before launching and set the asset working directory.
- Allow CodeLLDB setup and project trust when prompted.
- Optional: **task: spawn** offers Configure, Build, and Run for Debug/Release.
- Configure the corresponding build before using its Build, Run, or debug action.

Windows CodeLLDB supports x64, but MSVC PDB support has limitations.
Verify breakpoints, stepping, and variable inspection; use Visual Studio's debugger
if needed. See [CodeLLDB's Windows notes](https://github.com/vadimcn/codelldb/wiki/Windows).

### VS Code

1. Open the workspace and install the recommended CMake Tools and C/C++ extensions.
2. Select the `debug` configure preset and configure the project.
3. Select `KeyGuide` as the launch target and set a breakpoint.
4. Use **CMake: Debug** with a debugger supported on your platform.

Set the debugger working directory to `build/debug/KeyGuide` under the repository
so relative asset paths resolve. See [workspace instructions](../projects/VSCode/README.md).

### Visual Studio 2022

1. Use **Open a local folder** with the repository root.
2. Select the `debug` CMake configure preset.
3. Select `KeyGuide` as the startup target, set a breakpoint, and start debugging.
4. Set the debugger working directory beside the executable for asset loading.

The Visual Studio 2022 solution has been removed. Use the CMake folder workflow above.

### Code completion and local settings

- Root `.clangd` reads `build/debug/compile_commands.json`; no symlink is needed.
- Configuring Debug generates that file, even before compilation.
- clangd always uses Debug flags, including after a Release build.
- Zed can install clangd automatically if none is available on PATH.
- Keep generated databases and personal `CMakeUserPresets.json` out of Git.

If headers appear missing:

1. Configure Debug successfully and check its `compile_commands.json` exists.
2. Restart the language server.
3. On Windows, check MSVC/SDK installation and launch the editor from Developer PowerShell.

For existing checkouts, remove the old root `compile_commands.json` only after
confirming it is a symlink. Old `build-mac-*` directories can remain.

## Web

Install the Emscripten SDK, then run from Bash:

```sh
./packaging/web/build-web.sh
```

- Expected SDK location: `$HOME/emsdk`; edit `EMSDK_ENV` in the script if different.
- The script is not a native PowerShell script.
- Upload: `build-web/compiled-zip/KeyGuide-web.zip`.
- Serve over HTTP(S) or upload the ZIP to itch.io; do not open the HTML from disk.

## Build notes

- The executable, packages and app/menu name are **KeyGuide**. The macOS bundle identifier is
  `com.elf222.keyguide`.
- CMake is the only build system. The Visual Studio 2022 solution, the Makefiles and the
  Android makefile have been removed.
- CMake options use the `KEYGUIDE_` prefix:
  `KEYGUIDE_BUILD_TESTS`, `KEYGUIDE_NATIVE_MACOS` and `KEYGUIDE_AI_NETWORK`.
- Dependencies are fetched by CMake: raylib 6.0 (static), nlohmann/json 3.12.0 and toml++ 3.4.0
  (both header-only). libcurl is used for AI providers; see
  [AI tips and libcurl](#ai-tips-and-libcurl).

CMake presets keep generated objects and dependency files under `build/debug` and
`build/release`, outside `src`.

The Debug/Release/run presets, C++20, asset copying and the compilation database are
unchanged. CMake selects exactly one backend. Native macOS enables Objective-C for GLFW's `.m`
files and Objective-C++ for our `.mm` files, and links ApplicationServices and Carbon alongside
the existing frameworks. The bundled IBM Plex Mono font includes its SIL Open Font License.

## Verification status

See [validation notes](validation.md) for the current results. Native macOS
Debug/Release, packaging, fallback builds, and the portable test suites were checked. The
Windows fallback also cross-compiled with MinGW (not run). Linux, Web, Android, fresh-clone
IDE behavior, the Windows libcurl build, and live Zed/Ghostty input with Input Monitoring
enabled still need execution checks.
