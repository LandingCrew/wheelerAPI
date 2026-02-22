#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "Wheel.h"
#include "Wheeler.h"


#include "imgui.h"
#include "imgui_internal.h"

#include "bin/Rendering/Drawer.h"
#include "bin/Utilities/Utils.h"
#include "bin/Texts.h"

#include "WheelItems/WheelItem.h"
#include "WheelItems/WheelItemMutable.h"
#include "WheelItems/WheelItemSpell.h"
#include "WheelItems/WheelItemWeapon.h"

void Wheeler::Update(float a_deltaTime)
{
   std::shared_lock<std::shared_mutex> lock(_wheelDataLock);
   using namespace Config::Styling::Wheel;
   if (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Is3DLoaded()) {
      return;
   }
   // begin draw
   auto ui = RE::UI::GetSingleton();
   if (!ui) {
      return;
   }

   if (_state == WheelState::KClosed) {                  // should close
      if (ImGui::IsPopupOpen(_wheelWindowID)) {         // if it's open, close it
      ImGui::SetNextWindowPos(ImVec2(-100, -100));  // set the pop-up pos to be outside the screen space.
      ImGui::BeginPopup(_wheelWindowID);
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      if (_activeWheelIdx >= 0 && _activeWheelIdx < _wheels.size()) {
        _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry on close
      }
      //ImGui::GetIO().MouseDrawCursor = false;
      if (_editMode) {
        showEditModeVanillaMenus(ui);
      }
      }
      return;
   }
   // state is opened, opening, or closing, draw the wheel with different alphas.

   if (!ImGui::IsPopupOpen(_wheelWindowID)) {  // should open, but not opened yet
      //ImGui::GetIO().MouseDrawCursor = true;
      ImGui::OpenPopup(_wheelWindowID);
      if (_activeWheelIdx >= 0 && _activeWheelIdx < _wheels.size()) {
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry on reopen
      }
      _cursorPos = { 0, 0 };  // reset cursor pos
   }

   //ImGui::GetWindowDrawList()->AddCircleFilled(_cursorPos, 10, ImGuiCol_ButtonHovered, 32);

   ImGui::SetNextWindowPos(ImVec2(-100, -100));  // set the pop-up pos to be outside the screen space.

   if (shouldBeInEditMode(ui)) {
      if (!_editMode) {
      enterEditMode();
      }
      hideEditModeVanillaMenus(ui);
   } else {
      if (_editMode) {
      exitEditMode();
      }
   }

   bool poppedUp = _editMode ? ImGui::BeginPopupModal(_wheelWindowID) : ImGui::BeginPopup(_wheelWindowID);
   if (poppedUp) {
      ImDrawList* drawList = ImGui::GetWindowDrawList();
      drawList->PushClipRectFullScreen();

      // update fade timer, alpha and wheel state.
      _openTimer += a_deltaTime;

      float fadeLerp = 1.0f;
      switch (_state) {
      case WheelState::KOpening:
      fadeLerp = std::fminf(_openTimer / Config::Animation::FadeTime, 1.f);
      if (_openTimer >= Config::Animation::FadeTime) {
        _state = WheelState::KOpened;
      }
      break;
      case WheelState::KClosing:
      _closeTimer += a_deltaTime;
      fadeLerp = std::fmaxf(1 - _closeTimer / Config::Animation::FadeTime, 0.f);
      if (_closeTimer >= Config::Animation::FadeTime) {
        CloseWheeler();
        _closeTimer = 0;
      }
      break;
      }
      
      DrawArgs drawArgs;
      drawArgs.alphaMult = fadeLerp;
      // get ready to draw the wheel

      // lerp wheel center
      ImVec2 wheelCenter = getWheelCenter();
      wheelCenter.y += (1 - fadeLerp) * Config::Animation::ToggleVerticalFadeDistance;
      wheelCenter.x += (1 - fadeLerp) * Config::Animation::ToggleHorizontalFadeDistance;

      RE::TESObjectREFR::InventoryItemMap inv = RE::PlayerCharacter::GetSingleton()->GetInventory();

      /*
      //filter out duplicated FormID items to prevent Clib assertion failure
      RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
      if (!player) {
      return;
      }

      //use filtered inventory to prevent duplicated FormID items
      RE::TESObjectREFR::InventoryItemMap filteredInventory = Utils::Inventory::GetFilteredInventory(player);
      RE::TESObjectREFR::InventoryItemMap& inv = filteredInventory;*/
      

      float cursorAngle = atan2f(_cursorPos.y, _cursorPos.x);  // where the cursor is pointing to

      if (_wheels.empty()) {
      Drawer::draw_text(wheelCenter.x, wheelCenter.y, Texts::GetText(Texts::TextType::NoWheelPresent), C_SKYRIMWHITE, 40.F, drawArgs);
      } else {
      bool isCursorCentered = _cursorPos.x == 0 && _cursorPos.y == 0;
      _wheels[_activeWheelIdx]->Draw(wheelCenter, cursorAngle, isCursorCentered, inv, drawArgs, static_cast<int32_t>(_activeWheelIdx));
      }


      // draw wheel indicator
      for (int i = 0; i < _wheels.size(); i++) {
      bool isWheelActive = i == _activeWheelIdx;
      auto managedStyling = WheelerAPI::GetManagedWheelStyling(static_cast<int32_t>(i));
      bool isManaged = managedStyling.isValid;
      ImVec2 wheelIndicatorPos = { wheelCenter.x + Config::Styling::Wheel::WheelIndicatorOffsetX + i * Config::Styling::Wheel::WheelIndicatorSpacing,
        wheelCenter.y + Config::Styling::Wheel::WheelIndicatorOffsetY };
      if (Config::Styling::Wheel::WheelIndicatorAlignment == Config::WidgetAlignment::kCenter) {  // offset from center
        wheelIndicatorPos.x -= (_wheels.size() - 1) * Config::Styling::Wheel::WheelIndicatorSpacing / 2.f;
      }
      if (!Config::Styling::Wheel::UseGeometricPrimitiveForBackgroundTexture) {
        Texture::Image wheelIndicatorTexture = isWheelActive ? Texture::GetIconImage(Texture::icon_image_type::wheel_indicator_active) : Texture::GetIconImage(Texture::icon_image_type::wheel_indicator_inactive);
        Drawer::draw_texture(wheelIndicatorTexture.texture, wheelIndicatorPos,
           0, 0,
           { Config::Styling::Wheel::WheelIndicatorSize, Config::Styling::Wheel::WheelIndicatorSize },
           C_SKYRIMWHITE, drawArgs);
        // Draw indicator overlay for managed wheels (texture mode)
        if (isManaged && !managedStyling.indicatorText.empty()) {
           Drawer::draw_text(wheelIndicatorPos.x, wheelIndicatorPos.y, managedStyling.indicatorText.c_str(),
            isWheelActive ? managedStyling.indicatorActiveColor : managedStyling.indicatorInactiveColor,
            Config::Styling::Wheel::WheelIndicatorSize * 1.5f, drawArgs);
        }
      } else {
        // Use different color for managed wheels
        ImU32 indicatorColor;
        if (isManaged) {
           indicatorColor = isWheelActive ? managedStyling.indicatorActiveColor : managedStyling.indicatorInactiveColor;
        } else {
           indicatorColor = isWheelActive ? Config::Styling::Wheel::WheelIndicatorActiveColor : Config::Styling::Wheel::WheelIndicatorInactiveColor;
        }
        Drawer::draw_circle_filled(
           wheelIndicatorPos,
           Config::Styling::Wheel::WheelIndicatorSize / 2,
           indicatorColor,
           10, drawArgs);
        // Draw indicator overlay for managed wheels (geometric mode)
        if (isManaged && !managedStyling.indicatorText.empty()) {
           Drawer::draw_text(wheelIndicatorPos.x, wheelIndicatorPos.y, managedStyling.indicatorText.c_str(),
            IM_COL32(0, 0, 0, 255),  // Black text for contrast on colored circle
            Config::Styling::Wheel::WheelIndicatorSize * 1.25f, drawArgs);
        }
      }
      }

      // Draw client name label if active wheel is managed and label is enabled
      if (WheelerAPI::ShouldShowManagedWheelLabel(static_cast<int32_t>(_activeWheelIdx))) {
      auto styling = WheelerAPI::GetManagedWheelStyling(static_cast<int32_t>(_activeWheelIdx));
      std::string clientName = WheelerAPI::GetManagedWheelClientNameSafe(static_cast<int32_t>(_activeWheelIdx));
      if (!clientName.empty() && styling.isValid) {
        ImVec2 labelPos = { wheelCenter.x, wheelCenter.y + Config::Styling::Wheel::WheelIndicatorOffsetY + styling.labelOffsetY };
        // Display clientName directly - client controls full label text
        Drawer::draw_text(labelPos.x, labelPos.y, clientName.c_str(), styling.labelColor, styling.labelFontSize, drawArgs);
      }
      }

      

      drawList->PopClipRect();
      ImGui::EndPopup();
   }
}

