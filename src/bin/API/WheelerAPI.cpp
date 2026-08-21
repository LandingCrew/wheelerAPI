#include "PCH.h"
#include "WheelerAPI.h"

#include <atomic>

#include "bin/Wheeler/Wheeler.h"
#include "bin/Wheeler/Wheel.h"
#include "bin/Wheeler/WheelEntry.h"
#include "bin/Wheeler/WheelItems/WheelItem.h"
#include "bin/Wheeler/WheelItems/WheelItemFactory.h"

namespace WheelerAPI
{
   // ============================================================================
   // Static State
   // ============================================================================

   static std::atomic<bool> s_initialized{ false };

   // Managed-wheel metadata now lives ON each Wheel (Wheel::GetManagedInfo), so it
   // can never desync from the wheel's position when the list is reindexed. This
   // counter is a cheap best-effort tally for diagnostics only.
   static std::atomic<int> s_managedWheelCount{ 0 };

   // Read a wheel's managed info by list index. Lock-free: the caller must hold the
   // Wheeler wheel-data lock (same convention as Wheeler::GetWheelByIndex).
   static const WheelManagedInfo* GetManagedInfoForIndex(int32_t wheelIndex)
   {
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      return wheel ? wheel->GetManagedInfo() : nullptr;
   }

   // Callbacks - protected by s_callbackLock
   static std::mutex s_callbackLock;
   static ItemActivatedCallback s_itemActivatedCallback = nullptr;
   static EditModeCallback s_editModeCallback = nullptr;
   static WheelStateCallback s_wheelStateCallback = nullptr;

   // ============================================================================
   // Internal Helpers
   // ============================================================================

   void SetInitialized(bool initialized)
   {
      s_initialized.store(initialized, std::memory_order_release);
      if (initialized) {
      INFO("WheelerAPI v{} online", API_VERSION);
      }
   }

   // Called by Wheeler when item is activated
   void NotifyItemActivated(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex, uint32_t formID, bool isPrimary)
   {
      // Log if this is a managed wheel
      std::string clientNameCopy = GetManagedWheelClientNameSafe(wheelIndex);
      if (!clientNameCopy.empty()) {
      INFO("WheelerAPI: Item activated on managed wheel {} (client: {}), entry={}, item={}, formID={:08X}, primary={}",
        wheelIndex, clientNameCopy, entryIndex, itemIndex, formID, isPrimary);
      }

      // Copy callback under lock, then invoke outside lock to avoid deadlock
      ItemActivatedCallback callback = nullptr;
      {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      callback = s_itemActivatedCallback;
      }
      if (callback) {
      callback(wheelIndex, entryIndex, itemIndex, formID, isPrimary);
      }
   }

   // Called by Wheeler when edit mode changes
   void NotifyEditModeChanged(bool entered, const WheelChange* changes, size_t changeCount)
   {
      EditModeCallback callback = nullptr;
      {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      callback = s_editModeCallback;
      }
      if (callback) {
      callback(entered, changes, changeCount);
      }
   }

   // Called by Wheeler when wheel opens/closes
   void NotifyWheelStateChanged(int32_t wheelIndex, bool isOpen)
   {
      // Log managed wheel count on state change (best-effort diagnostic)
      {
      int managedCount = s_managedWheelCount.load(std::memory_order_relaxed);
      if (managedCount > 0) {
        DEBUG("WheelerAPI: Wheel {} (index={}) - {} managed wheel(s) registered",
           isOpen ? "opened" : "closed", wheelIndex, managedCount);
      }
      }

      WheelStateCallback callback = nullptr;
      {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      callback = s_wheelStateCallback;
      }
      if (callback) {
      callback(wheelIndex, isOpen);
      }
   }

   // NOTE: These managed-wheel readers are lock-free and read the info straight off
   // the Wheel. Every caller is either the renderer (Wheeler::Update, which holds a
   // shared wheel-data lock) or a Wheeler edit/navigation path that already accesses
   // _wheels the same way — the caller synchronizes, matching Wheeler::GetWheelByIndex.

