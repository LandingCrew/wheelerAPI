# Wheeler API - Client Implementation Reference (v2)

This document describes how external plugins (like On Cue) integrate with Wheeler.

## Overview

Clients access Wheeler's functionality through a single exported function that returns an interface struct. This approach requires only one `GetProcAddress` call and provides type-safe access to all API functions.

## What's New in v2

- **Custom indicator text** - Display "O" instead of "M" on your managed wheel
- **Label styling** - Customize font size, color, and position of the managed wheel label
- **Entry subtext** - Show per-entry labels like "Wildcard" or "Primary" below item names

## Getting Started

### 1. Include the Header

Copy `WheelerAPI.h` to your project and define `WHEELER_API` as empty before including:

```cpp
#define WHEELER_API
#include "WheelerAPI.h"
```

### 2. Get the API Interface

```cpp
#include <Windows.h>

WheelerAPI::IWheelerAPI* g_wheelerAPI = nullptr;

bool InitWheelerAPI()
{
    HMODULE hWheeler = GetModuleHandleA("Wheeler.dll");
    if (!hWheeler) {
        // Wheeler not loaded
        return false;
    }

    using GetAPIFunc = WheelerAPI::IWheelerAPI* (*)();
    auto GetWheelerAPI = (GetAPIFunc)GetProcAddress(hWheeler, "GetWheelerAPI");
    if (!GetWheelerAPI) {
        // Old version of Wheeler without API support
        return false;
    }

    g_wheelerAPI = GetWheelerAPI();
    if (!g_wheelerAPI || g_wheelerAPI->version < WheelerAPI::API_VERSION) {
        // API version mismatch
        return false;
    }

    return true;
}
```

### 3. Create a Managed Wheel

```cpp
int32_t g_myWheelIndex = -1;

bool SetupMyWheel()
{
    if (!g_wheelerAPI || !g_wheelerAPI->IsInitialized()) {
        return false;
    }

    WheelerAPI::WheelConfig config = {
        .numEntries = 3,           // Start with 3 empty slots
        .position = -1,            // Append to end (or specify index)
        .managed = true,           // Don't save to user config
        .clientName = "My Plugin", // Displayed in "[Managed By: ...]" label
        .showLabel = true,         // Show the managed label when viewing this wheel
        // v2 styling options (0 = use defaults)
        .labelFontSize = 0,
        .labelColor = 0,
        .labelOffsetY = 0,
        .indicatorText = "P",      // Custom indicator instead of "M"
        .indicatorActiveColor = 0,
        .indicatorInactiveColor = 0
    };

    g_myWheelIndex = g_wheelerAPI->CreateManagedWheel(&config);
    if (g_myWheelIndex < 0) {
        // Creation failed, g_myWheelIndex contains error code
        return false;
    }

    return true;
}
```

## WheelConfig Options

| Field | Type | Description |
|-------|------|-------------|
| `numEntries` | `int32_t` | Number of empty entry slots to create |
| `position` | `int32_t` | Where to insert wheel (-1 = append to end) |
| `managed` | `bool` | If true, wheel is not saved to user config |
| `clientName` | `const char*` | Name displayed in the managed wheel label |
| `showLabel` | `bool` | If true, shows "[Managed By: clientName]" when wheel is active |
| `labelFontSize` | `float` | Font size for label (default: 42, 0 = use default) |
| `labelColor` | `uint32_t` | RGBA color for label (default: white, 0 = use default) |
| `labelOffsetY` | `float` | Y offset below wheel indicator (default: 50, 0 = use default) |
| `indicatorText` | `const char*` | Text on wheel indicator (default: "M", nullptr = no indicator) |
| `indicatorActiveColor` | `uint32_t` | Indicator color when active (default: cyan, 0 = use default) |
| `indicatorInactiveColor` | `uint32_t` | Indicator color when inactive (default: dim cyan, 0 = use default) |

## Entry Subtext (v2)

v2 adds the ability to display custom text below item names. This is useful for showing status like "Wildcard", "Primary", or cooldown timers.

### SubtextConfig Options

