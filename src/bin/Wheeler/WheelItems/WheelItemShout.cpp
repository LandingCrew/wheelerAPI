#include "bin/Rendering/Drawer.h"
#include "bin/Rendering/TextureManager.h"
#include "WheelItemShout.h"
#include "bin/Utilities/Utils.h"
WheelItemShout::WheelItemShout(RE::TESShout* a_shout)
{
   this->_shout = a_shout;
   this->_texture = Texture::GetIconImage(Texture::icon_image_type::shout, a_shout);
}

std::string WheelItemShout::buildDescription(RE::TESObjectREFR::InventoryItemMap& a_imap)
{
   return readBaseDescription(this->_shout);
}

void WheelItemShout::DrawSlot(ImVec2 a_center, bool a_hovered, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs)
{
   this->drawSlotText(a_center, this->_shout->GetName(), a_drawArgs);
   this->drawSlotTexture(a_center, a_drawArgs);
}

void WheelItemShout::DrawHighlight(ImVec2 a_center, RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs)
{
   this->drawHighlightText(a_center, this->_shout->GetName(), a_drawArgs);
   this->drawHighlightTexture(a_center, a_drawArgs);
   const std::string& description = this->getDescription(a_imap);
   if (!description.empty()) {
      this->drawHighlightDescription(a_center, description.data(), a_drawArgs);
   }
}

bool WheelItemShout::IsActive(RE::TESObjectREFR::InventoryItemMap& a_inv)
{
   RE::PlayerCharacter* pc = RE::PlayerCharacter::GetSingleton();
   if (!pc) {
      return false;
   }
   RE::TESForm* selectedPower = pc->GetActorRuntimeData().selectedPower;
   if (selectedPower) {
      return selectedPower->GetFormID() == this->_shout->GetFormID();
   }
   return false;
}

bool WheelItemShout::IsAvailable(RE::TESObjectREFR::InventoryItemMap& a_inv)
{
   return true;
}

//TODO: check if shout's been unlocked, block equipment if not unlocked.
void WheelItemShout::ActivateItemSecondary()
{
   ZoneScoped;
   RE::PlayerCharacter* pc = RE::PlayerCharacter::GetSingleton();
   if (!pc) {
      return;
   }
   RE::ActorEquipManager* aeMan = RE::ActorEquipManager::GetSingleton();
   if (!aeMan) {
      return;
   }
   RE::TESForm* selectedPower = pc->GetActorRuntimeData().selectedPower;
   if (selectedPower && selectedPower->GetFormID() == this->_shout->GetFormID()) {
      //aeMan->UnEquipShout(pc, this->_shout);
      Utils::Slot::UnEquipShout(pc, this->_shout);
   } else {
      aeMan->EquipShout(pc, this->_shout);
   }
}

void WheelItemShout::ActivateItemPrimary()
{
   ZoneScoped;
   RE::PlayerCharacter* pc = RE::PlayerCharacter::GetSingleton();
   if (!pc) {
      return;
   }
   RE::ActorEquipManager* aeMan = RE::ActorEquipManager::GetSingleton();
   if (!aeMan) {
      return;
   }
   RE::TESForm* selectedPower = pc->GetActorRuntimeData().selectedPower;
   if (selectedPower && selectedPower->GetFormID() == this->_shout->GetFormID()) {
      //aeMan->UnEquipShout(pc, this->_shout);
      Utils::Slot::UnEquipShout(pc, this->_shout);
   } else {
      aeMan->EquipShout(pc, this->_shout);
   }
}

void WheelItemShout::ActivateItemSpecial()
{
   ZoneScoped;
}


void WheelItemShout::SerializeIntoJsonObj(nlohmann::json& a_json)
{
   a_json["type"] = WheelItemShout::ITEM_TYPE_STR;
   a_json["formID"] = this->_shout->GetFormID();
}