void Wheeler::Clear()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (_state != WheelState::KClosed) {
      CloseWheeler();  // force close menu, since we're loading items
   }
   if (_editMode) {
      exitEditMode();
   }
   // clean up old wheels
   for (auto& wheel : _wheels) {
      wheel->Clear();
   }
   _wheels.clear();
}

void Wheeler::ToggleWheeler()
{
   if (_state == WheelState::KClosed) {
      TryOpenWheeler();
   } else {
      TryCloseWheeler();
   }
}

void Wheeler::ToggleWheelIfInInventory()
{
   RE::UI* ui = RE::UI::GetSingleton();
   if (ui && shouldBeInEditMode(ui)) {
      ToggleWheeler();
   }
}

void Wheeler::ToggleWheelIfNotInInventory()
{
   RE::UI* ui = RE::UI::GetSingleton();
   if (ui && !shouldBeInEditMode(ui)) {
      ToggleWheeler();
   }
}

void Wheeler::CloseWheelerIfOpenedLongEnough()
{
   if (_openTimer > Config::Control::Wheel::ToggleHoldThreshold) {
      TryCloseWheeler();
   }
}

void Wheeler::CloseWheelerIfOpenedLongEnoughIfInInventory()
{
   RE::UI* ui = RE::UI::GetSingleton();
   if (ui && shouldBeInEditMode(ui)) {
      CloseWheelerIfOpenedLongEnough();
   }
}

