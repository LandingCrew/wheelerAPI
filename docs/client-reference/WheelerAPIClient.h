// =============================================================================
// Wheeler API Client Reference Implementation (v2)
// =============================================================================
//
// This is a complete, copy-paste-ready client implementation for integrating
// with Wheeler's external API. Drop this file into your SKSE plugin project.
//
// Requirements:
// - Wheeler.dll with API v2 support
// - Windows.h for GetModuleHandle/GetProcAddress
//
// Usage:
//   1. Copy this file to your project
//   2. Call WheelerClient::TryConnect() after SKSE loads (kPostLoad or kDataLoaded)
//   3. Call WheelerClient::CreateWheel() after game load (kPostLoadGame)
//   4. Call WheelerClient::UpdateItems() whenever your recommendations change
//
// v2 Features:
//   - Custom indicator text (e.g., "O" instead of "M")
//   - Label styling (font size, color, offset)
//   - Per-entry subtext (e.g., "Wildcard", "Primary", "(No healing)" on empty slots)
//
// =============================================================================

#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace WheelerAPI
{
    // =========================================================================
    // API Version - Clients should check this matches or exceeds their needs
    // =========================================================================
    constexpr uint32_t API_VERSION = 4;

    // =========================================================================
    // Result Codes
    // =========================================================================
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

    // =========================================================================
    // Configuration for creating a managed wheel
    // =========================================================================
    struct WheelConfig
    {
        int32_t numEntries;       // Number of empty entry slots to create
        int32_t position;         // Position in wheel list (-1 = append to end)
        bool managed;             // If true, wheel is not saved to user config
        const char* clientName;   // Name displayed in "[Managed By: ...]" label
        bool showLabel;           // If true, shows the managed label when wheel is active

        // --- Label Styling (optional, 0 = use defaults) ---
        float labelFontSize;       // Font size for clientName label (default: 42)
        uint32_t labelColor;       // RGBA color for label (default: white)
        float labelOffsetY;        // Y offset below wheel indicator (default: 50)

        // --- Indicator Styling (optional) ---
        const char* indicatorText;        // Text on wheel indicator (default: "M", nullptr/"" = no indicator)
        uint32_t indicatorActiveColor;    // Color when wheel is active (default: cyan)
        uint32_t indicatorInactiveColor;  // Color when wheel is inactive (default: dim cyan)
    };

    // =========================================================================
    // Configuration for entry subtext (v2)
    // Renders on both populated and empty entries. Use on empty entries to
    // show labels like "(No healing)" for classified slots with no candidate.
    // =========================================================================
    struct SubtextConfig
    {
        const char* text;    // The subtext to display (nullptr or "" to clear)
        float offsetX;       // X offset from entry center (default: 0)
        float offsetY;       // Y offset below item name (default: 20; use 0 for empty entries)
        float fontSize;      // Font size in pixels (default: 28, 0 = use default)
        uint32_t color;      // RGBA color (default: 0xB0FFFFFF = 70% white, 0 = use default)
    };

    // =========================================================================
    // Change tracking for edit mode callbacks
    // =========================================================================
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

    // =========================================================================
    // Callback function types
    // =========================================================================

    // Called when user activates an item from the wheel
    // isPrimary: true for left-click/primary action, false for right-click/secondary
    using ItemActivatedCallback = void (*)(int32_t wheelIndex, int32_t entryIndex,
                                           int32_t itemIndex, uint32_t formID, bool isPrimary);

    // Called when user enters/exits edit mode
    // changes array is only valid during the callback - copy if you need to store
    using EditModeCallback = void (*)(bool entered, const WheelChange* changes, size_t changeCount);

    // Called when wheel UI opens or closes
    // wheelIndex: the active wheel when the state changed
    using WheelStateCallback = void (*)(int32_t wheelIndex, bool isOpen);

    // =========================================================================
    // The API Interface - function pointers provided by Wheeler.dll
    // =========================================================================
    struct IWheelerAPI
    {
        uint32_t version;

        // --- State Queries ---
        bool (*IsInitialized)();
        bool (*IsInEditMode)();
        bool (*IsWheelOpen)();

        // --- Managed Wheel Operations ---
        int32_t (*CreateManagedWheel)(const WheelConfig* config);
        Result (*DeleteManagedWheel)(int32_t wheelIndex);
        bool (*IsManagedWheel)(int32_t wheelIndex);

        // --- Wheel Queries ---
        int32_t (*GetWheelCount)();
        int32_t (*GetActiveWheelIndex)();
        Result (*SetActiveWheelIndex)(int32_t index);
        bool (*IsWheelEmpty)(int32_t wheelIndex);

        // --- Entry Operations ---
        int32_t (*GetEntryCount)(int32_t wheelIndex);
        int32_t (*AddEntry)(int32_t wheelIndex);
        Result (*DeleteEntry)(int32_t wheelIndex, int32_t entryIndex);
        bool (*IsEntryEmpty)(int32_t wheelIndex, int32_t entryIndex);

        // --- Item Operations ---
        int32_t (*GetItemCount)(int32_t wheelIndex, int32_t entryIndex);
        int32_t (*AddItemByFormID)(int32_t wheelIndex, int32_t entryIndex,
                                    uint32_t formID, uint16_t uniqueID);
        Result (*RemoveItem)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);
        Result (*ClearEntry)(int32_t wheelIndex, int32_t entryIndex);
        uint32_t (*GetItemFormID)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);
        int32_t (*GetSelectedItemIndex)(int32_t wheelIndex, int32_t entryIndex);
        Result (*SetSelectedItemIndex)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);

        // --- Callbacks ---
        void (*RegisterItemActivatedCallback)(ItemActivatedCallback callback);
        void (*RegisterEditModeCallback)(EditModeCallback callback);
        void (*RegisterWheelStateCallback)(WheelStateCallback callback);

        // --- Unregister Callbacks ---
        void (*UnregisterItemActivatedCallback)();
        void (*UnregisterEditModeCallback)();
        void (*UnregisterWheelStateCallback)();

        // --- v2: Entry Subtext ---
        // Set subtext displayed below an entry's item name (managed wheels only)
        Result (*SetManagedWheelEntrySubtext)(int32_t wheelIndex, int32_t entryIndex, const SubtextConfig* config);

        // --- v3: Batch delete by client ---
        // Delete ALL managed wheels owned by the given client in one shift-safe pass.
        // Prefer over looping DeleteManagedWheel() with stored indices (which go stale
        // as each delete shifts the rest). Only valid when version >= 3.
        //
        // POST-CONDITION: on a non-negative return, NO wheel for clientName remains.
        // Every match is deleted, including one that is the only wheel Wheeler has
        // left, and the value returned is exactly the number that matched — never a
        // partial tally. This call never leaves Wheeler's wheel list empty: if your
        // wheels were the only ones in it, an empty UNMANAGED wheel is left in their
        // place. That wheel is not yours and will not appear in
        // GetManagedWheelsForClient().
        //
        // Unlike DeleteManagedWheel(), this never returns Result::LastWheel.
        //
        // @return number of wheels deleted (>= 0), or a negative Result on error
        int32_t (*DeleteManagedWheelsForClient)(const char* clientName);

        // --- v4: Batch lookup by client ---
        // Read counterpart to DeleteManagedWheelsForClient(): answers "which wheel
        // indices are mine?". Use it to re-derive indices after IsManagedWheel()
        // reports a stored index is no longer yours. Only valid when version >= 4.
        // Indices are ascending and valid only until the next wheel insert/remove.
        // @return TOTAL number of wheels managed for clientName (may exceed
        //         maxCount, meaning the buffer was truncated), or a negative
        //         Result on error. Pass nullptr/0 to query the count only.
        int32_t (*GetManagedWheelsForClient)(const char* clientName, int32_t* outIndices, size_t maxCount);
    };

}  // namespace WheelerAPI