   // Check if wheel index is managed (for serialization exclusion)
   bool IsManagedWheelIndex(int32_t wheelIndex)
   {
      return GetManagedInfoForIndex(wheelIndex) != nullptr;
   }

   // Get client name for a managed wheel (returns copy for thread safety)
   // Use this when you need to store/log the name safely
   std::string GetManagedWheelClientNameSafe(int32_t wheelIndex)
   {
      const WheelManagedInfo* info = GetManagedInfoForIndex(wheelIndex);
      return info ? info->clientName : std::string();
   }

   // Get client name for a managed wheel
   // WARNING: The returned pointer is only valid while the wheel-data lock is held
   // and the wheel exists. Caller must copy immediately if needed beyond that scope.
   const char* GetManagedWheelClientName(int32_t wheelIndex)
   {
      const WheelManagedInfo* info = GetManagedInfoForIndex(wheelIndex);
      return info ? info->clientName.c_str() : nullptr;
   }

   // Check if managed wheel label should be shown
   bool ShouldShowManagedWheelLabel(int32_t wheelIndex)
   {
      const WheelManagedInfo* info = GetManagedInfoForIndex(wheelIndex);
      return info ? info->showLabel : false;
   }

   // Get managed wheel styling for rendering
   ManagedWheelStyling GetManagedWheelStyling(int32_t wheelIndex)
   {
      ManagedWheelStyling styling;
      styling.isValid = false;

      const WheelManagedInfo* info = GetManagedInfoForIndex(wheelIndex);
      if (info) {
      styling.labelFontSize = info->labelFontSize;
      styling.labelColor = info->labelColor;
      styling.labelOffsetY = info->labelOffsetY;
      styling.indicatorText = info->indicatorText;
      styling.indicatorActiveColor = info->indicatorActiveColor;
      styling.indicatorInactiveColor = info->indicatorInactiveColor;
      styling.isValid = true;
      }
      return styling;
   }

   // (Nothing needs adjusting on insert/remove any more: managed metadata lives on
   // the Wheel and entry captions live on the WheelEntry, so both move with their
   // owner and there is no index-keyed side table left to fix up.)

   // ============================================================================
   // API Implementation Functions
   // ============================================================================

   static bool API_IsInitialized()
   {
      return s_initialized.load(std::memory_order_acquire);
   }

   static bool API_IsInEditMode()
   {
      return Wheeler::IsInEditMode();
   }

   static bool API_IsWheelOpen()
   {
      return Wheeler::IsWheelerOpen();
   }

   static int32_t API_CreateManagedWheel(const WheelConfig* config)
   {
      if (!s_initialized) {
      return static_cast<int32_t>(Result::NotInitialized);
      }
      if (!config) {
      return static_cast<int32_t>(Result::InternalError);
      }
      if (config->numEntries < 1) {
      return static_cast<int32_t>(Result::InternalError);  // Must have at least 1 entry
      }

      std::unique_lock wheelLock(Wheeler::GetWheelDataLock());
      auto& wheels = Wheeler::GetWheels();

      // Create wheel with empty entries
      auto wheel = std::make_unique<Wheel>();
      for (int32_t i = 0; i < config->numEntries; i++) {
      wheel->PushEmptyEntry();
      }

      // Attach managed metadata directly to the wheel (resolving styling defaults),
      // so identity travels with the wheel across any later reindexing.
      if (config->managed) {
      WheelManagedInfo info;
      info.clientName = config->clientName ? config->clientName : "Unknown";
      info.showLabel = config->showLabel;

      // Label styling (use defaults if 0)
      info.labelFontSize = (config->labelFontSize != 0.0f) ? config->labelFontSize : 42.0f;
      info.labelColor = (config->labelColor != 0) ? config->labelColor : C_SKYRIMWHITE;
      info.labelOffsetY = (config->labelOffsetY != 0.0f) ? config->labelOffsetY : 50.0f;

      // Indicator styling
      info.indicatorText = config->indicatorText ? config->indicatorText : "M";
      info.indicatorActiveColor = (config->indicatorActiveColor != 0) ? config->indicatorActiveColor : IM_COL32(0, 255, 255, 255);
      info.indicatorInactiveColor = (config->indicatorInactiveColor != 0) ? config->indicatorInactiveColor : IM_COL32(100, 200, 200, 180);

      wheel->SetManagedInfo(std::move(info));
      s_managedWheelCount.fetch_add(1, std::memory_order_relaxed);
      }

      // Determine insert position
      int32_t index;
      if (config->position < 0 || config->position >= static_cast<int32_t>(wheels.size())) {
      index = static_cast<int32_t>(wheels.size());
      wheels.push_back(std::move(wheel));
      } else {
      index = config->position;
      wheels.insert(wheels.begin() + index, std::move(wheel));
      }

      DEBUG("WheelerAPI: Created managed wheel at index {} with {} entries (client: {}, showLabel: {})",
      index, config->numEntries, config->clientName ? config->clientName : "N/A", config->showLabel);
      return index;
   }

