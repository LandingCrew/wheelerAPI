#pragma once

// Wheeler External API
// This header provides an interface for external SKSE plugins to interact with Wheeler.
// All functions are thread-safe and can be called from any thread.
//
// Client usage:
//   #define WHEELER_API
//   #include "WheelerAPI.h"
//   auto api = GetWheelerAPI();
//   if (api && api->version >= WheelerAPI::API_VERSION) { ... }
//
// THREAD CONTEXT FOR CALLBACKS:
// All callbacks are invoked on Skyrim's main render/input thread during ImGui frame updates.
// This is the same thread that processes input events and renders the UI.
//
// - ItemActivatedCallback: Called when user activates an item via primary/secondary input
// - WheelStateCallback: Called when wheel opens (OpenWheeler) or closes (CloseWheeler)
// - EditModeCallback: Called when edit mode is entered/exited (inventory/magic menu open/close)
//
// IMPORTANT: Callbacks should execute quickly to avoid blocking the render loop.
// If you need to do heavy processing, queue work to another thread.

#include <cstddef>  // size_t
#include <cstdint>

#ifndef WHEELER_API
#   ifdef WHEELER_EXPORTS
#      define WHEELER_API __declspec(dllexport)
#   else
#      define WHEELER_API __declspec(dllimport)
#   endif
#endif

namespace WheelerAPI
{
   // API version - bump on breaking changes
   // v1: Initial API
   // v2: Added SetManagedWheelEntrySubtext()
   // v3: Added DeleteManagedWheelsForClient(); managed metadata moved onto the wheel
   //     (fixes stale-index desync on mid-session wheel insert/remove)
   // v4: Added GetManagedWheelsForClient(). Managed wheels now survive Wheeler's
   //     load-time reset instead of being destroyed with the user's own wheels.
   //     BEHAVIOUR CHANGE: Wheeler no longer implicitly drops a client's wheels on
   //     save load, so a client that recreates its wheels every load MUST call
   //     DeleteManagedWheelsForClient() first or it will accumulate duplicates.
   constexpr uint32_t API_VERSION = 4;

   // ============================================================================
   // Result Codes
   // ============================================================================

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

   // ============================================================================
   // Configuration Structs
   // ============================================================================

   struct WheelConfig
   {
      int32_t numEntries;  // Number of empty entries to create
      int32_t position;    // Position in wheel list (-1 = append)
      bool managed;        // If true, wheel is not saved to user config
      const char* clientName;  // Name of the client managing this wheel (for display)
      bool showLabel;      // If true, show clientName as label when viewing this wheel

      // --- Label Styling (optional, 0 = use defaults) ---
      float labelFontSize;    // Font size for clientName label (default: 42)
      uint32_t labelColor;    // RGBA color for label (default: white)
      float labelOffsetY;     // Y offset below wheel indicator (default: 50)

      // --- Indicator Styling (optional) ---
      const char* indicatorText;     // Text to show on wheel indicator (default: "M", nullptr/"" = no indicator)
      uint32_t indicatorActiveColor;   // Indicator color when wheel is active (default: cyan 0xFFFFFF00)
      uint32_t indicatorInactiveColor; // Indicator color when wheel is inactive (default: dim cyan 0xB4C8C864)
   };

   // Configuration for entry subtext (v2)
   struct SubtextConfig
   {
      const char* text;    // The subtext to display (nullptr or "" to clear)
      float offsetX;       // X offset from entry center (default: 0)
      float offsetY;       // Y offset below item name (default: 20)
      float fontSize;      // Font size in pixels (default: 28, 0 = use default)
      uint32_t color;      // RGBA color (default: 0xB0FFFFFF = 70% white, 0 = use default)
   };

   // ============================================================================
   // Change Tracking (for edit mode callbacks)
   // ============================================================================

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

   // ============================================================================
   // Callback Types
   // ============================================================================

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
   // wheelIndex is the currently active wheel index when the state changed
   using WheelStateCallback = void (*)(int32_t wheelIndex, bool isOpen);

   // ============================================================================
   // API Interface Struct
   // ============================================================================

   struct IWheelerAPI
   {
      uint32_t version;  // API_VERSION

      // --- Status ---
      bool (*IsInitialized)();
      bool (*IsInEditMode)();
      bool (*IsWheelOpen)();

      // --- Managed Wheel Lifecycle ---
      // Returns wheel index on success, negative Result on failure
      int32_t (*CreateManagedWheel)(const WheelConfig* config);
      Result (*DeleteManagedWheel)(int32_t wheelIndex);
      bool (*IsManagedWheel)(int32_t wheelIndex);

      // --- Wheel Queries ---
      int32_t (*GetWheelCount)();
      int32_t (*GetActiveWheelIndex)();
      Result (*SetActiveWheelIndex)(int32_t index);
      bool (*IsWheelEmpty)(int32_t wheelIndex);

      // --- Entry Management ---
      int32_t (*GetEntryCount)(int32_t wheelIndex);
      // Returns entry index on success, negative Result on failure
      int32_t (*AddEntry)(int32_t wheelIndex);
      Result (*DeleteEntry)(int32_t wheelIndex, int32_t entryIndex);
      bool (*IsEntryEmpty)(int32_t wheelIndex, int32_t entryIndex);