// =============================================================================
// Reference Client Implementation
// =============================================================================
//
// This is a ready-to-use singleton client. Customize as needed for your plugin.
//
// Key features:
// - Lazy connection to Wheeler API
// - Managed wheel creation with customizable config and styling
// - Optimized item updates (only changes slots when content differs)
// - Per-entry subtext support (v2)
// - Thread-safe singleton pattern
//
// =============================================================================

class WheelerClient
{
public:
    // =========================================================================
    // Singleton access
    // =========================================================================
    static WheelerClient& GetSingleton()
    {
        static WheelerClient instance;
        return instance;
    }

    // =========================================================================
    // Connection Management
    // =========================================================================

    /// Try to connect to Wheeler API
    /// @return true if connected successfully, false if Wheeler not available
    bool TryConnect()
    {
        if (m_api) {
            return true;  // Already connected
        }

        // Try to get Wheeler.dll handle
        HMODULE hWheeler = GetModuleHandleA("Wheeler.dll");
        if (!hWheeler) {
            // Wheeler.dll not loaded - this is normal if user doesn't have Wheeler
            return false;
        }

        // Get the API entry point
        using GetWheelerAPIFn = WheelerAPI::IWheelerAPI* (*)();
        auto GetWheelerAPI = reinterpret_cast<GetWheelerAPIFn>(
            GetProcAddress(hWheeler, "GetWheelerAPI"));

        if (!GetWheelerAPI) {
            // Old version of Wheeler without API support
            return false;
        }

        m_api = GetWheelerAPI();
        if (!m_api) {
            // GetWheelerAPI() returned nullptr
            return false;
        }

        // Version check - require v2 for full feature set
        if (m_api->version < WheelerAPI::API_VERSION) {
            // API version too old - still usable but v2 features won't work
            // You can choose to fail here or continue with reduced functionality
        }

        return true;
    }