| Field | Type | Description |
|-------|------|-------------|
| `text` | `const char*` | The subtext to display (nullptr or "" to clear) |
| `offsetX` | `float` | X offset from entry center (default: 0) |
| `offsetY` | `float` | Y offset below item name (default: 20) |
| `fontSize` | `float` | Font size in pixels (default: 28, 0 = use default) |
| `color` | `uint32_t` | RGBA color (default: 0xB0FFFFFF = 70% white, 0 = use default) |

### Setting Subtext

```cpp
void SetWildcardLabel(int32_t entryIndex)
{
    if (!g_wheelerAPI || g_wheelerAPI->version < 2) return;

    WheelerAPI::SubtextConfig config = {
        .text = "Wildcard",
        .offsetX = 0.0f,
        .offsetY = 20.0f,
        .fontSize = 0.0f,  // Use default
        .color = 0          // Use default
    };

    g_wheelerAPI->SetManagedWheelEntrySubtext(g_myWheelIndex, entryIndex, &config);
}

void ClearSubtext(int32_t entryIndex)
{
    if (!g_wheelerAPI || g_wheelerAPI->version < 2) return;

    // Pass nullptr to clear
    g_wheelerAPI->SetManagedWheelEntrySubtext(g_myWheelIndex, entryIndex, nullptr);
}
```

## Silent Updates

The API is designed for background updates. All modifications happen silently without user notification.

### Update Pattern (Optimized)

Only update slots when the content actually changes:

```cpp
std::vector<RE::FormID> m_currentSlotFormIDs;

void UpdateRecommendations(const std::vector<uint32_t>& recommendedFormIDs)
{
    if (!g_wheelerAPI || g_myWheelIndex < 0) return;

    int32_t entryCount = g_wheelerAPI->GetEntryCount(g_myWheelIndex);

    for (int32_t i = 0; i < entryCount && i < 3; i++) {
        uint32_t newFormID = (i < recommendedFormIDs.size()) ? recommendedFormIDs[i] : 0;
        uint32_t currentFormID = (i < m_currentSlotFormIDs.size()) ? m_currentSlotFormIDs[i] : 0;

        // Skip if unchanged
        if (newFormID == currentFormID) {
            continue;
        }

        // Clear existing item in this entry
        g_wheelerAPI->ClearEntry(g_myWheelIndex, i);

        // Add new item if we have one
        if (newFormID != 0) {
            g_wheelerAPI->AddItemByFormID(g_myWheelIndex, i, newFormID, 0);
        }

        // Track the update
        if (i < m_currentSlotFormIDs.size()) {
            m_currentSlotFormIDs[i] = newFormID;
        }
    }
}
```

### Throttling

Don't update every frame. Recommended update rates:
- Context evaluation: 10 Hz (100ms)
- Full wheel update: Only when recommendations change

```cpp
void OnGameUpdate()
{
    static float timeSinceLastUpdate = 0.0f;
    timeSinceLastUpdate += deltaTime;

    if (timeSinceLastUpdate < 0.1f) return;  // 10 Hz max
    timeSinceLastUpdate = 0.0f;

    auto newRecommendations = EvaluateContext();
    if (newRecommendations != currentRecommendations) {
        UpdateRecommendations(newRecommendations);
        currentRecommendations = newRecommendations;
    }
}
```

## Callbacks

### Item Activation (for learning)

```cpp
void OnItemActivated(
    int32_t wheelIndex,
    int32_t entryIndex,
    int32_t itemIndex,
    uint32_t formID,
    bool isPrimary)
{
    if (wheelIndex != g_myWheelIndex) return;  // Not my wheel

    // Positive reinforcement for Q-learning
    RecordPositiveReward(formID, GetCurrentState());
}

// Register during init
g_wheelerAPI->RegisterItemActivatedCallback(OnItemActivated);
```

### Edit Mode (for conflict resolution)

