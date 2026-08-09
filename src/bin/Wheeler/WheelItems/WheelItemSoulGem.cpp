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

   // Locate the extra data holding the weapon's enchantment charge, along with the
   // capacity that charge is measured against. Player-enchanted weapons carry both
   // on ExtraEnchantment; weapons enchanted in the base form carry the capacity on
   // the form and only the current charge in extra data.
   //
   // r_maxCharge is left at 0 when the entry is not a rechargeable enchanted item.
   // r_charge is left null when the item has never been discharged — no ExtraCharge
   // exists until the game writes one, so a missing entry means "full".
   void findEnchantmentCharge(RE::InventoryEntryData* a_entry, float& r_maxCharge, RE::ExtraCharge*& r_charge)
   {
      r_maxCharge = 0.0f;
      r_charge = nullptr;
      if (!a_entry) {
      return;
      }

      if (a_entry->extraLists) {
      for (RE::ExtraDataList* xList : *a_entry->extraLists) {
        if (!xList) {
           continue;
        }
        auto* xEnch = xList->GetByType<RE::ExtraEnchantment>();
        if (xEnch && xEnch->enchantment && xEnch->charge != 0) {
           r_maxCharge = static_cast<float>(xEnch->charge);
           r_charge = xList->GetByType<RE::ExtraCharge>();
           return;
        }
      }
      }

      // Read the member directly: GetObject() collides with the Windows macro.
      RE::TESBoundObject* obj = a_entry->object;
      auto* enchantable = obj ? obj->As<RE::TESEnchantableForm>() : nullptr;
      if (!enchantable || !enchantable->formEnchanting || enchantable->amountofEnchantment == 0) {
      return;
      }
      r_maxCharge = static_cast<float>(enchantable->amountofEnchantment);

      if (a_entry->extraLists) {
      for (RE::ExtraDataList* xList : *a_entry->extraLists) {
        if (!xList) {
           continue;
        }
        if (auto* xCharge = xList->GetByType<RE::ExtraCharge>()) {
           r_charge = xCharge;
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

RE::SOUL_LEVEL WheelItemSoulGem::getAvailableSoul() const
{
   if (!this->_soulGem) {
      return RE::SOUL_LEVEL::kNone;
   }

   const RE::SOUL_LEVEL onForm = this->_soulGem->GetContainedSoul();
   if (onForm != RE::SOUL_LEVEL::kNone) {
      return onForm;
   }

   RE::PlayerCharacter* pc = RE::PlayerCharacter::GetSingleton();
   if (!pc) {
      return RE::SOUL_LEVEL::kNone;
   }

   // No soul on the form — check whether the copy in the player's inventory was
   // filled in place, and take the largest soul we can find on it.
   RE::TESObjectREFR::InventoryItemMap inv = pc->GetInventory();
   auto it = inv.find(this->_soulGem);
   if (it == inv.end() || it->second.first <= 0 || !it->second.second) {
      return RE::SOUL_LEVEL::kNone;
   }

   RE::SOUL_LEVEL best = RE::SOUL_LEVEL::kNone;
   const RE::InventoryEntryData* entry = it->second.second.get();
   if (entry->extraLists) {
      for (RE::ExtraDataList* xList : *entry->extraLists) {
      if (!xList) {
        continue;
      }
      if (auto* xSoul = xList->GetByType<RE::ExtraSoul>()) {
        if (xSoul->GetContainedSoul() > best) {
           best = xSoul->GetContainedSoul();
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
   if (this->getAvailableSoul() == RE::SOUL_LEVEL::kNone) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemEmptyWarning));
      return;
   }

   // Check both hands and take the first one that actually needs charge, right
   // hand first. Picking the first *enchanted* weapon instead would report "already
   // charged" for a full main hand while a drained off-hand sat there rechargeable.
   //
   // A weapon that has never been discharged has no ExtraCharge at all, so a
   // missing one means it is already at capacity.
   bool foundEnchanted = false;
   float maxCharge = 0.0f;
   RE::ExtraCharge* xCharge = nullptr;
   for (const bool leftHand : { false, true }) {
      float candidateMax = 0.0f;
      RE::ExtraCharge* candidateCharge = nullptr;
      findEnchantmentCharge(pc->GetEquippedEntryData(leftHand), candidateMax, candidateCharge);
      if (candidateMax <= 0.0f) {
      continue;
      }
      foundEnchanted = true;
      if (candidateCharge && candidateCharge->charge < candidateMax) {
      maxCharge = candidateMax;
      xCharge = candidateCharge;
      break;
      }
   }

   if (!foundEnchanted) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemNoEnchantedWeapon));
      return;
   }
   if (!xCharge) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemWeaponFullyCharged));
      return;
   }

   xCharge->charge = maxCharge;

   // Recharging uses the gem up, as it does in vanilla — except for the reusable
   // ones, which are kept. Note that a reusable gem is left holding its soul, so
   // it can be spent again immediately rather than needing to be refilled first.
   if (!isReusableSoulGem(this->_soulGem)) {
      pc->RemoveItem(this->_soulGem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
   }

   Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemRecharged));
}