    /// Check if connected to Wheeler API
    bool IsConnected() const { return m_api != nullptr; }

    /// Get API version (0 if not connected)
    uint32_t GetAPIVersion() const { return m_api ? m_api->version : 0; }

    /// Check if v2 features are available
    bool HasV2Features() const { return m_api && m_api->version >= 2; }

    /// Check if v3 features are available (DeleteManagedWheelsForClient)
    bool HasV3Features() const { return m_api && m_api->version >= 3; }

    /// Check if v4 features are available (GetManagedWheelsForClient)
    bool HasV4Features() const { return m_api && m_api->version >= 4; }

    /// Get raw API pointer (nullptr if not connected)
    WheelerAPI::IWheelerAPI* GetAPI() const { return m_api; }

    // =========================================================================
    // Wheel Management
    // =========================================================================

    /// Create a managed wheel with default styling
    /// @param clientName Name to display in "[Managed By: ...]" label
    /// @param numSlots Number of item slots (entries) to create
    /// @param showLabel Whether to show the managed label when viewing this wheel
    /// @return true if wheel created or already exists
    bool CreateWheel(const char* clientName, int32_t numSlots = 3, bool showLabel = true)
    {
        WheelerAPI::WheelConfig config = {
            .numEntries = numSlots,
            .position = -1,
            .managed = true,
            .clientName = clientName,
            .showLabel = showLabel,
            .labelFontSize = 0,
            .labelColor = 0,
            .labelOffsetY = 0,
            .indicatorText = nullptr,  // Use default "M"
            .indicatorActiveColor = 0,
            .indicatorInactiveColor = 0
        };
        return CreateWheelWithConfig(&config);
    }

    /// Create a managed wheel with custom styling (v2)
    /// @param config Full wheel configuration including styling
    /// @return true if wheel created or already exists
    bool CreateWheelWithConfig(const WheelerAPI::WheelConfig* config)
    {
        if (!m_api) {
            return false;
        }

        if (!m_api->IsInitialized()) {
            // Wheeler not ready yet - try again later
            return false;
        }

        // Already have a wheel?
        if (m_wheelIndex >= 0) {
            return true;
        }

        m_wheelIndex = m_api->CreateManagedWheel(config);
        if (m_wheelIndex < 0) {
            // Creation failed - error code in m_wheelIndex
            return false;
        }

        // Remember the label: it is the only key that survives reindexing, so it
        // is what RecoverWheelIndex() uses to find this wheel again later.
        m_clientName = config->clientName ? config->clientName : "";

        // Initialize slot tracking
        m_currentSlotFormIDs.resize(static_cast<size_t>(config->numEntries), 0);

        return true;
    }

    /// Check if we have a managed wheel
    bool HasWheel() const { return m_wheelIndex >= 0; }

    /// Re-derive our wheel index from our client name (v4).
    ///
    /// A stored index only stays correct until the next wheel insert or removal.
    /// When IsManagedWheel(m_wheelIndex) turns false, call this rather than
    /// assuming the wheel is gone: the label is stable, so if Wheeler still holds
    /// a wheel under it, this finds it again.
    ///
    /// @return true if an index was recovered; false if we own no wheel (in which
    ///         case m_wheelIndex is reset to -1 and the wheel must be recreated)
    bool RecoverWheelIndex()
    {
        if (!HasV4Features() || m_clientName.empty()) {
            return false;
        }

        int32_t index = -1;
        int32_t count = m_api->GetManagedWheelsForClient(m_clientName.c_str(), &index, 1);
        if (count <= 0) {
            // count < 0 is an error code; count == 0 means Wheeler holds nothing
            // under our label. Either way the stored index is not usable.
            m_wheelIndex = -1;
            return false;
        }

        m_wheelIndex = index;
        return true;
    }

    /// Verify our stored index still points at our wheel, recovering it if not.
    /// Cheap enough to call before a batch of updates.
    /// @return true if m_wheelIndex is usable afterwards
    bool EnsureWheelIndexValid()
    {
        if (!m_api) {
            return false;
        }
        if (m_wheelIndex >= 0 && m_api->IsManagedWheel(m_wheelIndex)) {
            return true;
        }
        return RecoverWheelIndex();
    }

