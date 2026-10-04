# Profiling Wheeler with Tracy

Wheeler can be built with [Tracy](https://github.com/wolfpld/tracy) instrumentation.
It is off by default: a normal build compiles the zone macros to nothing and does
not need the Tracy sources.

## Build

```
git submodule update --init extern/tracy
cmake --preset default -B build-tracy -DWHEELER_TRACY=ON -DCOPY_OUTPUT=OFF
cmake --build build-tracy --config Release
```

The plugin is `build-tracy/src/Release/wheeler.dll`. `COPY_OUTPUT=OFF` keeps it
from replacing the normal build in `CompiledPluginsPath`; copy it there yourself
when you want to profile, and rebuild the normal plugin to go back.

## Connect

Use the Tracy profiler from the **same release as the submodule** (currently
v0.14.1). The network protocol changes between releases, and a mismatched
profiler refuses to connect.

1. Start the game with the Tracy build installed.
2. Start the profiler. The game shows up in its discovery list on this machine,
   or connect to `127.0.0.1`.

The client runs **on demand**: nothing is collected until the profiler connects,
so the build can be left installed without buffering every frame from launch.
Anything that happened before connecting, such as loading the save, is not
captured.

The build sets a few other Tracy options in `src/CMakeLists.txt`:

- `TRACY_ONLY_LOCALHOST`: the client listens on loopback only.
- `TRACY_NO_CRASH_HANDLER`: crashes are left to the game's crash logger.

Run the game as administrator if you also want Tracy's call-stack sampling and
context-switch capture. Zones work without it.

## What is instrumented

- **Frames:** one `FrameMark` per call of the D3D present hook.
- **Per frame:** `Wheeler Present`, ImGui setup and render, `Wheeler::Update`,
  `Utils::Inventory::GetInventory`, `Wheel::Draw`, and the per-entry background
  and slot drawing.
- **Input:** `Input::ProcessAndFilter`.
- **Actions:** opening and closing the wheel, activating an entry, and reloading
  wheels from the save.
- **API:** each callback dispatched to client plugins (`NotifyItemActivated`,
  `NotifyEditModeChanged`, `NotifyWheelStateChanged`). A slow callback shows up
  here, not as Wheeler's own time.

## Adding zones

Put `ZoneScoped;` at the top of a function, or `ZoneScopedN("name");` in a block.
The macros come from `src/bin/Utilities/Profiling.h`, which the PCH includes.
That header stubs only the macros Wheeler uses. Before using another Tracy
macro, add an empty stub for it there, or the normal build will not compile.