void Wheeler::CloseWheelerIfOpenedLongEnoughIfNotInInventory()
{
   RE::UI* ui = RE::UI::GetSingleton();
   if (ui && !shouldBeInEditMode(ui)) {
      CloseWheelerIfOpenedLongEnough();
   }
}

void Wheeler::TryOpenWheeler()
{
   // here we straight up open the wheel, and set state to opening if we have a fade time.
   // this is because for the fade to start showing the wheel has to be actually fully opened,
   // but to track the state we give it a "opening" state.
   OpenWheeler();
}

void Wheeler::TryCloseWheeler()
{
   if (_state == WheelState::KClosed || _state == WheelState::KClosing) {
      return;
   }
   if (Config::Animation::FadeTime == 0) {
      CloseWheeler();  // close directly
   } else {
      // set timescale to 1 prior to closing the wheel to avoid weirdness
      if (Config::Styling::Wheel::SlowTimeScale <= 1) {
      if (Utils::Time::GGTM() != 1) {
        Utils::Time::SGTM(1);
      }
      }
      _state = WheelState::KClosing;  // set state to closing, will be closed once time out
      _closeTimer = 0;
   }
}

void Wheeler::OpenWheeler()
{
   if (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Is3DLoaded()) {
      return;
   }
   auto ui = RE::UI::GetSingleton();
   if (!ui) {
      return;
   }
   static constexpr std::array<std::string_view, 19> conflictingMenus({
      RE::BookMenu::MENU_NAME,
      RE::BarterMenu::MENU_NAME,
      RE::CraftingMenu::MENU_NAME,
      RE::JournalMenu::MENU_NAME,
      RE::LevelUpMenu::MENU_NAME,
      RE::LockpickingMenu::MENU_NAME,
      RE::LoadingMenu::MENU_NAME,
      RE::MainMenu::MENU_NAME,
      RE::MapMenu::MENU_NAME,
      RE::RaceSexMenu::MENU_NAME,
      RE::SleepWaitMenu::MENU_NAME,
      RE::StatsMenu::MENU_NAME,
      RE::TweenMenu::MENU_NAME,
      RE::Console::MENU_NAME,
      RE::DialogueMenu::MENU_NAME,
      RE::GiftMenu::MENU_NAME,
      RE::ModManagerMenu::MENU_NAME,
      RE::ContainerMenu::MENU_NAME,
      "LootMenu" // quick loot
   });
   for (std::string_view menuName : conflictingMenus) {
      if (ui->IsMenuOpen(menuName) 
      && !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) 
      && !ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
      return;
      }
   }
   
   if (_state != WheelState::KOpened && _state != WheelState::KOpening) {
      if (Config::Styling::Wheel::SlowTimeScale < 1) {
      if (Utils::Time::GGTM() == 1) {
        Utils::Time::SGTM(Config::Styling::Wheel::SlowTimeScale);
      }
      }
      if (Config::Styling::Wheel::BlurOnOpen) {
      RE::UIBlurManager::GetSingleton()->IncrementBlurCount();
      }
      if (_activeWheelIdx >= 0 && _activeWheelIdx < _wheels.size()) {
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry on OPEN
      _wheels[_activeWheelIdx]->ResetAnimation();
      }
      _state = Config::Animation::FadeTime > 0 ? WheelState::KOpening : WheelState::KOpened;
      _openTimer = 0;
#undef PlaySound

      RE::PlaySound(Config::Sound::SD_WHEELERTOGGLE);

#ifdef UNICODE
#   define PlaySound PlaySoundW
#else
#   define PlaySound PlaySoundA
#endif  // !UNICODE

      // Log if opening with a managed wheel active
      std::string clientName = WheelerAPI::GetManagedWheelClientNameSafe(static_cast<int32_t>(_activeWheelIdx));
      if (!clientName.empty()) {
      INFO("Wheeler: Opened with managed wheel {} (client: {}) active", _activeWheelIdx, clientName);
      }

      // Notify API clients that wheel opened
      WheelerAPI::NotifyWheelStateChanged(static_cast<int32_t>(_activeWheelIdx), true);
   }
}

