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
2. In the profiler, connect to **`127.0.0.1:8087`**.

Wheeler's client always uses port 8087. Other plugins with their own Tracy
client, such as Huginn, take the default port 8086. A plain `127.0.0.1` or a
`SkyrimSE.exe` entry on 8086 in the discovery list is the other plugin, not
Wheeler. Type `127.0.0.1`, not `localhost`: `localhost` can resolve to the IPv6
address `::1`, and Wheeler's client listens on IPv4 only. To profile both
plugins, open a second profiler window for the other port.

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
- **Per frame:** `Wheeler Present`, ImGui setup and render, `Wheeler::Update`
  and its `Wheel lock wait`, `Utils::Inventory::GetInventory`, `Wheel::Draw`,
  the per-entry background and slot drawing, `Drawer::draw_text` and
  `Drawer::draw_texture`, and the weapon/armour inventory lookup
  (`WheelItemMutable::GetItemExtraDataAndCount`).
- **Input:** `Input::ProcessAndFilter`.
- **Actions:** opening and closing the wheel, activating an entry and each item
  type's `ActivateItem*`, `EquipObject` for weapons and armour,
  `Utils::Slot::CleanSlot`, and reloading wheels from the save.
- **API:** every `API_*` entry point a client calls, with its wait for the
  wheel-data lock in `API lock wait (exclusive)` or `API lock wait (shared)`.
  Item construction for `AddItemByFormID` is in
  `WheelItemFactory::MakeWheelItemFromFormID`. Inside it, the item's base
  description text (`TESDescription::GetDescription`, which the game reads from
  the plugin file), the game-side magic description
  (`GetMagicItemDescription (ItemCard)`) and the custom icon lookup
  (`GetIconImage (form)`) have zones of their own. Also the dispatch of each
  notification (`NotifyItemActivated`, `NotifyEditModeChanged`,
  `NotifyWheelStateChanged`). The client's own code runs inside the
  `client callback` zone, so its time is kept apart from Wheeler's.

## Adding zones

Put `ZoneScoped;` at the top of a function, or `ZoneScopedN("name");` in a block.
The macros come from `src/bin/Utilities/Profiling.h`, which the PCH includes.
That header stubs only the macros Wheeler uses. Before using another Tracy
macro, add an empty stub for it there, or the normal build will not compile.
