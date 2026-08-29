# Wheeler API - Server Implementation Reference (v2)

This document describes what Wheeler (the server) must implement to support external clients.

## Overview

Wheeler exposes a C API via a single exported function `GetWheelerAPI()` that returns a struct of function pointers. Clients call this once on init and use the returned interface.

## What's New in v2

- **Extended WheelConfig** - Styling options for label and indicator
- **Entry Subtext** - Per-entry labels displayed below item names (also renders on empty entries)
- **WheelStateCallback signature change** - Now includes wheelIndex parameter

## Export

```cpp
extern "C" __declspec(dllexport) IWheelerAPI* GetWheelerAPI();
```

## Interface Struct

```cpp
struct IWheelerAPI
{
    uint32_t version;  // API_VERSION = 4, bump on breaking changes

    // Status
    bool (*IsInitialized)();
    bool (*IsInEditMode)();
    bool (*IsWheelOpen)();

    // Managed Wheel Lifecycle
    int32_t (*CreateManagedWheel)(const WheelConfig* config);
    Result (*DeleteManagedWheel)(int32_t wheelIndex);
    bool (*IsManagedWheel)(int32_t wheelIndex);

    // Wheel Queries
    int32_t (*GetWheelCount)();
    int32_t (*GetActiveWheelIndex)();
    Result (*SetActiveWheelIndex)(int32_t index);
    bool (*IsWheelEmpty)(int32_t wheelIndex);

    // Entry Management
    int32_t (*GetEntryCount)(int32_t wheelIndex);
    int32_t (*AddEntry)(int32_t wheelIndex);
    Result (*DeleteEntry)(int32_t wheelIndex, int32_t entryIndex);
    bool (*IsEntryEmpty)(int32_t wheelIndex, int32_t entryIndex);

    // Item Management
    int32_t (*GetItemCount)(int32_t wheelIndex, int32_t entryIndex);
    int32_t (*AddItemByFormID)(int32_t wheelIndex, int32_t entryIndex, uint32_t formID, uint16_t uniqueID);
    Result (*RemoveItem)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);
    Result (*ClearEntry)(int32_t wheelIndex, int32_t entryIndex);
    uint32_t (*GetItemFormID)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);
    int32_t (*GetSelectedItemIndex)(int32_t wheelIndex, int32_t entryIndex);
    Result (*SetSelectedItemIndex)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);

    // Callbacks
    void (*RegisterItemActivatedCallback)(ItemActivatedCallback callback);
    void (*RegisterEditModeCallback)(EditModeCallback callback);
    void (*RegisterWheelStateCallback)(WheelStateCallback callback);

    // Unregister Callbacks
    void (*UnregisterItemActivatedCallback)();
    void (*UnregisterEditModeCallback)();
    void (*UnregisterWheelStateCallback)();

    // --- v2: Entry Subtext ---
    Result (*SetManagedWheelEntrySubtext)(int32_t wheelIndex, int32_t entryIndex, const SubtextConfig* config);
};
```

## Configuration Structs

### WheelConfig

```cpp
struct WheelConfig
{
    int32_t numEntries;       // Number of empty entries to create
    int32_t position;         // Position in wheel list (-1 = append)
    bool managed;             // If true, wheel is not saved to user config
    const char* clientName;   // Name of the client managing this wheel (for display)
    bool showLabel;           // If true, show "[Managed By: clientName]" label

    // --- v2: Label Styling (0 = use defaults) ---
    float labelFontSize;       // Font size for label (default: 42)
    uint32_t labelColor;       // RGBA color for label (default: white)
    float labelOffsetY;        // Y offset below indicator (default: 50)

    // --- v2: Indicator Styling ---
    const char* indicatorText;        // Text on indicator (default: "M", nullptr = no indicator)
    uint32_t indicatorActiveColor;    // Color when active (default: cyan)
    uint32_t indicatorInactiveColor;  // Color when inactive (default: dim cyan)
};
```

### SubtextConfig (v2)