void Wheeler::CloseWheeler()
{
   if (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Is3DLoaded()) {
      return;
   }
   bool wasOpen = (_state != WheelState::KClosed);
   if (_state != WheelState::KClosed) {
      if (Config::Styling::Wheel::SlowTimeScale <= 1) {
      if (Utils::Time::GGTM() != 1) {
        Utils::Time::SGTM(1);
      }
      }
      if (Config::Styling::Wheel::BlurOnOpen) {
      RE::UIBlurManager::GetSingleton()->DecrementBlurCount();
      }
      if (_activeWheelIdx >= 0 && _activeWheelIdx < _wheels.size()) {
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry on close
      _wheels[_activeWheelIdx]->ResetAnimation();
      }
      _openTimer = 0;
      _closeTimer = 0;
   }
   _state = WheelState::KClosed;

   // Notify API clients that wheel closed
   if (wasOpen) {
      WheelerAPI::NotifyWheelStateChanged(static_cast<int32_t>(_activeWheelIdx), false);
   }
}

void Wheeler::UpdateCursorPosMouse(float a_deltaX, float a_deltaY)
{
   if (_state == WheelState::KClosed) {
      return;
   }
   ImVec2 newPos = _cursorPos + ImVec2{ a_deltaX, a_deltaY };
   // Calculate the distance from the wheel center to the new cursor position
   float distanceFromCenter = sqrt(newPos.x * newPos.x + newPos.y * newPos.y);

   // If the distance exceeds the cursor radius, adjust the cursor position
   float cursorRadius = getCursorRadiusMax();
   if (distanceFromCenter > cursorRadius) {
      // Calculate the normalized direction vector from the center to the new position
      ImVec2 direction = newPos / distanceFromCenter;

      // Set the cursor position at the edge of the cursor radius
      newPos = direction * cursorRadius;
   }

   _cursorPos = newPos;
}

void Wheeler::UpdateCursorPosGamepad(float a_x, float a_y)
{
   if (_state == WheelState::KClosed) {
      return;
   }
   RE::PlayerControls* controls = RE::PlayerControls::GetSingleton();
   
   #define CONTROLLER_DEADZONE 0.1f
   if (abs(a_x) <= CONTROLLER_DEADZONE && abs(a_y) <= CONTROLLER_DEADZONE) {
      return;
   }
   float cursorRadius = getCursorRadiusMax();
   _cursorPos.x = a_x * cursorRadius;
   _cursorPos.y = -a_y * cursorRadius;
}

