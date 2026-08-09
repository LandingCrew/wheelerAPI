#include "bin/Rendering/TextureManager.h"
#include "bin/Rendering/Drawer.h"
#include "bin/Utilities/Utils.h"
#include "bin/Texts.h"
#include "WheelItemSoulGem.h"

namespace
{
   // Reusable soul gems, which survive being spent on a recharge instead of being
   // used up. Skyrim.esm is always at load order 00, so these full form IDs are
   // stable. Nothing in the form data distinguishes a reusable gem from an
   // ordinary one, so the only way to know is to name them.
   //
   // Add to this list to cover reusable gems from other plugins; a form ID from a
   // non-master plugin has to be resolved through TESDataHandler::LookupFormID
   // rather than hardcoded, since its load order index is not fixed.
   constexpr RE::FormID AZURAS_STAR = 0x00063B27;
   constexpr RE::FormID THE_BLACK_STAR = 0x00063B29;

   bool isReusableSoulGem(const RE::TESSoulGem* a_soulGem)
   {
      if (!a_soulGem) {
      return false;
      }
      const RE::FormID formID = a_soulGem->GetFormID();
      return formID == AZURAS_STAR || formID == THE_BLACK_STAR;
   }

   // The extra data of the item worn in one hand. Actor::GetEquippedEntryData()
   // returns the process's cached entry, whose extra data does not reliably carry
   // the item's own ExtraCharge, so this reads the inventory's worn stack instead.
   // Matching on the hand rather than on the form alone matters when the player
   // dual-wields two copies of the same weapon.
   RE::ExtraDataList* wornExtraData(RE::PlayerCharacter* a_pc, RE::TESBoundObject* a_bound, bool a_leftHand)
   {
      RE::InventoryChanges* changes = a_pc ? a_pc->GetInventoryChanges() : nullptr;
      if (!a_bound || !changes || !changes->entryList) {
      return nullptr;
      }

      for (RE::InventoryEntryData* entry : *changes->entryList) {
      if (!entry || entry->object != a_bound || !entry->extraLists) {
        continue;
      }
      for (RE::ExtraDataList* xList : *entry->extraLists) {
        if (!xList) {
           continue;
        }
        if (a_leftHand ? xList->HasType<RE::ExtraWornLeft>() : xList->HasType<RE::ExtraWorn>()) {
           return xList;
        }
      }
      }
      return nullptr;
   }

   // Charge capacity of the enchanted weapon held in one hand, or 0 if that hand
   // holds nothing rechargeable. r_wornList receives the worn stack's extra data
   // when there is one.
   //
   // Capacity lives in one of two places. A base-enchanted weapon, and a staff,
   // declare it on the form as amountofEnchantment. A weapon the player enchanted
   // themselves declares nothing on the form at all -- formEnchanting is null and
   // amountofEnchantment is 0 -- and carries both on the worn stack's
   // ExtraEnchantment, so the form must not be used to rule the weapon out.
   float equippedWeaponCapacity(RE::PlayerCharacter* a_pc, RE::TESForm* a_equipped, bool a_leftHand,
      RE::ExtraDataList*& r_wornList)
   {
      r_wornList = nullptr;

      auto* weapon = a_equipped ? a_equipped->As<RE::TESObjectWEAP>() : nullptr;
      auto* enchantable = weapon ? weapon->As<RE::TESEnchantableForm>() : nullptr;
      if (!enchantable) {
      return 0.0f;
      }

      r_wornList = wornExtraData(a_pc, weapon, a_leftHand);

      const bool formDeclaresCharge =
      enchantable->formEnchanting || weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff;
      if (formDeclaresCharge && enchantable->amountofEnchantment != 0) {
      return static_cast<float>(enchantable->amountofEnchantment);
      }

      if (r_wornList) {
      auto* xEnch = r_wornList->GetByType<RE::ExtraEnchantment>();
      if (xEnch && xEnch->enchantment && xEnch->charge != 0) {
        return static_cast<float>(xEnch->charge);
      }
      }
      return 0.0f;
   }
}

WheelItemSoulGem::WheelItemSoulGem(RE::TESSoulGem* a_soulGem)
{
   this->_soulGem = a_soulGem;
   this->_texture = Texture::GetIconImage(Texture::icon_image_type::icon_default, a_soulGem);

   // Deliberately no capacity/contained-soul description. Tracking what a gem
   // holds is not Wheeler's job, and it cannot be done honestly from the form
   // anyway: reusable gems such as the Black Star are a single form filled in
   // place through ExtraSoul, so the form always reports an empty soul and the
   // label read "Grand (Empty)" for a gem that was full. The name and count the
   // slot already draws are what Wheeler can actually vouch for.
}

