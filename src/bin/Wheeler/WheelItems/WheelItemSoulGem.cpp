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

   // What a hand is actually holding, taken straight off the actor's process
   // instead of searching the inventory for it.
   //
   // A two-handed weapon lives in bothHands and is reported for neither hand slot,
   // which is why GetEquippedObject(true) misses it entirely. It charges through
   // the right-hand actor value, so it is reported here for the right hand only.
   RE::InventoryEntryData* equippedEntry(RE::PlayerCharacter* a_pc, bool a_leftHand)
   {
      auto* process = a_pc ? a_pc->GetActorRuntimeData().currentProcess : nullptr;
      auto* middleHigh = process ? process->middleHigh : nullptr;
      if (!middleHigh) {
      return nullptr;
      }
      if (middleHigh->bothHands) {
      return a_leftHand ? nullptr : middleHigh->bothHands;
      }
      return a_leftHand ? middleHigh->leftHand : middleHigh->rightHand;
   }

   // Capacity of a player-applied enchantment, which lives on the worn stack's
   // ExtraEnchantment rather than on the base form.
   //
   // Reading it off the hand's own entry is what keeps the player's inventory
   // untouched here. An earlier revision searched InventoryChanges::entryList for
   // the worn stack, and recharging then crashed reproducibly on the following
   // frame, in Wheeler::Update where GetInventory() copies that same list. The
   // gate that was meant to keep the search rare could not work: a plain sword
   // and a player-enchanted one are indistinguishable on the form, so every
   // ordinary weapon fell through to it.
   //
   // The entry is already the one stack the hand holds, so the ExtraWorn filter
   // the inventory search needed to pick the worn copy out is no longer required.
   float wornEnchantmentCapacity(RE::InventoryEntryData* a_entry)
   {
      if (!a_entry || !a_entry->extraLists) {
      return 0.0f;
      }

      for (RE::ExtraDataList* xList : *a_entry->extraLists) {
      if (!xList) {
        continue;
      }
      auto* xEnch = xList->GetByType<RE::ExtraEnchantment>();
      if (xEnch && xEnch->enchantment && xEnch->charge != 0) {
        return static_cast<float>(xEnch->charge);
      }
      }
      return 0.0f;
   }

   // Charge capacity of the enchanted weapon held in one hand, or 0 if that hand
   // holds nothing rechargeable.
   //
   // A base-enchanted weapon, and a staff, declare capacity on the form. A weapon
   // the player enchanted themselves declares nothing there at all -- formEnchanting
   // is null and amountofEnchantment is 0, exactly as a plain weapon does -- and
   // carries it on the worn stack instead, so the form cannot be used to rule the
   // weapon out. Both cases now cost the same lookup.
   float equippedWeaponCapacity(RE::PlayerCharacter* a_pc, bool a_leftHand)
   {
      RE::InventoryEntryData* entry = equippedEntry(a_pc, a_leftHand);
      auto* weapon = entry && entry->object ? entry->object->As<RE::TESObjectWEAP>() : nullptr;
      auto* enchantable = weapon ? weapon->As<RE::TESEnchantableForm>() : nullptr;
      if (!enchantable) {
      return 0.0f;
      }

      const bool formDeclaresCharge =
      enchantable->formEnchanting || weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff;
      if (formDeclaresCharge && enchantable->amountofEnchantment != 0) {
      return static_cast<float>(enchantable->amountofEnchantment);
      }

      return wornEnchantmentCapacity(entry);
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

   // Touch the player's inventory as little as possible. This runs from the input
   // path while Wheeler::Update is copying the whole inventory every frame in
   // GetInventory(), and a revision that walked the inventory changes list on every
   // activation crashed reproducibly in that copy on the following frame.
   //
   // The only thing worth asking of the gem is whether there is a soul in it at
   // all. How much charge a soul is worth is the game's bookkeeping, not
   // Wheeler's, so a spent gem restores the weapon to full.
   //
   // This is a design decision, not a gap: wheeler is a UI/UX tool, not an optimization tool. 
   //
   // soulHolder pins the stack the soul came from, and stays null for vanilla
   // filled gems, whose soul is on the form and whose copies are interchangeable.
   RE::ExtraDataList* soulHolder = nullptr;
   if (this->getAvailableSoul(&soulHolder) == RE::SOUL_LEVEL::kNone) {
      Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemEmptyWarning));
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
   RE::ActorValue chargeAV = RE::ActorValue::kNone;
   float maxCharge = 0.0f;
   float current = 0.0f;
   for (const auto& hand : { std::pair{ false, RE::ActorValue::kRightItemCharge },
           std::pair{ true, RE::ActorValue::kLeftItemCharge } }) {
      const float capacity = equippedWeaponCapacity(pc, hand.first);
      if (capacity <= 0.0f) {
      continue;
      }
      foundEnchanted = true;
      const float charge = avOwner->GetActorValue(hand.second);
      if (charge < capacity) {
      chargeAV = hand.second;
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

   if (isReusableSoulGem(this->_soulGem)) {
      // A reusable gem survives but is emptied, as in vanilla, so it has to be
      // refilled before it works again. Without this the artifacts recharge for
      // free, which is the whole reason they are worth carrying.
      //
      // The soul is cleared in place rather than removed. ExtraDataList's removal
      // APIs are both unsuitable: RemoveByType dereferences null inside
      // CommonLibSSE-NG once the list drains empty -- reachable here, since a gem
      // filled in place can hold nothing but an ExtraSoul -- and Remove() unlinks
      // the node without freeing it. Assigning kNone is one store, and
      // ExtraDataList::GetSoulLevel() reads straight off this field, so the engine
      // sees an empty gem.
      //
      // soulHolder is the stack getAvailableSoul() read the soul from, and the gem
      // reaches this branch only when its soul lives in ExtraSoul, so there is
      // always one. It is used here exactly as the RemoveItem below uses it, with
      // the same lifetime.
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

   // Deliberately NOT writing the weapon's ExtraCharge to match. That was added to
   // stop a recharge being undone on unequip or reload -- a problem never actually
   // observed -- and it meant reaching into extra data through a list resolved by
   // walking the inventory, which is the behaviour this crash was traced to. The
   // actor value is what the game reads, and it is enough on its own.
   avOwner->ModActorValue(chargeAV, maxCharge - current);

   DEBUG("WheelerAPI: Recharged {} hand: {} -> {}",
      chargeAV == RE::ActorValue::kRightItemCharge ? "right" : "left", current, maxCharge);

   Utils::NotificationMessage(Texts::GetText(Texts::TextType::SoulGemRecharged));
}