void Wheeler::NextWheel()
{
   if (_state == WheelState::KOpened) {
      if (_wheels.empty()) {
      return;
      }
      _cursorPos = { 0, 0 };
      if (_activeWheelIdx >= 0) {
      _wheels[_activeWheelIdx]->ResetAnimation();
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1); // reset active entry for current wheel
      }

      // In edit mode, skip managed wheels
      int startIdx = _activeWheelIdx;
      do {
      _activeWheelIdx += 1;
      if (_activeWheelIdx >= static_cast<int>(_wheels.size())) {
        _activeWheelIdx = 0;
      }
      // If not in edit mode, accept any wheel; otherwise skip managed wheels
      if (!_editMode || !WheelerAPI::IsManagedWheelIndex(_activeWheelIdx)) {
        break;
      }
      } while (_activeWheelIdx != startIdx);  // Prevent infinite loop

      _wheels[_activeWheelIdx]->ResetAnimation();
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry for new wheel

      // Log if switched to a managed wheel
      std::string clientName = WheelerAPI::GetManagedWheelClientNameSafe(static_cast<int32_t>(_activeWheelIdx));
      if (!clientName.empty()) {
      INFO("Wheeler: Switched to managed wheel {} (client: {})", _activeWheelIdx, clientName);
      }

      if (_wheels.size() > 1) {
#undef PlaySound
      RE::PlaySound(Config::Sound::SD_WHEELSWITCH);
#ifdef UNICODE
#   define PlaySound PlaySoundW
#else
#   define PlaySound PlaySoundA
#endif  // !UNICODE
      }
   }
}

void Wheeler::PrevWheel()
{
   if (_state == WheelState::KOpened) {
      if (_wheels.empty()) {
      return;
      }
      _cursorPos = { 0, 0 };
      if (_activeWheelIdx >= 0) {
      _wheels[_activeWheelIdx]->ResetAnimation();
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1); // reset active entry for current wheel
      }

      // In edit mode, skip managed wheels
      int startIdx = _activeWheelIdx;
      do {
      _activeWheelIdx -= 1;
      if (_activeWheelIdx < 0) {
        _activeWheelIdx = static_cast<int>(_wheels.size()) - 1;
      }
      // If not in edit mode, accept any wheel; otherwise skip managed wheels
      if (!_editMode || !WheelerAPI::IsManagedWheelIndex(_activeWheelIdx)) {
        break;
      }
      } while (_activeWheelIdx != startIdx);  // Prevent infinite loop

      _wheels[_activeWheelIdx]->ResetAnimation();
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry for new wheel

      // Log if switched to a managed wheel
      std::string clientName = WheelerAPI::GetManagedWheelClientNameSafe(static_cast<int32_t>(_activeWheelIdx));
      if (!clientName.empty()) {
      INFO("Wheeler: Switched to managed wheel {} (client: {})", _activeWheelIdx, clientName);
      }

      if (_wheels.size() > 1) {
#undef PlaySound

      RE::PlaySound(Config::Sound::SD_WHEELSWITCH);

#ifdef UNICODE
#   define PlaySound PlaySoundW
#else
#   define PlaySound PlaySoundA
#endif  // !UNICODE
      }
   }
}

void Wheeler::PrevItemInEntry()
{
   if (_state == WheelState::KOpened) {
      _wheels[_activeWheelIdx]->PrevItemInHoveredEntry();
   }
}

void Wheeler::NextItemInEntry()
{
   if (_state == WheelState::KOpened) {
      _wheels[_activeWheelIdx]->NextItemInHoveredEntry();
   }
}

bool Wheeler::GetCursorAngleRadian(float& r_ret)
{
   if (_cursorPos.x != 0 || _cursorPos.y != 0) {
      r_ret = atan2(_cursorPos.y, _cursorPos.x);
      return true;
   }
   return false;
}