void WheelItemSoulGem::DrawSlot(ImVec2 a_center, bool a_hovered, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs)
{
   const char* itemName = Utils::SafeGetName(this->_soulGem, "Soul Gem");
   int itemCount = a_imap.contains(this->_soulGem) ? a_imap.find(this->_soulGem)->second.first : 0;
   std::string text = fmt::format("{} ({})", itemName, itemCount);
   this->drawSlotText(a_center, text.data(), a_drawArgs);
   this->drawSlotTexture(a_center, a_drawArgs);
}

void WheelItemSoulGem::DrawHighlight(ImVec2 a_center, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs)
{
   this->drawHighlightText(a_center, Utils::SafeGetName(this->_soulGem, "Soul Gem"), a_drawArgs);
   this->drawHighlightTexture(a_center, a_drawArgs);
   if (!this->_description.empty()) {
      this->drawHighlightDescription(a_center, this->_description.data(), a_drawArgs);
   }
}

bool WheelItemSoulGem::IsActive(RE::TESObjectREFR::InventoryItemMap& a_inv)
{
   return false;
}

bool WheelItemSoulGem::IsAvailable(RE::TESObjectREFR::InventoryItemMap& a_inv)
{
   auto it = a_inv.find(this->_soulGem);
   return it != a_inv.end() && it->second.first > 0;
}

void WheelItemSoulGem::ActivateItemPrimary()
{
   this->rechargeEquippedWeapon();
}

void WheelItemSoulGem::ActivateItemSecondary()
{
   this->rechargeEquippedWeapon();
}

void WheelItemSoulGem::SerializeIntoJsonObj(nlohmann::json& a_json)
{
   a_json["type"] = WheelItemSoulGem::ITEM_TYPE_STR;
   a_json["formID"] = this->_soulGem->GetFormID();
}

RE::SOUL_LEVEL WheelItemSoulGem::getAvailableSoul(RE::ExtraDataList** a_holder) const
{
   if (a_holder) {
      *a_holder = nullptr;
   }
   if (!this->_soulGem) {
      return RE::SOUL_LEVEL::kNone;
   }

   const RE::SOUL_LEVEL onForm = this->_soulGem->GetContainedSoul();
   if (onForm != RE::SOUL_LEVEL::kNone) {
      // A filled vanilla gem is its own base form, so every copy the player holds
      // is equally full and there is no particular stack to single out.
      return onForm;
   }

   RE::PlayerCharacter* pc = RE::PlayerCharacter::GetSingleton();
   RE::InventoryChanges* changes = pc ? pc->GetInventoryChanges() : nullptr;
   if (!changes || !changes->entryList) {
      return RE::SOUL_LEVEL::kNone;
   }

   // Nothing on the form — the gem was filled in place, so the soul is on one
   // particular stack in the inventory. Remember which, because the caller has to
   // drain or spend that same stack and not merely something of the same form.
   //
   // Take the SMALLEST soul, not the largest. A recharge fills the weapon
   // regardless of soul size, so spending the biggest soul available destroys the
   // most valuable gem for exactly the result the cheapest one would have given.
   RE::SOUL_LEVEL best = RE::SOUL_LEVEL::kNone;
   for (RE::InventoryEntryData* entry : *changes->entryList) {
      if (!entry || entry->object != this->_soulGem || !entry->extraLists) {
      continue;
      }
      for (RE::ExtraDataList* xList : *entry->extraLists) {
      if (!xList) {
        continue;
      }
      auto* xSoul = xList->GetByType<RE::ExtraSoul>();
      if (!xSoul) {
        continue;
      }
      const RE::SOUL_LEVEL soul = xSoul->GetContainedSoul();
      if (soul == RE::SOUL_LEVEL::kNone) {
        continue;
      }
      if (best == RE::SOUL_LEVEL::kNone || soul < best) {
        best = soul;
        if (a_holder) {
           *a_holder = xList;
        }
      }
      }
   }
   return best;
}