```cpp
struct SubtextConfig
{
    const char* text;    // The subtext to display (nullptr or "" to clear)
    float offsetX;       // X offset from entry center (default: 0)
    float offsetY;       // Y offset below item name (default: 20)
    float fontSize;      // Font size in pixels (default: 28, 0 = use default)
    uint32_t color;      // RGBA color (default: 0xB0FFFFFF = 70% white, 0 = use default)
};
```

## Implementation Requirements

### Thread Safety

All API functions must be thread-safe. Use `std::shared_mutex`:
- Read operations: `std::shared_lock`
- Write operations: `std::unique_lock`

### Silent Updates

**Critical:** All wheel/entry/item modifications via API must be silent - no UI notifications, sounds, or visual feedback. The client is updating wheels in the background and the user should not notice.

Implementation checklist:
- [x] No toast/notification on item add/remove
- [x] No sound effects
- [x] No animation triggers
- [x] No log spam (debug only)

### Managed Wheel Tracking

Managed wheels are owned by clients, not the user. Track both client name and styling.

Since v3 this metadata lives **on the `Wheel` itself** (`Wheel::GetManagedInfo()`), not in an index-keyed side table. A side table desyncs the moment the wheel list is reindexed by an insert or removal; storing it on the wheel means identity travels with the wheel:

```cpp
// Attached to each Wheel as std::optional<WheelManagedInfo>
struct WheelManagedInfo
{
    std::string clientName;
    bool showLabel;

    // Label styling
    float labelFontSize = 42.0f;
    uint32_t labelColor = C_SKYRIMWHITE;
    float labelOffsetY = 50.0f;

    // Indicator styling
    std::string indicatorText = "M";
    uint32_t indicatorActiveColor = IM_COL32(0, 255, 255, 255);    // Cyan
    uint32_t indicatorInactiveColor = IM_COL32(100, 200, 200, 180); // Dim cyan
};
```

Managed wheel properties:
- **Not persisted** to save files or user config
- **Not editable** by user in edit mode (skip in edit UI, or show as locked)
- **Not destroyed** by Wheeler's load-time reset (see below)
- **Display label** when `showLabel` is true and wheel is active

#### Lifecycle: never destroy a managed wheel implicitly (v4)

"Not persisted" and "destroyed on load" cannot both hold. Because managed wheels are excluded from the co-save, a reset that wipes them is **unrecoverable** — there is nothing left to deserialize them back from, and the owning client is left holding indices that no longer resolve.

`Wheeler::Clear()` therefore keeps managed wheels and destroys only the user's own, compacting the survivors to the front of `_wheels`. Two consequences for anyone touching this code:

- The saved active-wheel index is written relative to the unmanaged wheels alone (`SerializeIntoJsonObj` skips managed ones), so on restore it must be shifted past the surviving managed wheels to mean the same wheel again.
- Anything that rebuilds `_wheels` from index 0 after a clear (e.g. `SetupDefaultWheels()`) must start from `_wheels.size()`, not 0, or it will reach into a surviving client wheel.

Clear-then-repopulate must also happen under a **single** exclusive hold of the wheel-data lock (`Wheeler::ReloadFromJsonObj`). Releasing between the two leaves a window in which a client calling `CreateManagedWheel()` from another thread races the repopulate on the same vector.

### Entry Subtext Storage (v2)

Track subtext per-entry for managed wheels. Subtext is independent of item state — it persists across `ClearEntry()` calls and renders on empty entries:

```cpp
// Internal storage for subtext
struct SubtextData
{
    std::string text;
    float offsetX = 0.0f;
    float offsetY = 20.0f;
    float fontSize = 28.0f;
    uint32_t color = 0xB0FFFFFF;  // 70% white
};

// Map: wheelIndex -> (entryIndex -> SubtextData)
static std::unordered_map<int32_t, std::unordered_map<int32_t, SubtextData>> s_entrySubtext;
static std::shared_mutex s_subtextLock;

// Default values
constexpr float DEFAULT_SUBTEXT_OFFSET_X = 0.0f;
constexpr float DEFAULT_SUBTEXT_OFFSET_Y = 20.0f;
constexpr float DEFAULT_SUBTEXT_FONT_SIZE = 28.0f;
constexpr uint32_t DEFAULT_SUBTEXT_COLOR = 0xB0FFFFFF;  // 70% white
```