void Wheeler::ActivateHoveredEntrySecondary()
{
   if (_wheels.empty()) {
      return;
   }
   if (_state == WheelState::KOpened) {
      Wheel* activeWheel = _wheels[_activeWheelIdx].get();

      // Block edit operations on managed wheels
      bool isManaged = WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx));
      DEBUG("Wheeler::ActivateHoveredEntrySecondary: wheelIdx={}, editMode={}, isManaged={}",
      _activeWheelIdx, _editMode, isManaged);
      if (_editMode && isManaged) {
      INFO("Wheeler: Blocked edit operation on managed wheel {}", _activeWheelIdx);
      return;
      }

      if (activeWheel->IsEmpty()) {         // empty wheel, we can only delete in edit mode.
      if (_editMode && _wheels.size() > 1) {  // we have more than one wheel, so it's safe to delete this one.
        DeleteCurrentWheel();
      }
      } else {
      // Capture item info before activation for callback (only if not in edit mode)
      int32_t entryIndex = -1;
      int32_t itemIndex = -1;
      uint32_t formID = 0;

      if (!_editMode) {
        entryIndex = activeWheel->GetHoveredEntryIndex();
        if (entryIndex >= 0) {
           WheelEntry* entry = activeWheel->GetEntry(entryIndex);
           if (entry && !entry->IsEmpty()) {
            itemIndex = entry->GetSelectedItemIndex();
            WheelItem* item = entry->GetItem(itemIndex);
            if (item) {
              formID = item->GetFormID();
            }
           }
        }
      }

      activeWheel->ActivateHoveredEntrySecondary(_editMode);

      // Notify callback if we had valid item info (not in edit mode)
      if (!_editMode && formID != 0) {
        WheelerAPI::NotifyItemActivated(
           static_cast<int32_t>(_activeWheelIdx),
           entryIndex,
           itemIndex,
           formID,
           false  // isPrimary = false for secondary
        );
      }
      }
   }
}

void Wheeler::ActivateHoveredEntryPrimary()
{
   if (_wheels.empty()) {
      return;
   }
   if (_state == WheelState::KOpened) {
      Wheel* activeWheel = _wheels[_activeWheelIdx].get();

      // Capture item info before activation for callback (only if not in edit mode)
      int32_t entryIndex = -1;
      int32_t itemIndex = -1;
      uint32_t formID = 0;

      if (!_editMode) {
      entryIndex = activeWheel->GetHoveredEntryIndex();
      if (entryIndex >= 0) {
        WheelEntry* entry = activeWheel->GetEntry(entryIndex);
        if (entry && !entry->IsEmpty()) {
           itemIndex = entry->GetSelectedItemIndex();
           WheelItem* item = entry->GetItem(itemIndex);
           if (item) {
            formID = item->GetFormID();
           }
        }
      }
      }

      activeWheel->ActivateHoveredEntryPrimary(_editMode);

      // Notify callback if we had valid item info (not in edit mode)
      if (!_editMode && formID != 0) {
      WheelerAPI::NotifyItemActivated(
        static_cast<int32_t>(_activeWheelIdx),
        entryIndex,
        itemIndex,
        formID,
        true  // isPrimary
      );
      }
   }
}

void Wheeler::ActivateHoveredEntrySpecial()
{
   if (_wheels.empty()) {
      return;
   }
   if (_state == WheelState::KOpened) {
      _wheels[_activeWheelIdx]->ActivateHoveredEntrySpecial(_editMode);
   }
}

void Wheeler::AddEmptyEntryToCurrentWheel()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed || _wheels.empty() || _activeWheelIdx == -1) {
      return;
   }
   // Block edit operations on managed wheels
   if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx))) {
      INFO("Wheeler: Blocked AddEmptyEntry on managed wheel {}", _activeWheelIdx);
      return;
   }
   _wheels[_activeWheelIdx]->PushEmptyEntry();
}


void Wheeler::AddWheel()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed) {
      return;
   }
   _wheels.push_back(std::make_unique<Wheel>());
}

void Wheeler::PushWheel()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   _wheels.push_back(std::make_unique<Wheel>());
}

void Wheeler::DeleteCurrentWheel()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed) {
      return;
   }
   // Block deletion of managed wheels
   if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx))) {
      INFO("Wheeler: Blocked DeleteCurrentWheel on managed wheel {}", _activeWheelIdx);
      return;
   }
   if (_wheels.size() > 1) {
      std::unique_ptr<Wheel>& toDelete = _wheels[_activeWheelIdx];
      if (!toDelete->IsEmpty()) { // do not delete an non-empty wheel
      return;
      }
      _wheels.erase(_wheels.begin() + _activeWheelIdx);
      if (_activeWheelIdx == _wheels.size() && _activeWheelIdx != 0) {  //deleted the last wheel
      _activeWheelIdx = _wheels.size() - 1;
      }
      _wheels[_activeWheelIdx]->SetHoveredEntryIndex(-1);  // reset active entry for new wheel
   }
}

