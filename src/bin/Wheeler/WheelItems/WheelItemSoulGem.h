#pragma once
#include "WheelItem.h"

/// <summary>
/// A soul gem in a wheel slot. Activating one spends its soul to recharge the
/// enchanted weapon the player has equipped, which is the only thing soul gems
/// are actually for — the vanilla inventory does nothing when you select one.
/// </summary>
///
/// A slot holds one base form, deliberately. Note that in vanilla "Grand Soul
/// Gem" empty and "Grand Soul Gem" filled are two separate forms — same name in
/// the UI, different FormIDs, linked by TESSoulGem::linkedSoulGem — and capturing
/// or spending a soul swaps one for the other rather than modifying the gem. So a
/// slot made from an empty gem stays an empty gem even once the player has filled
/// ones, and a slot made from a filled gem clears when the last of them is spent.
///
/// That is the same contract every other wheel item has: the slot is the form you
/// put in it, and it clears when you run out, exactly as a potion does. Following
/// linkedSoulGem to treat the pair as interchangeable was considered and rejected
/// — it would have Wheeler substitute a form the player did not choose, quietly
/// spending filled gems from a slot set up to hold empty ones.
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
   ///
   /// a_holder receives the live ExtraDataList the soul was read from, so the
   /// caller can act on that exact stack rather than on any copy of the form. It
   /// is left null when the soul came from the base form, where every instance is
   /// equally full and picking between them is meaningless.
   /// </summary>
   RE::SOUL_LEVEL getAvailableSoul(RE::ExtraDataList** a_holder = nullptr) const;
};
