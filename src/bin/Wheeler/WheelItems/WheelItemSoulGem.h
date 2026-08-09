#pragma once
#include "WheelItem.h"

/// <summary>
/// A soul gem in a wheel slot. Activating one spends its soul to recharge the
/// enchanted weapon the player has equipped, which is the only thing soul gems
/// are actually for — the vanilla inventory does nothing when you select one.
/// </summary>
///
/// TODO: a slot remembers one base form, but a gem changes form when it is
/// filled, so the slot can end up pointing at a form the player never has.
///
/// In vanilla, "Grand Soul Gem" (empty) and "Grand Soul Gem" (filled) are two
/// separate forms — same name in the UI, different FormIDs, linked by
/// TESSoulGem::linkedSoulGem. Capturing a soul does not modify the gem; it swaps
/// the empty form for the filled one. Spending it swaps back.
///
/// This slot stores whichever form was on the cursor when it was created, so:
///   - Add an EMPTY gem to the wheel, then go and fill your gems. The slot still
///     points at the empty form, so activating it says "holds no soul" while the
///     player is carrying a bag of filled ones.
///   - Add a FILLED gem instead and use it. The gem becomes the empty form, the
///     filled count drops to zero, IsAvailable goes false and the wheel drops the
///     item — the slot goes blank after a single use.
///
/// The fix is for the slot to treat a gem and its linked counterpart as the same
/// thing: follow linkedSoulGem in both directions when resolving availability,
/// count and soul, rather than matching one FormID. Left as a TODO because it
/// changes what "the item in this slot" means and wants its own testing pass.
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