void Wheeler::MoveEntryForwardInCurrentWheel()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed) {
      return;
   }
   // Block edit operations on managed wheels
   if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx))) {
      return;
   }
   if (_activeWheelIdx != -1) {
      _wheels[_activeWheelIdx]->MoveHoveredEntryForward();
   }
}

void Wheeler::MoveEntryBackInCurrentWheel()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed) {
      return;
   }
   // Block edit operations on managed wheels
   if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx))) {
      return;
   }
   if (_activeWheelIdx != -1) {
      _wheels[_activeWheelIdx]->MoveHoveredEntryBack();
   }
}

void Wheeler::MoveWheelForward()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed) {
      return;
   }
   // Block reordering of managed wheels
   if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx))) {
      return;
   }
   if (_wheels.size() > 1) {
      int targetIdx;
      if (_activeWheelIdx == _wheels.size() - 1) {
      // Move the current wheel to the very front
      targetIdx = 0;
      auto currentWheel = std::move(_wheels.back());
      _wheels.pop_back();
      _wheels.insert(_wheels.begin(), std::move(currentWheel));
      } else {
      targetIdx = _activeWheelIdx + 1;
      std::swap(_wheels[_activeWheelIdx], _wheels[targetIdx]);
      }
      _activeWheelIdx = targetIdx;
   }
}

void Wheeler::MoveWheelBack()
{
   std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
   if (!_editMode || _state == WheelState::KClosed) {
      return;
   }
   // Block reordering of managed wheels
   if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(_activeWheelIdx))) {
      return;
   }
   if (_wheels.size() > 1) {
      int targetIdx;
      if (_activeWheelIdx == 0) {
      // Move the current wheel to the very end
      targetIdx = _wheels.size() - 1;
      auto currentWheel = std::move(_wheels.front());
      _wheels.erase(_wheels.begin());
      _wheels.push_back(std::move(currentWheel));
      _activeWheelIdx = _wheels.size() - 1;
      } else {
      targetIdx = _activeWheelIdx - 1;
      std::swap(_wheels[_activeWheelIdx], _wheels[targetIdx]);
      }
      _activeWheelIdx = targetIdx;
   }
}

int Wheeler::GetActiveWheelIndex()
{
   return _activeWheelIdx;
}

void Wheeler::SetActiveWheelIndex(int a_index)
{
   _activeWheelIdx = a_index;
}

bool Wheeler::IsWheelerOpen() { return _state != WheelState::KClosed; }

bool Wheeler::IsInEditMode() { return _editMode; }

void Wheeler::SerializeFromJsonObj(const nlohmann::json& j_wheeler, SKSE::SerializationInterface* a_intfc)
{
   nlohmann::json j_wheels = j_wheeler["wheels"];
   for (const auto& j_wheel : j_wheels) {
      std::unique_ptr<Wheel> wheel = Wheel::SerializeFromJsonObj(j_wheel, a_intfc);
      _wheels.push_back(std::move(wheel));
   }
   SetActiveWheelIndex(j_wheeler["activewheel"]);
}

void Wheeler::SerializeIntoJsonObj(nlohmann::json& j_wheeler)
{
   j_wheeler["wheels"] = nlohmann::json::array();
   int32_t savedWheelCount = 0;
   int32_t adjustedActiveIdx = _activeWheelIdx;

   for (size_t i = 0; i < _wheels.size(); i++) {
      // Skip managed wheels - they are owned by external clients
      if (WheelerAPI::IsManagedWheelIndex(static_cast<int32_t>(i))) {
      // If active wheel is after this managed wheel, adjust index
      if (static_cast<int32_t>(i) < _activeWheelIdx) {
        adjustedActiveIdx--;
      }
      continue;
      }

      nlohmann::json j_wheel;
      _wheels[i]->SerializeIntoJsonObj(j_wheel);
      j_wheeler["wheels"].push_back(j_wheel);
      savedWheelCount++;
   }

   // Clamp adjusted active index to valid range
   if (adjustedActiveIdx < 0) {
      adjustedActiveIdx = 0;
   } else if (savedWheelCount > 0 && adjustedActiveIdx >= savedWheelCount) {
      adjustedActiveIdx = savedWheelCount - 1;
   }

   j_wheeler["activewheel"] = adjustedActiveIdx;
}

