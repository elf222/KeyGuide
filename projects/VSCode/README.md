# VS Code

Open `main.code-workspace`; it opens the repository root. Install the recommended
CMake Tools and C/C++ extensions and the platform prerequisites in
[docs/build_and_run.md](../../docs/build_and_run.md#prerequisites).
On Windows, launch VS Code from Developer PowerShell so MSVC and the SDK are available.

1. Run **CMake: Select Configure Preset** and select `debug` or `release`.
2. Run **CMake: Configure** and **CMake: Build**.
3. To run with the correct asset working directory, use the integrated terminal:
   `cmake --build --preset run-debug` (or `run-release`).

CMake Tools supplies compiler information to the C/C++ extension. The shared
presets fetch and statically build raylib; no separate raylib checkout is needed.
The old Makefile-based workflow is no longer the recommended editor setup.
