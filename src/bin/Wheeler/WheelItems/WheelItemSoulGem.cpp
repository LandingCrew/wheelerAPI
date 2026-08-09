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

   // Find the worn stack of a_object in the player's inventory and report the
   // charge capacity of that particular item, along with the extra data it is
   // worn on.
   //
   // Actor::GetEquippedEntryData() looks like the obvious source and is not: it
   // returns the process's cached entry (middleHigh->rightHand / leftHand), whose
   // extra data does not reliably carry the item's ExtraCharge. Reading that made
   // a fully drained weapon look untouched. The inventory's worn stack is real.
   //
   // r_maxCharge is left at 0 when nothing rechargeable was found.
   void findWornEnchantedItem(RE::PlayerCharacter* a_pc, RE::TESForm* a_object,
      RE::ExtraDataList*& r_wornList, float& r_maxCharge)
   {
      r_wornList = nullptr;
      r_maxCharge = 0.0f;

      auto* bound = a_object ? a_object->As<RE::TESBoundObject>() : nullptr;
      RE::InventoryChanges* changes = a_pc ? a_pc->GetInventoryChanges() : nullptr;
      if (!bound || !changes || !changes->entryList) {
      return;
      }

      auto* enchantable = bound->As<RE::TESEnchantableForm>();
      const float formCharge = (enchantable && enchantable->formEnchanting)
                                  ? static_cast<float>(enchantable->amountofEnchantment)
                                  : 0.0f;

      for (RE::InventoryEntryData* entry : *changes->entryList) {
      if (!entry || entry->object != bound || !entry->extraLists) {
        continue;
      }
      for (RE::ExtraDataList* xList : *entry->extraLists) {
        if (!xList || !(xList->HasType<RE::ExtraWorn>() || xList->HasType<RE::ExtraWornLeft>())) {
           continue;
        }
        // A player-applied enchantment carries its own capacity and overrides
        // whatever the base form declares.
        auto* xEnch = xList->GetByType<RE::ExtraEnchantment>();
        const float charge = (xEnch && xEnch->enchantment && xEnch->charge != 0)
                                ? static_cast<float>(xEnch->charge)
                                : formCharge;
        if (charge > 0.0f) {
           r_wornList = xList;
           r_maxCharge = charge;
           return;
        }
      }
      }
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
   RE::SOUL_LEVEL best = RE::SOUL_LEVEL::kNone;
   for (RE::InventoryEntryData* entry : *changes->entryList) {
      if (!entry || entry->object != this->_soulGem || !entry->extraLists) {
      continue;
      }
      for (RE::ExtraDataList* xList : *entry->extraLists) {
      if (!xList) {
        continue;
      }
      if (auto* xSoul = xList->GetByType<RE::ExtraSoul>()) {
        if (xSoul->GetContainedSoul() > best) {
           best = xSoul->GetContainedSoul();
           if (a_holder) {
            *a_holder = xList;
           }
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

   // The only thing worth asking is whether there is a soul in here at all. How
   // much charge a given soul is worth is the game's bookkeeping, not Wheeler's,
   // and the conversion is not exposed anywhere we could read it honestly — so a
   // spent gem restores the weapon to full rather than to a number we invented.
   //
   // soulHolder pins the exact stack the soul came from; it stays null for vanilla
   // filled gems, whose soul is on the form and whose copies are interchangeable.
   RE::ExtraDataList* soulHolder = nullptr;
   if (this->getAvailableSoul(&soulHolder) == RE::SOUL_LEVEL::kNone) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemEmptyWarning));
      return;
   }

   // Right hand first, then left.
   RE::ExtraDataList* wornList = nullptr;
   float maxCharge = 0.0f;
   for (const bool leftHand : { false, true }) {
      findWornEnchantedItem(pc, pc->GetEquippedObject(leftHand), wornList, maxCharge);
      if (maxCharge > 0.0f) {
      break;
      }
   }

   if (!wornList || maxCharge <= 0.0f) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemNoEnchantedWeapon));
      return;
   }

   // No "already charged" check. Whether a gem is worth spending on a weapon that
   // did not need it is the player's call to make, not Wheeler's to refuse.
   //
   // An item that has never been discharged carries no ExtraCharge at all, so one
   // has to be attached before the charge can be written.
   auto* xCharge = wornList->GetByType<RE::ExtraCharge>();
   if (!xCharge) {
      wornList->Add(new RE::ExtraCharge());
      xCharge = wornList->GetByType<RE::ExtraCharge>();
   }
   if (!xCharge) {
      return;  // could not attach charge data — leave the gem alone
   }

   xCharge->charge = maxCharge;

   // Something has to be spent, or the recharge is free. A reusable gem survives
   // but is emptied and has to be refilled before it works again, as in vanilla;
   // anything else is used up.
   if (isReusableSoulGem(this->_soulGem)) {
      if (soulHolder) {
      if (auto* xSoul = soulHolder->GetByType<RE::ExtraSoul>()) {
        xSoul->soul = RE::SOUL_LEVEL::kNone;
      }
      }
   } else {
      // Pass the holder so the stack that supplied the soul is the one spent.
      // Removing by form alone can delete an empty copy and leave the full one,
      // which hands the player an unlimited recharge.
      pc->RemoveItem(this->_soulGem, 1, RE::ITEM_REMOVE_REASON::kRemove, soulHolder, nullptr);
   }

   Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemRecharged));
}