      // --- Item Management ---
      int32_t (*GetItemCount)(int32_t wheelIndex, int32_t entryIndex);
      // Returns item index on success, negative Result on failure
      int32_t (*AddItemByFormID)(int32_t wheelIndex, int32_t entryIndex, uint32_t formID, uint16_t uniqueID);
      Result (*RemoveItem)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);
      Result (*ClearEntry)(int32_t wheelIndex, int32_t entryIndex);
      uint32_t (*GetItemFormID)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);
      int32_t (*GetSelectedItemIndex)(int32_t wheelIndex, int32_t entryIndex);
      Result (*SetSelectedItemIndex)(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex);

      // --- Callbacks ---
      // Pass nullptr to unregister a previously registered callback
      void (*RegisterItemActivatedCallback)(ItemActivatedCallback callback);
      void (*RegisterEditModeCallback)(EditModeCallback callback);
      void (*RegisterWheelStateCallback)(WheelStateCallback callback);

      // --- Unregister Callbacks (convenience) ---
      void (*UnregisterItemActivatedCallback)();
      void (*UnregisterEditModeCallback)();
      void (*UnregisterWheelStateCallback)();

      // --- v2: Entry Subtext ---
      // Set subtext displayed below an entry's item name (managed wheels only)
      // @param wheelIndex The managed wheel index
      // @param entryIndex The entry index within the wheel
      // @param config Subtext configuration (text, position, size, color)
      // @return Result::OK on success
      Result (*SetManagedWheelEntrySubtext)(int32_t wheelIndex, int32_t entryIndex, const SubtextConfig* config);

      // --- v3: Batch delete by client ---
      // Delete ALL managed wheels owned by the given client in one shift-safe pass.
      // Prefer this over looping DeleteManagedWheel() with stored indices: each
      // single delete shifts the remaining indices, so a caller's stored indices go
      // stale mid-loop and wheels get orphaned. This removes them all at once.
      // @param clientName The client name passed in WheelConfig::clientName
      // @return number of wheels deleted (>= 0), or a negative Result on error
      int32_t (*DeleteManagedWheelsForClient)(const char* clientName);

      // --- v4: Batch lookup by client ---
      // Read counterpart to DeleteManagedWheelsForClient(): answers "which wheel
      // indices are mine?" from the one key that stays stable across reindexing.
      // Use it to re-derive indices after IsManagedWheel() reports a stored index
      // is no longer yours, instead of writing the wheel off for the session.
      //
      // Indices are written in ascending order and are valid only until the next
      // wheel insert or removal — read them and use them promptly.
      //
      // @param clientName The client name passed in WheelConfig::clientName
      // @param outIndices Buffer receiving up to maxCount indices; may be nullptr
      // @param maxCount Capacity of outIndices, in elements
      // @return TOTAL number of wheels managed for clientName (>= 0), which may
      //         exceed maxCount, or a negative Result on error. Pass nullptr/0 to
      //         query the count only; a return greater than maxCount means the
      //         buffer was too small and only the first maxCount were written.
      int32_t (*GetManagedWheelsForClient)(const char* clientName, int32_t* outIndices, size_t maxCount);
   };

   // ============================================================================
   // Internal Functions (Wheeler server only)
   // ============================================================================

   // Set initialization state (called by Wheeler::Init)
   void SetInitialized(bool initialized);

   // Notification functions - called by Wheeler to notify registered callbacks
   void NotifyItemActivated(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex, uint32_t formID, bool isPrimary);
   void NotifyEditModeChanged(bool entered, const WheelChange* changes, size_t changeCount);
   void NotifyWheelStateChanged(int32_t wheelIndex, bool isOpen);

   // Check if a wheel is managed (for serialization exclusion)
   bool IsManagedWheelIndex(int32_t wheelIndex);

   // Get the client name for a managed wheel (returns nullptr if not managed)
   // WARNING: Returned pointer is only valid momentarily - copy immediately if storing
   const char* GetManagedWheelClientName(int32_t wheelIndex);

   // Get the client name as a safe copy (returns empty string if not managed)
   // Use this when you need to store/log the name safely
   std::string GetManagedWheelClientNameSafe(int32_t wheelIndex);

   // Check if managed wheel label should be shown
   bool ShouldShowManagedWheelLabel(int32_t wheelIndex);

   // Get managed wheel styling info for rendering
   struct ManagedWheelStyling
   {
      // Label styling
      float labelFontSize;
      uint32_t labelColor;
      float labelOffsetY;

      // Indicator styling
      std::string indicatorText;
      uint32_t indicatorActiveColor;
      uint32_t indicatorInactiveColor;

      bool isValid;  // false if wheel is not managed
   };
   ManagedWheelStyling GetManagedWheelStyling(int32_t wheelIndex);

   // Internal storage struct for entry subtext (includes computed values)
   struct EntrySubtextInfo
   {
      std::string text;
      float offsetX;
      float offsetY;
      float fontSize;
      uint32_t color;
      bool hasSubtext;  // true if text is non-empty
   };

   // Get entry subtext info for rendering
   // Used internally by WheelEntry::drawSlot()
   EntrySubtextInfo GetEntrySubtextInfo(int32_t wheelIndex, int32_t entryIndex);

}  // namespace WheelerAPI

// ============================================================================
// Main Entry Point
// ============================================================================

// Returns pointer to static IWheelerAPI instance, or nullptr if not available
extern "C" WHEELER_API WheelerAPI::IWheelerAPI* GetWheelerAPI();
