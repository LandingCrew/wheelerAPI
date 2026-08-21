#pragma once
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include "nlohmann/json.hpp"
#include "WheelEntry.h"

// Metadata for an API-managed wheel (one owned by an external client, e.g. Huginn).
// This lives ON the wheel rather than in a side-map keyed by list position, so it
// can never desync from the wheel when the list is reindexed by insert/remove.
// Colors are stored resolved (the API applies its defaults before setting these).
struct WheelManagedInfo
{
   std::string clientName;
   bool        showLabel = false;

   // Label styling
   float    labelFontSize = 42.0f;
   uint32_t labelColor = 0;
   float    labelOffsetY = 50.0f;

   // Indicator styling
   std::string indicatorText = "M";
   uint32_t    indicatorActiveColor = 0;
   uint32_t    indicatorInactiveColor = 0;
};

class Wheel
{
public:
    Wheel();
    ~Wheel();

   // ---- API-managed wheel metadata (lock-free; caller holds the wheel-data lock) ----
   [[nodiscard]] bool IsManaged() const { return _managedInfo.has_value(); }
   [[nodiscard]] const WheelManagedInfo* GetManagedInfo() const { return _managedInfo ? &*_managedInfo : nullptr; }
   void SetManagedInfo(WheelManagedInfo a_info) { _managedInfo = std::move(a_info); }
   void ClearManagedInfo() { _managedInfo.reset(); }

   void Draw(ImVec2 a_wheelCenter, float a_cursorAngle, bool a_cursorCentered, RE::TESObjectREFR::InventoryItemMap& a_imap,
      DrawArgs a_drawArgs);
      

    void Clear();
    bool IsEmpty();
    
    void PushEntry(std::unique_ptr<WheelEntry> a_entry);
   void PushEmptyEntry();

    void PrevItemInHoveredEntry();
    void NextItemInHoveredEntry();
   
   void ResetAnimation();

    /// <summary>
   /// Activate the entry using a primary input(mouse left click / controller right trigger), which either activates
   /// the currently active item in the entry, or, under edit mode, adds a new item to the entry(if applicable).
    /// </summary>
    void ActivateHoveredEntryPrimary(bool a_editMode);

   /// <summary>
   /// Activate the entry using a secondary input(mouse right click / controller left trigger), which either deletes
   /// an item in the entry, or the whole entry when it's empty.
   /// </summary>
   /// <param name="a_editMode">Whether we're in edit mode, which prompts us to deletion.</param>
   void ActivateHoveredEntrySecondary(bool a_editMode);

   void ActivateHoveredEntrySpecial(bool a_editMode);
   void SetHoveredEntryIndex(int a_index);

   void MoveHoveredEntryForward();
   void MoveHoveredEntryBack();

    void SerializeIntoJsonObj(nlohmann::json& a_json);
   static std::unique_ptr<Wheel> SerializeFromJsonObj(const nlohmann::json& a_json, SKSE::SerializationInterface* a_intfc);
   
   int GetNumEntries();

   // API access - returns nullptr if index out of range
   WheelEntry* GetEntry(int a_index);

   // API access - remove entry at index, returns true if successful
   bool RemoveEntry(int a_index);

   // API access - get currently hovered entry index (-1 if none)
   int GetHoveredEntryIndex() const;

private:
    std::vector<std::unique_ptr<WheelEntry>> _entries = {};
   std::shared_mutex _lock;

   // currently active item, will be highlighted. Gets reset every time wheel reopens.
   int _hoveredEntryIdx = -1;

   // Present iff this wheel is API-managed by an external client. Guarded by the
   // caller (Wheeler's wheel-data lock), like the rest of the wheel's state.
   std::optional<WheelManagedInfo> _managedInfo;
};