   // Settle the active wheel index after wheels have been erased. Callers pass the
   // index already decremented once per erased slot BELOW it — erasing there shifts
   // every later wheel down, so leaving the index alone silently moves the player
   // onto a different wheel. a_activeWasErased means the wheel the player was on is
   // the one that went away, in which case the index now names whichever wheel slid
   // into its place and the hover left behind no longer belongs to it.
   // Caller must hold the wheel-data lock exclusively.
   static void SettleActiveWheelLocked(int a_activeIdx, bool a_activeWasErased)
   {
      auto& wheels = Wheeler::GetWheels();
      if (wheels.empty()) {
      return;
      }
      if (a_activeIdx >= static_cast<int>(wheels.size())) {
      a_activeIdx = static_cast<int>(wheels.size()) - 1;
      }
      if (a_activeIdx < 0) {
      a_activeIdx = 0;
      }
      Wheeler::SetActiveWheelIndex(a_activeIdx);
      if (a_activeWasErased) {
      wheels[a_activeIdx]->SetHoveredEntryIndex(-1);
      }
   }

   static Result API_DeleteManagedWheel(int32_t wheelIndex)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      std::unique_lock wheelLock(Wheeler::GetWheelDataLock());
      auto& wheels = Wheeler::GetWheels();

      if (wheelIndex < 0 || wheelIndex >= static_cast<int32_t>(wheels.size())) {
      return Result::InvalidWheelIndex;
      }

      // Managed state reads straight off the wheel — cannot be stale.
      if (!wheels[wheelIndex]->IsManaged()) {
      return Result::NotManagedWheel;
      }

      if (wheels.size() <= 1) {
      return Result::LastWheel;
      }

      int activeIdx = Wheeler::GetActiveWheelIndex();
      const bool activeWasErased = (wheelIndex == activeIdx);

      wheels.erase(wheels.begin() + wheelIndex);
      s_managedWheelCount.fetch_sub(1, std::memory_order_relaxed);

      if (wheelIndex < activeIdx) {
      --activeIdx;  // the wheels above the erased slot all shifted down one
      }
      SettleActiveWheelLocked(activeIdx, activeWasErased);