    /// Get the wheel index
    int32_t GetWheelIndex() const { return m_wheelIndex; }

    /// Delete the managed wheel
    void DeleteWheel()
    {
        if (!m_api) {
            return;
        }

        // Prefer the label-keyed delete: it does not depend on m_wheelIndex still
        // being accurate, and it sweeps up any wheel we created under this name
        // but lost track of. Wheeler keeps managed wheels across a save load
        // (v4+), so failing to clean up here is what causes duplicates.
        if (HasV3Features() && !m_clientName.empty()) {
            m_api->DeleteManagedWheelsForClient(m_clientName.c_str());
        } else if (m_wheelIndex >= 0) {
            m_api->DeleteManagedWheel(m_wheelIndex);
        }

        m_wheelIndex = -1;
        m_clientName.clear();
        m_currentSlotFormIDs.clear();
    }

    // =========================================================================
    // Item Management
    // =========================================================================

    /// Update the wheel with new items
    /// Only modifies slots that have changed, minimizing overhead.
    /// @param formIDs Vector of FormIDs to display (up to slot count)
    void UpdateItems(const std::vector<uint32_t>& formIDs)
    {
        if (!m_api || m_wheelIndex < 0) {
            return;
        }

        int32_t entryCount = m_api->GetEntryCount(m_wheelIndex);
        if (entryCount <= 0) {
            return;
        }

        size_t maxSlots = static_cast<size_t>(entryCount);

        for (size_t i = 0; i < maxSlots; ++i) {
            uint32_t newFormID = (i < formIDs.size()) ? formIDs[i] : 0;
            uint32_t currentFormID = (i < m_currentSlotFormIDs.size()) ? m_currentSlotFormIDs[i] : 0;

            // Skip if unchanged
            if (newFormID == currentFormID) {
                continue;
            }

            int32_t entryIndex = static_cast<int32_t>(i);

            // Clear existing item in this entry
            m_api->ClearEntry(m_wheelIndex, entryIndex);

            // Add new item if we have one
            if (newFormID != 0) {
                m_api->AddItemByFormID(m_wheelIndex, entryIndex, newFormID, 0);
            }

            // Track the update
            if (i < m_currentSlotFormIDs.size()) {
                m_currentSlotFormIDs[i] = newFormID;
            }
        }
    }

    /// Clear all items from the wheel
    void ClearAllItems()
    {
        if (!m_api || m_wheelIndex < 0) {
            return;
        }

        int32_t entryCount = m_api->GetEntryCount(m_wheelIndex);
        for (int32_t i = 0; i < entryCount; ++i) {
            m_api->ClearEntry(m_wheelIndex, i);
        }

        // Reset tracking
        std::fill(m_currentSlotFormIDs.begin(), m_currentSlotFormIDs.end(), 0);
    }

    // =========================================================================
    // Entry Subtext (v2)
    // =========================================================================

    /// Set subtext on an entry (v2 feature)
    /// Works on both populated and empty entries.
    /// @param entryIndex The entry to set subtext on
    /// @param text The subtext to display (nullptr or "" to clear)
    void SetEntrySubtext(int32_t entryIndex, const char* text)
    {
        if (!m_api || m_wheelIndex < 0 || m_api->version < 2) {
            return;  // v2 required
        }

        WheelerAPI::SubtextConfig config = {
            .text = text,
            .offsetX = 0.0f,
            .offsetY = 20.0f,
            .fontSize = 0.0f,  // Use default
            .color = 0          // Use default
        };

        m_api->SetManagedWheelEntrySubtext(m_wheelIndex, entryIndex, &config);
    }

    /// Set subtext with full styling control (v2 feature)
    /// @param entryIndex The entry to set subtext on
    /// @param config Subtext configuration (pass nullptr to clear)
    void SetEntrySubtextWithConfig(int32_t entryIndex, const WheelerAPI::SubtextConfig* config)
    {
        if (!m_api || m_wheelIndex < 0 || m_api->version < 2) {
            return;
        }

        m_api->SetManagedWheelEntrySubtext(m_wheelIndex, entryIndex, config);
    }

    /// Clear subtext from an entry (v2 feature)
    void ClearEntrySubtext(int32_t entryIndex)
    {
        if (!m_api || m_wheelIndex < 0 || m_api->version < 2) {
            return;
        }

        m_api->SetManagedWheelEntrySubtext(m_wheelIndex, entryIndex, nullptr);
    }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Register callback for when user activates an item
    void RegisterItemActivatedCallback(WheelerAPI::ItemActivatedCallback callback)
    {
        if (m_api) {
            m_api->RegisterItemActivatedCallback(callback);
        }
    }