void WheelItemSoulGem::rechargeEquippedWeapon()
{
   RE::PlayerCharacter* pc = RE::PlayerCharacter::GetSingleton();
   if (!pc || !this->_soulGem) {
      return;
   }

   // IMPORTANT: no ExtraDataList* obtained here may be held across a call back
   // into the game. ModActorValue below can run perk entry points and HUD updates,
   // and anything there that touches the player's inventory frees or relocates
   // extra data. Caching a stack pointer across that and handing the stale one to
   // RemoveItem corrupts the entry list, which then crashes the next frame in
   // Wheeler::Update, where GetInventory() walks that list to copy it. Every
   // lookup below is therefore re-resolved at its point of use.
   //
   // The only thing worth asking of the gem is whether there is a soul in it at
   // all. How much charge a soul is worth is the game's bookkeeping, not
   // Wheeler's, so a spent gem restores the weapon to full.
   if (this->getAvailableSoul() == RE::SOUL_LEVEL::kNone) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemEmptyWarning));
      return;
   }

   // The wheel slot outlives the inventory: the item can be gone by the time this
   // runs, and for a gem whose soul sits on the base form nothing above would have
   // noticed. Spending what the player does not have would recharge for free.
   RE::InventoryChanges* changes = pc->GetInventoryChanges();
   if (!changes || changes->GetItemCount(this->_soulGem) <= 0) {
      return;
   }

   RE::ActorValueOwner* avOwner = pc->AsActorValueOwner();
   if (!avOwner) {
      return;
   }

   // Charge lives in the kRightItemCharge / kLeftItemCharge actor values. That is
   // what the game's own HUD reads, and it is the only reliable source: the
   // ExtraCharge in inventory extra data is created lazily and does not track the
   // current charge of a base-enchanted weapon. Writing ExtraCharge instead spent
   // the gem and moved nothing.
   //
   // Right hand first, then left, taking the first hand that actually needs
   // charge. foundEnchanted records that there was something rechargeable at all,
   // so "no enchanted weapon" stays distinguishable from "nothing needed it".
   bool foundEnchanted = false;
   bool chargeLeftHand = false;
   RE::ActorValue chargeAV = RE::ActorValue::kNone;
   float maxCharge = 0.0f;
   float current = 0.0f;
   for (const auto& hand : { std::pair{ false, RE::ActorValue::kRightItemCharge },
           std::pair{ true, RE::ActorValue::kLeftItemCharge } }) {
      RE::ExtraDataList* candidateList = nullptr;
      const float capacity =
      equippedWeaponCapacity(pc, pc->GetEquippedObject(hand.first), hand.first, candidateList);
      if (capacity <= 0.0f) {
      continue;
      }
      foundEnchanted = true;
      const float charge = avOwner->GetActorValue(hand.second);
      if (charge < capacity) {
      chargeAV = hand.second;
      chargeLeftHand = hand.first;
      maxCharge = capacity;
      current = charge;
      break;
      }
   }

   if (!foundEnchanted) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemNoEnchantedWeapon));
      return;
   }

   // Every equipped weapon is already full. Nothing is said about it — refusing to
   // overcharge is not Wheeler's call — but nothing is taken either, because
   // destroying a gem to grant zero charge is not the same as letting the player
   // overcharge. There is simply nothing here to do.
   if (chargeAV == RE::ActorValue::kNone) {
      return;
   }

   // Spend the gem BEFORE granting the charge. ModActorValue is the call that can
   // invalidate extra data, so everything that needs a live ExtraDataList has to
   // be finished first. If the spend fails there is nothing to undo, whereas
   // granting first and failing to spend would be a free recharge.
   //
   // Re-resolve the soul's stack here rather than reusing anything looked up
   // earlier: this is the point of use, and the pointer is only known good now.
   RE::ExtraDataList* soulHolder = nullptr;
   if (this->getAvailableSoul(&soulHolder) == RE::SOUL_LEVEL::kNone) {
      return;
   }

   if (isReusableSoulGem(this->_soulGem)) {
      // A reusable gem survives but is emptied, as in vanilla. With no stack to
      // clear there is nothing to spend, and granting the charge anyway would make
      // the refill free — so do nothing at all.
      if (!soulHolder) {
      return;
      }
      auto* xSoul = soulHolder->GetByType<RE::ExtraSoul>();
      if (!xSoul) {
      return;
      }
      xSoul->soul = RE::SOUL_LEVEL::kNone;
   } else {
      // Pass the holder so the stack that supplied the soul is the one spent.
      // Removing by form alone can delete an empty copy and leave the full one,
      // which hands the player an unlimited recharge.
      pc->RemoveItem(this->_soulGem, 1, RE::ITEM_REMOVE_REASON::kRemove, soulHolder, nullptr);
   }

   // Keep the item's own charge data in step when it exists. The actor value is
   // what the game reads while the weapon is equipped, but ExtraCharge is what
   // persists on the stack, and leaving a stale one behind risks the recharge
   // being undone on unequip or reload. Never created here — the game creates it
   // lazily, and inventing one would be asserting state we do not own. Re-resolved
   // rather than cached, for the same reason as the soul stack above.
   RE::TESForm* chargedWeapon = pc->GetEquippedObject(chargeLeftHand);
   auto* chargedBound = chargedWeapon ? chargedWeapon->As<RE::TESBoundObject>() : nullptr;
   if (RE::ExtraDataList* weaponList = wornExtraData(pc, chargedBound, chargeLeftHand)) {
      if (auto* xCharge = weaponList->GetByType<RE::ExtraCharge>()) {
      xCharge->charge = maxCharge;
      }
   }

   avOwner->ModActorValue(chargeAV, maxCharge - current);

   DEBUG("WheelerAPI: Recharged {} hand: {} -> {}",
      chargeLeftHand ? "left" : "right", current, maxCharge);

   Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemRecharged));
}
