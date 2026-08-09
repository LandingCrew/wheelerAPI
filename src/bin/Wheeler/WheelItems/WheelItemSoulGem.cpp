#include "bin/Rendering/TextureManager.h"
#include "bin/Rendering/Drawer.h"
#include "bin/Utilities/Utils.h"
#include "bin/Texts.h"
#include "WheelItemSoulGem.h"

namespace
{
   // Charge restored per soul, matching vanilla's soul sizes:
   // petty 250, lesser 500, common 1000, greater 2000, grand 3000. These are not
   // exposed as game settings, so they are reproduced here — tune them if a mod
   // in your load order rebalances soul sizes.
   float soulChargeValue(RE::SOUL_LEVEL a_soul)
   {
      switch (a_soul) {
      case RE::SOUL_LEVEL::kPetty:
      return 250.0f;
      case RE::SOUL_LEVEL::kLesser:
      return 500.0f;
      case RE::SOUL_LEVEL::kCommon:
      return 1000.0f;
      case RE::SOUL_LEVEL::kGreater:
      return 2000.0f;
      case RE::SOUL_LEVEL::kGrand:
      return 3000.0f;
      default:
      return 0.0f;
      }
   }

   const char* soulLevelName(RE::SOUL_LEVEL a_soul)
   {
      switch (a_soul) {
      case RE::SOUL_LEVEL::kPetty:
      return "Petty";
      case RE::SOUL_LEVEL::kLesser:
      return "Lesser";
      case RE::SOUL_LEVEL::kCommon:
      return "Common";
      case RE::SOUL_LEVEL::kGreater:
      return "Greater";
      case RE::SOUL_LEVEL::kGrand:
      return "Grand";
      default:
      return "";
      }
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

   if (a_soulGem) {
      const char* capacity = soulLevelName(a_soulGem->GetMaximumCapacity());
      const RE::SOUL_LEVEL contained = a_soulGem->GetContainedSoul();
      if (contained != RE::SOUL_LEVEL::kNone) {
      this->_description = fmt::format("{} / {}", soulLevelName(contained), capacity);
      } else {
      this->_description = fmt::format("{} ({})", capacity, Texts::GetText(Texts::TextType::SoulGemEmpty));
      }
   }
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

   const float soulCharge = soulChargeValue(this->getAvailableSoul());
   if (soulCharge <= 0.0f) {
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

   // Parenthesised to keep the Windows min macro from eating the call.
   xCharge->charge = (std::min)(xCharge->charge + soulCharge, maxCharge);

   // Recharging consumes the gem, as it does in vanilla. Reusable gems such as
   // Azura's Star are driven by their own quest scripts and are not special-cased
   // here — they will be consumed like any other gem.
   pc->RemoveItem(this->_soulGem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

   Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemRecharged));
}
