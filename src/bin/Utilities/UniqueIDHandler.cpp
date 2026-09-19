#include "UniqueIDHandler.h"
#include "Utils.h"

inline static RE::ExtraDataList* InitExtraDataList(RE::ExtraDataList* a_list)
{
   using func_t = RE::ExtraDataList* (*)(RE::ExtraDataList*);
   REL::Relocation<func_t> func{ RELOCATION_ID(11437, 11583) };  
   return func(a_list);
}

void UniqueIDHandler::EnsureXListUniquenessInPcInventory()
{
   try {
      auto pc = RE::PlayerCharacter::GetSingleton();
      if (!pc) {
      return;
      }
      // Walk InventoryChanges directly rather than GetInventory(): the latter hands back
      // copies of each InventoryEntryData, and while stamping an existing extraDataList
      // writes through the shared pointer, attaching a *new* one to a copy is discarded.
      // These are the real entries, so AddExtraList sticks - same as the hooks in Hooks.cpp.
      auto invChanges = pc->GetInventoryChanges();
      if (!invChanges || !invChanges->entryList) {
      return;
      }

      for (auto& entry : *invChanges->entryList) {
      if (!entry || !entry->object) {
        continue;
      }
      //过滤非武器或非护甲
      auto ft = entry->object->GetFormType();
      if (ft != RE::FormType::Armor && ft != RE::FormType::Weapon) {
        continue;
      }

      auto rawCount = entry->countDelta;

      //处理 ExtraDataLists
      if (entry->extraLists) {
        for (auto& xList : *entry->extraLists) {
           if (xList) {
            rawCount -= xList->GetCount();
            try {
              EnsureXListUniqueness(xList);
            } catch (std::exception& exception) {
              logger::error("Error occured when ensuring extraDataList uniqueness: {}, item: {}",
                exception.what(), entry->object->GetName());
            }
           }
        }
      }

      // Whatever count is left over is a plain stack that has never been equipped,
      // tempered or enchanted, so it carries no extraDataList and therefore no
      // uniqueID for the wheel to match an instance on. Give each one its own list.
      while (rawCount-- > 0) {
        RE::ExtraDataList* xList = nullptr;
        try {
           EnsureXListUniqueness(xList);
           if (xList) {
            entry->AddExtraList(xList);
           }
        } catch (std::exception& exception) {
           logger::error("Error occured when attaching an extraDataList to {}: {}",
            entry->object->GetName(), exception.what());
           break;
        }
      }
      }
   } catch (std::exception& exception) {
      logger::error("Error occured when scanning player inventory extraDataList: {}", exception.what());
   }
}


void UniqueIDHandler::EnsureXListUniqueness(RE::ExtraDataList*& a_extraList)
{
   auto pc = RE::PlayerCharacter::GetSingleton();
   if (!pc) {
      return;
   }
   auto invChanges = pc->GetInventoryChanges();
   if (!invChanges) {
      return;
   }

   if (a_extraList == nullptr) {
      a_extraList = (RE::ExtraDataList*)Utils::Workaround::NiMemAlloc_1400F6B40(24);
      //RE::ExtraDataList::InitExtraDataList(a_extraList);
      InitExtraDataList(a_extraList);
   }

   if (!a_extraList->HasType(RE::ExtraDataType::kUniqueID)) {
      uint16_t nextID = invChanges->GetNextUniqueID();
      auto xID = new RE::ExtraUniqueID(0x14, nextID);
      a_extraList->Add(xID);
   }
}