### Internal Helper Functions

These functions are used internally by Wheeler to query managed wheel state:

```cpp
namespace WheelerAPI
{
    // Set initialization state (called by Wheeler::Init)
    void SetInitialized(bool initialized);

    // Notification functions - called by Wheeler to notify registered callbacks
    void NotifyItemActivated(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex,
                             uint32_t formID, bool isPrimary);
    void NotifyEditModeChanged(bool entered, const WheelChange* changes, size_t changeCount);
    void NotifyWheelStateChanged(int32_t wheelIndex, bool isOpen);

    // Check if a wheel is managed (for serialization exclusion)
    bool IsManagedWheelIndex(int32_t wheelIndex);

    // Get the client name for a managed wheel (returns nullptr if not managed)
    const char* GetManagedWheelClientName(int32_t wheelIndex);

    // Get the client name as a safe copy (returns empty string if not managed)
    std::string GetManagedWheelClientNameSafe(int32_t wheelIndex);

    // Check if managed wheel label should be shown
    bool ShouldShowManagedWheelLabel(int32_t wheelIndex);

    // Get managed wheel styling (v2)
    struct ManagedWheelStyling
    {
        float labelFontSize;
        uint32_t labelColor;
        float labelOffsetY;
        std::string indicatorText;
        uint32_t indicatorActiveColor;
        uint32_t indicatorInactiveColor;
        bool isValid;
    };
    ManagedWheelStyling GetManagedWheelStyling(int32_t wheelIndex);

    // Get entry subtext for rendering (v2)
    struct EntrySubtextInfo
    {
        std::string text;
        float offsetX;
        float offsetY;
        float fontSize;
        uint32_t color;
        bool hasSubtext;
    };
    EntrySubtextInfo GetEntrySubtextInfo(int32_t wheelIndex, int32_t entryIndex);
}
```

### Managed Wheel Label Display

When a managed wheel with `showLabel = true` is active, Wheeler draws a label with styling:

```cpp
// In Wheeler::DrawWheel() after drawing the wheel indicator
auto styling = WheelerAPI::GetManagedWheelStyling(static_cast<int32_t>(_activeWheelIdx));
if (styling.isValid && WheelerAPI::ShouldShowManagedWheelLabel(_activeWheelIdx)) {
    std::string clientName = WheelerAPI::GetManagedWheelClientNameSafe(_activeWheelIdx);
    if (!clientName.empty()) {
        ImVec2 labelPos = { wheelCenter.x, wheelCenter.y + Config::Styling::Wheel::WheelIndicatorOffsetY + styling.labelOffsetY };
        std::string label = std::string("[Managed By: ") + clientName + "]";
        Drawer::draw_text(labelPos.x, labelPos.y, label.c_str(), styling.labelColor, styling.labelFontSize, drawArgs);
    }
}
```

### Entry Subtext Rendering (v2)

Subtext must render **regardless of whether the entry has items**. This allows managed wheels to display labels like "(No healing)" on empty classified slots.

In `WheelEntry::drawSlot()`, the item drawing is guarded by `!_items.empty()`, but subtext rendering runs unconditionally afterward:

```cpp
// Item drawing is conditional on having items
if (!_items.empty()) {
    // ... availability checks, bounds validation, DrawSlot() ...
}

// Subtext always renders — even on empty entries (e.g., "(No healing)")
if (a_wheelIndex >= 0 && a_entryIndex >= 0) {
    auto subtextInfo = WheelerAPI::GetEntrySubtextInfo(a_wheelIndex, a_entryIndex);
    if (subtextInfo.hasSubtext) {
        float subtextX = a_center.x + subtextInfo.offsetX;
        float subtextY = a_center.y + subtextInfo.offsetY;
        Drawer::draw_text(subtextX, subtextY, subtextInfo.text.c_str(),
                          subtextInfo.color, subtextInfo.fontSize, a_drawArgs);
    }
}
```