```cpp
void OnEditMode(
    bool entered,
    const WheelerAPI::WheelChange* changes,
    size_t changeCount)
{
    if (entered) {
        // User entered edit mode
        // Optionally pause updates, or continue (managed wheels still work)
        return;
    }

    // User exited edit mode - process changes
    for (size_t i = 0; i < changeCount; i++) {
        const auto& change = changes[i];

        // Skip changes to my managed wheel (shouldn't happen)
        if (change.wheelIndex == g_myWheelIndex) continue;

        switch (change.type) {
        case WheelerAPI::ChangeType::ItemAdded:
            // User manually added item - maybe learn from this?
            OnUserAddedItem(change.formID);
            break;

        case WheelerAPI::ChangeType::ItemRemoved:
            // User manually removed item
            OnUserRemovedItem(change.formID);
            break;

        // Handle other change types...
        }
    }
}

// Register during init
g_wheelerAPI->RegisterEditModeCallback(OnEditMode);
```

### Wheel State

```cpp
void OnWheelStateChanged(int32_t wheelIndex, bool isOpen)
{
    if (isOpen) {
        // Wheel just opened - good time to ensure recommendations are fresh
        ForceUpdateRecommendations();
    }
}

g_wheelerAPI->RegisterWheelStateCallback(OnWheelStateChanged);
```

## Dynamic Entry Management

If your client needs variable number of slots:

```cpp
void EnsureEntryCount(int32_t needed)
{
    int32_t current = g_wheelerAPI->GetEntryCount(g_myWheelIndex);

    // Add entries if needed
    while (current < needed) {
        g_wheelerAPI->AddEntry(g_myWheelIndex);
        current++;
    }

    // Remove entries if too many (must be empty first)
    while (current > needed) {
        int32_t lastEntry = current - 1;
        if (!g_wheelerAPI->IsEntryEmpty(g_myWheelIndex, lastEntry)) {
            g_wheelerAPI->ClearEntry(g_myWheelIndex, lastEntry);
        }
        g_wheelerAPI->DeleteEntry(g_myWheelIndex, lastEntry);
        current--;
    }
}
```

## Error Handling

```cpp
void SafeAddItem(int32_t entry, uint32_t formID)
{
    int32_t result = g_wheelerAPI->AddItemByFormID(
        g_myWheelIndex, entry, formID, 0);

    if (result < 0) {
        auto error = static_cast<WheelerAPI::Result>(result);
        switch (error) {
        case WheelerAPI::Result::InvalidWheelIndex:
            // Wheel was deleted? Recreate it
            SetupMyWheel();
            break;
        case WheelerAPI::Result::FormNotFound:
            // Item doesn't exist in game
            LogWarning("Form {:08X} not found", formID);
            break;
        case WheelerAPI::Result::InEditMode:
            // Queue for later (shouldn't happen for managed wheels)
            break;
        default:
            LogError("AddItem failed: {}", (int)error);
        }
    }
}
```

## Cleanup

```cpp
void Shutdown()
{
    if (g_wheelerAPI && g_myWheelIndex >= 0) {
        g_wheelerAPI->DeleteManagedWheel(g_myWheelIndex);
        g_myWheelIndex = -1;
    }

    // Unregister callbacks
    if (g_wheelerAPI) {
        g_wheelerAPI->UnregisterItemActivatedCallback();
        g_wheelerAPI->UnregisterEditModeCallback();
        g_wheelerAPI->UnregisterWheelStateCallback();
    }

    g_wheelerAPI = nullptr;
}
```

## Complete Example: On Cue Integration

This is the actual implementation pattern used by On Cue:

