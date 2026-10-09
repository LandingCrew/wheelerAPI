#pragma once
#include "bin/Rendering/TextureManager.h"
#include "bin/Config.h"
#include "nlohmann/json.hpp"
class ImVec2;
class WheelItem
{
public:
   WheelItem(){};
   
   /// <summary>
   /// Draw everything that's supposed to be in a wheel slot(entry)
   /// Currently, DrawSlot should draw an image of the item and its name.
   /// </summary>
   virtual void DrawSlot(ImVec2 a_center, bool a_hovered, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs);
   
   /// <summary>
   /// Draw everything of the item that's supposed to be in the highlight region i.e. center of the wheel.
   /// Currently, DrawHighlight should draw an enlarged image of the item, item description, and item stats(if applicable).
   /// </summary>
   virtual void DrawHighlight(ImVec2 a_center, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs);
   virtual bool IsActive(RE::TESObjectREFR::InventoryItemMap& a_inv);

   /// <summary>
   /// Whether the item is available. 
   /// An item is unavailable when the player lacks skill to use it, or the item is not in the player's inventory.
   /// </summary>
   virtual bool IsAvailable(RE::TESObjectREFR::InventoryItemMap& a_inv);

   virtual void ActivateItemPrimary();
   virtual void ActivateItemSecondary();
   virtual void ActivateItemSpecial();

   virtual void SerializeIntoJsonObj(nlohmann::json& a_json);
   static std::shared_ptr<WheelItem> SerializeFromJsonObj(nlohmann::json& a_json);

   // API: Get the form ID of the underlying game form, or 0 if not applicable
   virtual RE::FormID GetFormID() const { return 0; }

   static inline const char* ITEM_TYPE_STR = "WheelItem";

   /// <summary>
   /// Makes every item rebuild its description the next time it's hovered.
   /// Called when the wheel opens, so text that depends on perks or enchantments
   /// is at most one open out of date.
   /// </summary>
   static void InvalidateDescriptions() { _descriptionGeneration.fetch_add(1, std::memory_order_relaxed); }


protected:
   Texture::Image _texture = Texture::Image();
   Texture::Image _stat_texture = Texture::Image();

   /// <summary>
   /// The item's description for the highlight region. Built by buildDescription()
   /// the first time the item is hovered after the wheel opens, not when the item
   /// is made: the game reads description text from the plugin file, and API
   /// clients add items in bursts. Called from the render thread only.
   /// </summary>
   const std::string& getDescription(RE::TESObjectREFR::InventoryItemMap& a_imap);

   /// <summary>
   /// Builds the text getDescription() caches. Empty by default.
   /// </summary>
   virtual std::string buildDescription(RE::TESObjectREFR::InventoryItemMap& a_imap) { return ""; }

   /// <summary>
   /// Whether buildDescription() is safe to call now. While it isn't, getDescription()
   /// keeps the last text it built.
   /// </summary>
   virtual bool canBuildDescription(RE::TESObjectREFR::InventoryItemMap& a_imap) { return true; }

   /// <summary>
   /// The base description text a form carries, which the game reads from disk.
   /// </summary>
   static std::string readBaseDescription(RE::TESDescription* a_form);

   /// <summary>
   /// Draws stat icon and value of the item when the item is highlighted.
   /// Coordinates and scale of the icon texture and value text are determined by Config.
   /// </summary>
   void drawItemHighlightStatIconAndValue(ImVec2 a_center, Texture::Image& a_stat_icon, float a_stat_value, DrawArgs a_drawArgs);

   void drawHighlightDescription(ImVec2 a_center, const char* a_text, DrawArgs a_drawArgs);
   void drawHighlightTexture(ImVec2 a_center, DrawArgs a_drawArgs);
   void drawHighlightText(ImVec2 a_center, const char* a_text, DrawArgs a_drawArgs);
   void drawSlotTexture(ImVec2 a_center, DrawArgs a_drawArgs);
   void drawSlotText(ImVec2 a_center, const char* a_text, DrawArgs a_drawArgs);

private:
   static inline std::atomic<std::uint32_t> _descriptionGeneration = 1;

   std::string _description = "";
   std::uint32_t _descriptionBuiltAt = 0;  // generation the description was built in, 0 = never
};