    /// Register callback for edit mode enter/exit
    void RegisterEditModeCallback(WheelerAPI::EditModeCallback callback)
    {
        if (m_api) {
            m_api->RegisterEditModeCallback(callback);
        }
    }

    /// Register callback for wheel open/close
    void RegisterWheelStateCallback(WheelerAPI::WheelStateCallback callback)
    {
        if (m_api) {
            m_api->RegisterWheelStateCallback(callback);
        }
    }

    /// Unregister all callbacks
    void UnregisterAllCallbacks()
    {
        if (m_api) {
            m_api->UnregisterItemActivatedCallback();
            m_api->UnregisterEditModeCallback();
            m_api->UnregisterWheelStateCallback();
        }
    }

    // =========================================================================
    // Cleanup
    // =========================================================================

    /// Full cleanup - call on plugin shutdown
    void Shutdown()
    {
        UnregisterAllCallbacks();
        DeleteWheel();
        m_api = nullptr;
    }

private:
    WheelerClient() = default;
    ~WheelerClient() = default;
    WheelerClient(const WheelerClient&) = delete;
    WheelerClient& operator=(const WheelerClient&) = delete;

    WheelerAPI::IWheelerAPI* m_api = nullptr;
    int32_t m_wheelIndex = -1;
    std::string m_clientName;  // stable key for RecoverWheelIndex()
    std::vector<uint32_t> m_currentSlotFormIDs;
};


// =============================================================================
// Usage Example
// =============================================================================
/*

// In your SKSE plugin message handler:
void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        // Good time to try connecting to Wheeler
        WheelerClient::GetSingleton().TryConnect();
        break;

    case SKSE::MessagingInterface::kPostLoadGame:
        // Game loaded - create our recommendation wheel
        {
            auto& client = WheelerClient::GetSingleton();
            if (!client.IsConnected()) {
                client.TryConnect();
            }
            if (client.IsConnected()) {
                // Option 1: Simple creation with default styling
                client.CreateWheel("My Plugin", 4, true);

                // Option 2: Custom styling (v2)
                WheelerAPI::WheelConfig config = {
                    .numEntries = 4,
                    .position = -1,
                    .managed = true,
                    .clientName = "My Plugin",
                    .showLabel = true,
                    .labelFontSize = 0,
                    .labelColor = 0,
                    .labelOffsetY = 0,
                    .indicatorText = "P",  // Custom indicator
                    .indicatorActiveColor = 0,
                    .indicatorInactiveColor = 0
                };
                client.CreateWheelWithConfig(&config);
            }
        }
        break;
    }
}

// In your update loop (10 Hz recommended):
void OnGameUpdate()
{
    static auto lastUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 100) {
        return;  // Throttle to 10 Hz
    }
    lastUpdate = now;

    auto& client = WheelerClient::GetSingleton();
    if (!client.HasWheel()) {
        return;
    }

    // Get your recommended items (spells, weapons, etc.)
    std::vector<uint32_t> recommendedFormIDs = GetMyRecommendations();
    std::vector<bool> isWildcard = GetWildcardFlags();

    // Update the wheel
    client.UpdateItems(recommendedFormIDs);

    // Update subtext (v2 feature) — works on both populated and empty entries
    if (client.HasV2Features()) {
        for (size_t i = 0; i < recommendedFormIDs.size(); ++i) {
            if (isWildcard[i]) {
                client.SetEntrySubtext(static_cast<int32_t>(i), "Wildcard");
            } else if (recommendedFormIDs[i] == 0) {
                // Empty slot — show what type of item is missing
                client.SetEntrySubtext(static_cast<int32_t>(i), "(No healing)");
            } else {
                client.ClearEntrySubtext(static_cast<int32_t>(i));
            }
        }
    }
}

// On plugin shutdown:
void OnPluginShutdown()
{
    WheelerClient::GetSingleton().Shutdown();
}

*/


// =============================================================================
// Known Limitations / Future Work
// =============================================================================
//
// 1. EditModeCallback currently passes nullptr for changes array
//    - The callback fires correctly on enter/exit, but doesn't track what
//      actually changed during the edit session
//    - Future versions may implement change tracking
//
// 2. Single wheel per client
//    - This reference implementation supports one wheel per client
//    - For multiple wheels, extend m_wheelIndex to a vector
//
// 3. Thread safety
//    - API calls are thread-safe on the Wheeler side
//    - This client implementation is not thread-safe - call from one thread
//    - Add mutex protection if you need multi-threaded access
//
// =============================================================================