      DEBUG("WheelerAPI: Deleted managed wheel at index {}", wheelIndex);
      return Result::OK;
   }

   // Delete every managed wheel owned by a client in one shift-safe pass. Callers
   // that track their own wheels by index (e.g. Huginn) can't safely delete them
   // one-by-one, because each erase shifts the remaining indices; this removes them
   // all at once, high-to-low, so no stale index is ever dereferenced. Returns the
   // number of wheels deleted (>= 0), or a negative Result on error.
   static int32_t API_DeleteManagedWheelsForClient(const char* clientName)
   {
      if (!s_initialized) {
      return static_cast<int32_t>(Result::NotInitialized);
      }
      if (!clientName) {
      return static_cast<int32_t>(Result::InternalError);
      }

      std::unique_lock wheelLock(Wheeler::GetWheelDataLock());
      auto& wheels = Wheeler::GetWheels();

      // Collect matching indices, then erase from the highest down so earlier
      // indices stay valid as we go.
      std::vector<int32_t> toDelete;
      for (int32_t i = 0; i < static_cast<int32_t>(wheels.size()); ++i) {
      const WheelManagedInfo* info = wheels[i]->GetManagedInfo();
      if (info && info->clientName == clientName) {
        toDelete.push_back(i);
      }
      }

      // Never remove the last remaining wheel (Wheeler must keep >= 1).
      // Track the active index as we go rather than afterwards: the loop can stop
      // early on that guard, so only the slots actually erased may shift it.
      int activeIdx = Wheeler::GetActiveWheelIndex();
      bool activeWasErased = false;
      int32_t deleted = 0;
      for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it) {
      if (wheels.size() <= 1) {
        break;
      }
      wheels.erase(wheels.begin() + *it);
      s_managedWheelCount.fetch_sub(1, std::memory_order_relaxed);
      ++deleted;
      if (*it < activeIdx) {
        --activeIdx;
      } else if (*it == activeIdx) {
        activeWasErased = true;
      }
      }

      SettleActiveWheelLocked(activeIdx, activeWasErased);

      DEBUG("WheelerAPI: Deleted {} managed wheel(s) for client '{}'", deleted, clientName);
      return deleted;
   }

   // Read counterpart to API_DeleteManagedWheelsForClient. Clients store wheel
   // indices, but an index is only meaningful until the next insert/remove; the
   // client name is the one key that survives. This lets a client that has lost
   // track of its indices re-derive them rather than give up on the wheel.
   // Returns the total match count (which may exceed maxCount), or a negative
   // Result on error.
   static int32_t API_GetManagedWheelsForClient(const char* clientName, int32_t* outIndices, size_t maxCount)
   {
      if (!s_initialized) {
      return static_cast<int32_t>(Result::NotInitialized);
      }
      if (!clientName) {
      return static_cast<int32_t>(Result::InternalError);
      }

      std::shared_lock wheelLock(Wheeler::GetWheelDataLock());
      auto& wheels = Wheeler::GetWheels();

      int32_t found = 0;
      for (int32_t i = 0; i < static_cast<int32_t>(wheels.size()); ++i) {
      const WheelManagedInfo* info = wheels[i]->GetManagedInfo();
      if (!info || info->clientName != clientName) {
        continue;
      }
      // Keep counting past the buffer so the caller can tell it was truncated.
      if (outIndices && static_cast<size_t>(found) < maxCount) {
        outIndices[found] = i;
      }
      ++found;
      }

      DEBUG("WheelerAPI: {} managed wheel(s) found for client '{}'", found, clientName);
      return found;
   }

   static bool API_IsManagedWheel(int32_t wheelIndex)
   {
      // Public entry — external callers don't hold the wheel-data lock, so take it.
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      return GetManagedInfoForIndex(wheelIndex) != nullptr;
   }

   static int32_t API_GetWheelCount()
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      return Wheeler::GetWheelCount();
   }

   static int32_t API_GetActiveWheelIndex()
   {
      return Wheeler::GetActiveWheelIndex();
   }

   static Result API_SetActiveWheelIndex(int32_t index)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      std::shared_lock lock(Wheeler::GetWheelDataLock());
      if (index < 0 || index >= Wheeler::GetWheelCount()) {
      return Result::InvalidWheelIndex;
      }

      Wheeler::SetActiveWheelIndex(index);
      return Result::OK;
   }

   static bool API_IsWheelEmpty(int32_t wheelIndex)
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return true;
      }
      return wheel->IsEmpty();
   }

   static int32_t API_GetEntryCount(int32_t wheelIndex)
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return static_cast<int32_t>(Result::InvalidWheelIndex);
      }
      return wheel->GetNumEntries();
   }

   static int32_t API_AddEntry(int32_t wheelIndex)
   {
      if (!s_initialized) {
      return static_cast<int32_t>(Result::NotInitialized);
      }

      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return static_cast<int32_t>(Result::InvalidWheelIndex);
      }

      wheel->PushEmptyEntry();
      return wheel->GetNumEntries() - 1;
   }

   static Result API_DeleteEntry(int32_t wheelIndex, int32_t entryIndex)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return Result::InvalidWheelIndex;
      }

      if (!wheel->RemoveEntry(entryIndex)) {
      return Result::InvalidEntryIndex;
      }
      return Result::OK;
   }

   static bool API_IsEntryEmpty(int32_t wheelIndex, int32_t entryIndex)
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return true;
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return true;
      }
      return entry->IsEmpty();
   }

   static int32_t API_GetItemCount(int32_t wheelIndex, int32_t entryIndex)
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return static_cast<int32_t>(Result::InvalidWheelIndex);
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return static_cast<int32_t>(Result::InvalidEntryIndex);
      }
      return entry->GetNumItems();
   }

   static int32_t API_AddItemByFormID(int32_t wheelIndex, int32_t entryIndex, uint32_t formID, uint16_t uniqueID)
   {
      if (!s_initialized) {
      return static_cast<int32_t>(Result::NotInitialized);
      }

      // Validate form exists
      RE::TESForm* form = RE::TESForm::LookupByID(formID);
      if (!form) {
      return static_cast<int32_t>(Result::FormNotFound);
      }

      // Create the wheel item
      std::shared_ptr<WheelItem> item = WheelItemFactory::MakeWheelItemFromFormID(formID, uniqueID);
      if (!item) {
      return static_cast<int32_t>(Result::UnsupportedFormType);
      }

      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return static_cast<int32_t>(Result::InvalidWheelIndex);
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return static_cast<int32_t>(Result::InvalidEntryIndex);
      }

      entry->PushItem(item);
      return entry->GetNumItems() - 1;  // Return index of newly added item
   }

   static Result API_RemoveItem(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return Result::InvalidWheelIndex;
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return Result::InvalidEntryIndex;
      }
      if (!entry->RemoveItemAt(itemIndex)) {
      return Result::InvalidItemIndex;
      }
      return Result::OK;
   }

   static Result API_ClearEntry(int32_t wheelIndex, int32_t entryIndex)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return Result::InvalidWheelIndex;
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return Result::InvalidEntryIndex;
      }
      entry->ClearItems();
      return Result::OK;
   }

   static uint32_t API_GetItemFormID(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex)
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return 0;
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return 0;
      }
      WheelItem* item = entry->GetItem(itemIndex);
      if (!item) {
      return 0;
      }
      return item->GetFormID();
   }

   static int32_t API_GetSelectedItemIndex(int32_t wheelIndex, int32_t entryIndex)
   {
      std::shared_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return static_cast<int32_t>(Result::InvalidWheelIndex);
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return static_cast<int32_t>(Result::InvalidEntryIndex);
      }
      return entry->GetSelectedItemIndex();
   }

   static Result API_SetSelectedItemIndex(int32_t wheelIndex, int32_t entryIndex, int32_t itemIndex)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return Result::InvalidWheelIndex;
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return Result::InvalidEntryIndex;
      }
      if (itemIndex < 0 || itemIndex >= entry->GetNumItems()) {
      return Result::InvalidItemIndex;
      }
      entry->SetSelectedItem(itemIndex);
      return Result::OK;
   }

   static void API_RegisterItemActivatedCallback(ItemActivatedCallback callback)
   {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      s_itemActivatedCallback = callback;
   }

   static void API_RegisterEditModeCallback(EditModeCallback callback)
   {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      s_editModeCallback = callback;
   }

   static void API_RegisterWheelStateCallback(WheelStateCallback callback)
   {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      s_wheelStateCallback = callback;
   }

   static void API_UnregisterItemActivatedCallback()
   {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      s_itemActivatedCallback = nullptr;
   }

   static void API_UnregisterEditModeCallback()
   {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      s_editModeCallback = nullptr;
   }

   static void API_UnregisterWheelStateCallback()
   {
      std::lock_guard<std::mutex> lock(s_callbackLock);
      s_wheelStateCallback = nullptr;
   }

   static Result API_SetManagedWheelEntrySubtext(int32_t wheelIndex, int32_t entryIndex, const SubtextConfig* config)
   {
      if (!s_initialized) {
      return Result::NotInitialized;
      }

      // The caption is stored on the entry itself, so it follows that entry through
      // any later reindexing instead of being stranded on a (wheel, entry) position.
      std::unique_lock lock(Wheeler::GetWheelDataLock());
      Wheel* wheel = Wheeler::GetWheelByIndex(wheelIndex);
      if (!wheel) {
      return Result::InvalidWheelIndex;
      }
      if (!wheel->IsManaged()) {
      return Result::NotManagedWheel;
      }
      WheelEntry* entry = wheel->GetEntry(entryIndex);
      if (!entry) {
      return Result::InvalidEntryIndex;
      }

      // Clear subtext if config is null or text is null/empty
      if (!config || !config->text || config->text[0] == '\0') {
      entry->ClearSubtext();
      DEBUG("WheelerAPI: Cleared subtext for wheel {} entry {}", wheelIndex, entryIndex);
      return Result::OK;
      }

      // EntrySubtext's member initializers carry the defaults; a 0 in the config
      // means "leave the default alone" for everything except offsetX, where 0 is
      // a meaningful value (centered).
      EntrySubtext subtext;
      subtext.text = config->text;
      subtext.offsetX = config->offsetX;
      if (config->offsetY != 0.0f) {
      subtext.offsetY = config->offsetY;
      }
      if (config->fontSize != 0.0f) {
      subtext.fontSize = config->fontSize;
      }
      if (config->color != 0) {
      subtext.color = config->color;
      }

      DEBUG("WheelerAPI: Set subtext '{}' for wheel {} entry {} (offset: {},{}, size: {}, color: {:08X})",
      subtext.text, wheelIndex, entryIndex, subtext.offsetX, subtext.offsetY, subtext.fontSize, subtext.color);
      entry->SetSubtext(std::move(subtext));
      return Result::OK;
   }

   // ============================================================================
   // API Interface Instance
   // ============================================================================

   static IWheelerAPI s_apiInstance = {
      .version = API_VERSION,
      .IsInitialized = API_IsInitialized,
      .IsInEditMode = API_IsInEditMode,
      .IsWheelOpen = API_IsWheelOpen,
      .CreateManagedWheel = API_CreateManagedWheel,
      .DeleteManagedWheel = API_DeleteManagedWheel,
      .IsManagedWheel = API_IsManagedWheel,
      .GetWheelCount = API_GetWheelCount,
      .GetActiveWheelIndex = API_GetActiveWheelIndex,
      .SetActiveWheelIndex = API_SetActiveWheelIndex,
      .IsWheelEmpty = API_IsWheelEmpty,
      .GetEntryCount = API_GetEntryCount,
      .AddEntry = API_AddEntry,
      .DeleteEntry = API_DeleteEntry,
      .IsEntryEmpty = API_IsEntryEmpty,
      .GetItemCount = API_GetItemCount,
      .AddItemByFormID = API_AddItemByFormID,
      .RemoveItem = API_RemoveItem,
      .ClearEntry = API_ClearEntry,
      .GetItemFormID = API_GetItemFormID,
      .GetSelectedItemIndex = API_GetSelectedItemIndex,
      .SetSelectedItemIndex = API_SetSelectedItemIndex,
      .RegisterItemActivatedCallback = API_RegisterItemActivatedCallback,
      .RegisterEditModeCallback = API_RegisterEditModeCallback,
      .RegisterWheelStateCallback = API_RegisterWheelStateCallback,
      .UnregisterItemActivatedCallback = API_UnregisterItemActivatedCallback,
      .UnregisterEditModeCallback = API_UnregisterEditModeCallback,
      .UnregisterWheelStateCallback = API_UnregisterWheelStateCallback,
      .SetManagedWheelEntrySubtext = API_SetManagedWheelEntrySubtext,
      .DeleteManagedWheelsForClient = API_DeleteManagedWheelsForClient,
      .GetManagedWheelsForClient = API_GetManagedWheelsForClient,
   };

}  // namespace WheelerAPI

// ============================================================================
// Main Entry Point Export
// ============================================================================

extern "C" WHEELER_API WheelerAPI::IWheelerAPI* GetWheelerAPI()
{
   return &WheelerAPI::s_apiInstance;
}
