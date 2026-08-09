#pragma once
#include "WheelItem.h"

/// <summary>
/// A soul gem in a wheel slot. Activating one spends its soul to recharge the
/// enchanted weapon the player has equipped, which is the only thing soul gems
/// are actually for — the vanilla inventory does nothing when you select one.
/// </summary>
class WheelItemSoulGem : public WheelItem
{
public:
   WheelItemSoulGem() = delete;
   WheelItemSoulGem(RE::TESSoulGem* a_soulGem);

   virtual void DrawSlot(ImVec2 a_center, bool a_hovered, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs) override;
   virtual void DrawHighlight(ImVec2 a_center, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs) override;
   virtual bool IsActive(RE::TESObjectREFR::InventoryItemMap& a_inv) override;
   virtual bool IsAvailable(RE::TESObjectREFR::InventoryItemMap& a_inv) override;
   virtual void ActivateItemPrimary() override;
   virtual void ActivateItemSecondary() override;

   virtual void SerializeIntoJsonObj(nlohmann::json& a_json) override;
   virtual RE::FormID GetFormID() const override { return _soulGem ? _soulGem->GetFormID() : 0; }

   static inline const char* ITEM_TYPE_STR = "WheelItemSoulGem";

private:
   RE::TESSoulGem* _soulGem;

   /// <summary>
   /// Spend this gem's soul on the equipped enchanted weapon. Notifies the
   /// player and leaves the gem untouched if there is nothing to recharge.
   /// </summary>
   void rechargeEquippedWeapon();

   /// <summary>
   /// The soul actually available from this gem. Vanilla gems swap to a filled
   /// base form when they capture a soul, so the level usually lives on the form
   /// itself; gems filled by other means carry it in ExtraSoul on the inventory
   /// entry instead, so fall back to that.
   /// </summary>
   RE::SOUL_LEVEL getAvailableSoul() const;
};