```cpp
namespace OnCue::Wheeler
{
    class WheelerClient
    {
    public:
        static WheelerClient& GetSingleton()
        {
            static WheelerClient instance;
            return instance;
        }

        bool TryConnect()
        {
            if (m_api) return true;

            HMODULE hWheeler = GetModuleHandleA("Wheeler.dll");
            if (!hWheeler) return false;

            using GetWheelerAPIFn = WheelerAPI::IWheelerAPI* (*)();
            auto GetWheelerAPI = reinterpret_cast<GetWheelerAPIFn>(
                GetProcAddress(hWheeler, "GetWheelerAPI"));
            if (!GetWheelerAPI) return false;

            m_api = GetWheelerAPI();
            if (!m_api || m_api->version < WheelerAPI::API_VERSION) {
                m_api = nullptr;
                return false;
            }

            return true;
        }

        bool CreateRecommendationWheel()
        {
            if (!m_api || !m_api->IsInitialized()) return false;
            if (m_wheelIndex >= 0) return true;  // Already exists

            WheelerAPI::WheelConfig config = {
                .numEntries = 3,
                .position = -1,
                .managed = true,
                .clientName = "On Cue",
                .showLabel = true,
                // v2 styling
                .labelFontSize = 0,
                .labelColor = 0,
                .labelOffsetY = 0,
                .indicatorText = "O",  // "O" for OnCue
                .indicatorActiveColor = 0,
                .indicatorInactiveColor = 0
            };

            m_wheelIndex = m_api->CreateManagedWheel(&config);
            if (m_wheelIndex < 0) return false;

            m_currentSlotFormIDs.resize(3, 0);
            m_currentSlotWildcard.resize(3, false);
            return true;
        }

        void UpdateRecommendations(const std::vector<RE::FormID>& spellFormIDs,
                                   const std::vector<bool>& isWildcard)
        {
            if (!m_api || m_wheelIndex < 0) return;

            for (size_t i = 0; i < 3; ++i) {
                RE::FormID newFormID = (i < spellFormIDs.size()) ? spellFormIDs[i] : 0;
                RE::FormID currentFormID = (i < m_currentSlotFormIDs.size()) ? m_currentSlotFormIDs[i] : 0;
                bool newWildcard = (i < isWildcard.size()) ? isWildcard[i] : false;
                bool currentWildcard = m_currentSlotWildcard[i];

                // Update item if changed
                if (newFormID != currentFormID) {
                    m_api->ClearEntry(m_wheelIndex, static_cast<int32_t>(i));
                    if (newFormID != 0) {
                        m_api->AddItemByFormID(m_wheelIndex, static_cast<int32_t>(i), newFormID, 0);
                    }
                    m_currentSlotFormIDs[i] = newFormID;
                }

                // Update subtext if changed (v2)
                if (m_api->version >= 2 && (newWildcard != currentWildcard || newFormID != currentFormID)) {
                    if (newWildcard && newFormID != 0) {
                        WheelerAPI::SubtextConfig subtext = {
                            .text = "Wildcard",
                            .offsetX = 0, .offsetY = 20, .fontSize = 0, .color = 0
                        };
                        m_api->SetManagedWheelEntrySubtext(m_wheelIndex, static_cast<int32_t>(i), &subtext);
                    } else {
                        m_api->SetManagedWheelEntrySubtext(m_wheelIndex, static_cast<int32_t>(i), nullptr);
                    }
                    m_currentSlotWildcard[i] = newWildcard;
                }
            }
        }

        bool HasRecommendationWheel() const { return m_wheelIndex >= 0; }

    private:
        WheelerAPI::IWheelerAPI* m_api = nullptr;
        int32_t m_wheelIndex = -1;
        std::vector<RE::FormID> m_currentSlotFormIDs;
        std::vector<bool> m_currentSlotWildcard;
    };
}
```

## Types Reference

```cpp
namespace WheelerAPI
{
    constexpr uint32_t API_VERSION = 2;

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

    struct WheelConfig
    {
        int32_t numEntries;
        int32_t position;
        bool managed;
        const char* clientName;
        bool showLabel;
        // v2 styling
        float labelFontSize;
        uint32_t labelColor;
        float labelOffsetY;
        const char* indicatorText;
        uint32_t indicatorActiveColor;
        uint32_t indicatorInactiveColor;
    };

    struct SubtextConfig  // v2
    {
        const char* text;
        float offsetX;
        float offsetY;
        float fontSize;
        uint32_t color;
    };

    enum class ChangeType : int32_t
    {
        ItemAdded,
        ItemRemoved,
        EntryAdded,
        EntryRemoved,
        ItemMoved
    };

    struct WheelChange
    {
        ChangeType type;
        int32_t wheelIndex;
        int32_t entryIndex;
        int32_t itemIndex;
        uint32_t formID;
    };

    // Callback types
    using ItemActivatedCallback = void (*)(int32_t wheelIndex, int32_t entryIndex,
                                           int32_t itemIndex, uint32_t formID, bool isPrimary);
    using EditModeCallback = void (*)(bool entered, const WheelChange* changes, size_t changeCount);
    using WheelStateCallback = void (*)(int32_t wheelIndex, bool isOpen);
}
```