void Wheeler::SetupDefaultWheels()
{
   const int defaultWheelNum = 2;
   const int defaultEntryNum = 4;
   Wheeler::Clear();
   int wheelIdx = 0;
   while (wheelIdx < defaultWheelNum) {
      Wheeler::PushWheel();
      int entryIdx = 0;
      while (entryIdx < defaultEntryNum) {
      _wheels[wheelIdx]->PushEmptyEntry();
      entryIdx++;
      }
      wheelIdx++;
   }
   Wheeler::SetActiveWheelIndex(0);

}

inline ImVec2 Wheeler::getWheelCenter()
{
   using namespace Config::Styling::Wheel;
   return ImVec2(ImGui::GetIO().DisplaySize.x / 2 + CenterOffsetX, ImGui::GetIO().DisplaySize.y / 2 + CenterOffsetY);
}

bool Wheeler::shouldBeInEditMode(RE::UI* a_ui)
{
   return a_ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || a_ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);
}

void Wheeler::hideEditModeVanillaMenus(RE::UI* a_ui)
{
   if (!Config::Control::Wheel::HideGameUIInEditMode) {
      return; // don't hide
   }
   if (a_ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
      RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::InventoryMenu>()->uiMovie.get();
      if (uiMovie) {
      uiMovie->SetVisible(false);
      }
   }
   if (a_ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
      RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::MagicMenu>()->uiMovie.get();
      if (uiMovie) {
      uiMovie->SetVisible(false);
      }
   }
}

void Wheeler::showEditModeVanillaMenus(RE::UI* a_ui)
{
   if (a_ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
      RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::InventoryMenu>()->uiMovie.get();
      if (uiMovie) {
      uiMovie->SetVisible(true);
      }
   }
   if (a_ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
      RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::MagicMenu>()->uiMovie.get();
      if (uiMovie) {
      uiMovie->SetVisible(true);
      }
   }
}

void Wheeler::enterEditMode()
{
   if (_editMode) {
      return;
   }
   _editMode = true;
   WheelerAPI::NotifyEditModeChanged(true, nullptr, 0);
}

void Wheeler::exitEditMode()
{
   if (!_editMode) {
      return;
   }
   _editMode = false;
   // TODO: Track changes during edit mode and pass them here
   WheelerAPI::NotifyEditModeChanged(false, nullptr, 0);
}

float Wheeler::getCursorRadiusMax()
{
   if (_activeWheelIdx == -1 || _wheels.empty()) {
      return 0.0f;
   }
   return Config::Control::Wheel::CursorRadiusPerEntry * _wheels[_activeWheelIdx]->GetNumEntries();
}

Wheel* Wheeler::GetWheelByIndex(int a_index)
{
   if (a_index < 0 || a_index >= static_cast<int>(_wheels.size())) {
      return nullptr;
   }
   return _wheels[a_index].get();
}

// bool Wheeler::OffsetCamera(RE::TESCamera* a_this)
// {
//    using namespace Config::Animation;
//    if (!CameraRotation || _state == WheelState::KClosed || (_cursorPos.x == 0 && _cursorPos.y == 0)) {
//       return false;
//    }
//    float cursorRadius = getCursorRadiusMax();
//    // Calculate yaw rotation matrix
//    RE::NiMatrix3 yawRotation = Utils::Math::MatrixFromAxisAngle(-_cursorPos.x / cursorRadius * 0.05, Utils::Math::HORIZONTAL_AXIS);
//    // Set roll component to zero
//    yawRotation.entry[3][3] = 1.0f;

//    // Calculate pitch rotation matrix
//    RE::NiMatrix3 pitchRotation = Utils::Math::MatrixFromAxisAngle(-_cursorPos.y / cursorRadius * 0.05, Utils::Math::VERTICAL_AXIS);
//    // Set roll component to zero
//    pitchRotation.entry[3][3] = 1.0f;

//    // Apply rotations to the camera root's local rotation matrix
//    a_this->cameraRoot->local.rotate = a_this->cameraRoot->local.rotate * yawRotation;
//    a_this->cameraRoot->local.rotate = a_this->cameraRoot->local.rotate * pitchRotation;

//    return true;

//    return true;
// }