**Important:** Do not early-return from `drawSlot()` when `_items` is empty if subtext might be set. The subtext code must be reachable for empty entries on managed wheels.

### Edit Mode Behavior

When user enters edit mode:
1. Set internal flag `_editMode = true`
2. Call registered `EditModeCallback(true, nullptr, 0)`
3. Start tracking changes to non-managed wheels

When scrolling between wheels in edit mode:
- **Skip managed wheels** - user cannot edit them

```cpp
// In NextWheel/PrevWheel - skip managed wheels during edit mode
do {
    _activeWheelIdx = (_activeWheelIdx + 1) % _wheels.size();
    if (!_editMode || !WheelerAPI::IsManagedWheelIndex(_activeWheelIdx)) {
        break;
    }
} while (_activeWheelIdx != startIdx);
```

When user exits edit mode:
1. Build array of `WheelChange` structs for all changes
2. Call registered `EditModeCallback(false, changes, count)`
3. Set `_editMode = false`

During edit mode:
- API calls to managed wheels: **allowed** (client can still update)
- API calls to user wheels: return `Result::InEditMode`

### Deleting Managed Wheels (v3+)

`DeleteManagedWheel(wheelIndex)` refuses to remove Wheeler's last remaining wheel
and reports that honestly, as `Result::LastWheel`.

`DeleteManagedWheelsForClient(clientName)` must **not** carry that guard. It
returns a count, not a `Result`, so a refusal has nowhere to go: the original
implementation broke out of its erase loop once `_wheels.size() <= 1` and still
returned a plain count, which meant a client that owned every wheel present was
handed `0` while one of its wheels was still live — indistinguishable from
owning none. Clients treat that as "teardown succeeded" and recreate, ending up
with duplicates under one name.

The contract is therefore: **every match is erased, and the return equals the
number that matched.** A caller reading `N >= 0` can rely on no wheel for that
client surviving.

Wheeler's list must still not be left empty. The draw path handles it (it renders
the `NoWheelPresent` text) and most input paths guard on `_wheels.empty()`, but
`MoveEntryForwardInCurrentWheel` / `MoveEntryBackInCurrentWheel` — both bound to
edit-mode inputs in `Controls.cpp` — index `_wheels[_activeWheelIdx]` behind only
an `_activeWheelIdx != -1` test, and nothing in Wheeler ever sets that index to
`-1`. An empty vector is an out-of-bounds read there.

So when erasing the matches would empty the list, push one empty **unmanaged**
wheel (the same wheel `Wheeler::AddWheel` creates) and settle the active index
onto it:

```cpp
if (wheels.empty() && !toDelete.empty()) {
    wheels.push_back(std::make_unique<Wheel>());
    activeIdx = 0;
    activeWasErased = false;  // a freshly built wheel has no hover state to clear
}
```

The placeholder carries no managed info, so it is invisible to
`GetManagedWheelsForClient()` and is saved to the co-save like any user wheel.

### Index Adjustment

When wheels are inserted or removed, managed wheel indices and subtext must be adjusted:

```cpp
static void AdjustManagedIndicesAfterInsert(int32_t insertedAt)
{
    // Adjust managed wheel tracking
    {
        std::unique_lock lock(s_managedWheelLock);
        std::unordered_map<int32_t, ManagedWheelInfo> adjusted;
        for (auto& [idx, info] : s_managedWheelClients) {
            if (idx >= insertedAt) {
                adjusted[idx + 1] = std::move(info);
            } else {
                adjusted[idx] = std::move(info);
            }
        }
        s_managedWheelClients = std::move(adjusted);
    }

    // Adjust subtext tracking
    {
        std::unique_lock lock(s_subtextLock);
        std::unordered_map<int32_t, std::unordered_map<int32_t, SubtextData>> adjusted;
        for (auto& [idx, entries] : s_entrySubtext) {
            if (idx >= insertedAt) {
                adjusted[idx + 1] = std::move(entries);
            } else {
                adjusted[idx] = std::move(entries);
            }
        }
        s_entrySubtext = std::move(adjusted);
    }
}

static void AdjustManagedIndicesAfterRemove(int32_t removedAt)
{
    // Adjust managed wheel tracking
    {
        std::unique_lock lock(s_managedWheelLock);
        s_managedWheelClients.erase(removedAt);
        std::unordered_map<int32_t, ManagedWheelInfo> adjusted;
        for (auto& [idx, info] : s_managedWheelClients) {
            if (idx > removedAt) {
                adjusted[idx - 1] = std::move(info);
            } else {
                adjusted[idx] = std::move(info);
            }
        }
        s_managedWheelClients = std::move(adjusted);
    }

    // Adjust subtext tracking
    {
        std::unique_lock lock(s_subtextLock);
        s_entrySubtext.erase(removedAt);
        std::unordered_map<int32_t, std::unordered_map<int32_t, SubtextData>> adjusted;
        for (auto& [idx, entries] : s_entrySubtext) {
            if (idx > removedAt) {
                adjusted[idx - 1] = std::move(entries);
            } else {
                adjusted[idx] = std::move(entries);
            }
        }
        s_entrySubtext = std::move(adjusted);
    }
}
```

### CreateManagedWheel Implementation

```cpp
int32_t CreateManagedWheel(const WheelConfig* config)
{
    if (!config) return (int32_t)Result::InternalError;
    if (!s_initialized) return (int32_t)Result::NotInitialized;

    auto& wheeler = Wheeler::GetInstance();
    std::unique_lock lock(wheeler._wheelDataLock);

    auto& wheels = wheeler._wheels;

    // Create wheel with empty entries
    auto wheel = std::make_unique<Wheel>();
    for (int i = 0; i < config->numEntries; i++) {
        wheel->PushEmptyEntry();
    }

    // Insert at position
    int32_t index;
    if (config->position < 0 || config->position >= static_cast<int32_t>(wheels.size())) {
        index = static_cast<int32_t>(wheels.size());
        wheels.push_back(std::move(wheel));
    } else {
        index = config->position;
        wheels.insert(wheels.begin() + index, std::move(wheel));
        AdjustManagedIndicesAfterInsert(index);
    }

    // Track as managed with client name and styling settings
    if (config->managed) {
        std::unique_lock lock(s_managedWheelLock);
        ManagedWheelInfo info;
        info.clientName = config->clientName ? config->clientName : "Unknown";
        info.showLabel = config->showLabel;

        // Label styling (use defaults if 0)
        info.labelFontSize = (config->labelFontSize != 0.0f) ? config->labelFontSize : 42.0f;
        info.labelColor = (config->labelColor != 0) ? config->labelColor : C_SKYRIMWHITE;
        info.labelOffsetY = (config->labelOffsetY != 0.0f) ? config->labelOffsetY : 50.0f;

        // Indicator styling
        info.indicatorText = config->indicatorText ? config->indicatorText : "M";
        info.indicatorActiveColor = (config->indicatorActiveColor != 0) ?
            config->indicatorActiveColor : IM_COL32(0, 255, 255, 255);
        info.indicatorInactiveColor = (config->indicatorInactiveColor != 0) ?
            config->indicatorInactiveColor : IM_COL32(100, 200, 200, 180);

        s_managedWheelClients[index] = std::move(info);
    }

    DEBUG("WheelerAPI: Created managed wheel at index {} with {} entries (client: {}, showLabel: {})",
        index, config->numEntries, config->clientName ? config->clientName : "N/A", config->showLabel);

    return index;
}
```

### SetManagedWheelEntrySubtext Implementation (v2)

```cpp
Result SetManagedWheelEntrySubtext(int32_t wheelIndex, int32_t entryIndex, const SubtextConfig* config)
{
    if (!s_initialized) return Result::NotInitialized;

    // Must be a managed wheel
    if (!IsManagedWheelIndex(wheelIndex)) {
        return Result::NotManagedWheel;
    }

    // Validate entry index
    auto* wheel = Wheeler::GetWheelByIndex(wheelIndex);
    if (!wheel) return Result::InvalidWheelIndex;

    int32_t entryCount = static_cast<int32_t>(wheel->GetEntryCount());
    if (entryIndex < 0 || entryIndex >= entryCount) {
        return Result::InvalidEntryIndex;
    }

    std::unique_lock lock(s_subtextLock);

    if (config == nullptr || config->text == nullptr || config->text[0] == '\0') {
        // Clear subtext
        auto wheelIt = s_entrySubtext.find(wheelIndex);
        if (wheelIt != s_entrySubtext.end()) {
            wheelIt->second.erase(entryIndex);
            if (wheelIt->second.empty()) {
                s_entrySubtext.erase(wheelIt);
            }
        }
    } else {
        // Set subtext
        SubtextData data;
        data.text = config->text;
        data.offsetX = config->offsetX;
        data.offsetY = (config->offsetY != 0.0f) ? config->offsetY : DEFAULT_SUBTEXT_OFFSET_Y;
        data.fontSize = (config->fontSize != 0.0f) ? config->fontSize : DEFAULT_SUBTEXT_FONT_SIZE;
        data.color = (config->color != 0) ? config->color : DEFAULT_SUBTEXT_COLOR;

        s_entrySubtext[wheelIndex][entryIndex] = std::move(data);
    }

    return Result::OK;
}
```

### Callback Invocation

When user activates item:
```cpp
void OnItemActivated(int wheel, int entry, int item, bool primary)
{
    uint32_t formID = GetItemFormID(wheel, entry, item);

    // Log if this is a managed wheel
    std::string clientName = WheelerAPI::GetManagedWheelClientNameSafe(wheel);
    if (!clientName.empty()) {
        INFO("WheelerAPI: Item activated on managed wheel {} (client: {})", wheel, clientName);
    }

    WheelerAPI::NotifyItemActivated(wheel, entry, item, formID, primary);
}
```

When wheel opens/closes:
```cpp
void OnWheelStateChanged(int32_t wheelIndex, bool open)
{
    // Log managed wheel count on state change
    DEBUG("WheelerAPI: Wheel {} - {} managed wheel(s) registered",
        open ? "opened" : "closed", s_managedWheelClients.size());

    WheelerAPI::NotifyWheelStateChanged(wheelIndex, open);
}
```

## Result Codes

```cpp
enum class Result : int32_t
{
    OK = 0,
    InvalidWheelIndex = -1,
    InvalidEntryIndex = -2,
    InvalidItemIndex = -3,
    InvalidFormID = -4,
    FormNotFound = -5,
    UnsupportedFormType = -6,
    WheelNotEmpty = -7,
    LastWheel = -8,
    NotInitialized = -9,
    NotManagedWheel = -10,
    InEditMode = -11,
    EntryNotEmpty = -12,
    InternalError = -100
};
```

## Files Modified

1. `src/bin/API/WheelerAPI.h` - Public header with types, interface, and internal function declarations
2. `src/bin/API/WheelerAPI.cpp` - Implementation of all API functions, styling, and subtext tracking
3. `src/bin/Wheeler/Wheeler.cpp` - Hook callbacks, draw managed label with styling
4. `src/bin/Wheeler/WheelEntry.cpp` - Draw entry subtext (renders on empty entries too)
5. `src/bin/Wheeler/WheelItemFactory.cpp` - Create wheel items from FormID
6. `src/bin/Wheeler/WheelItemFactory.h` - Factory function declarations
7. Various WheelItem headers - Added `GetFormID()` virtual method overrides

## Callback Types

```cpp
// Called when user activates an item
using ItemActivatedCallback = void (*)(
    int32_t wheelIndex,
    int32_t entryIndex,
    int32_t itemIndex,
    uint32_t formID,
    bool isPrimary);

// Called when edit mode is entered/exited
// On exit: changes array contains all modifications made during edit session
using EditModeCallback = void (*)(
    bool entered,
    const WheelChange* changes,
    size_t changeCount);

// Called when wheel opens or closes
// wheelIndex: the active wheel when state changed
using WheelStateCallback = void (*)(int32_t wheelIndex, bool isOpen);
```
