#pragma once
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include "nlohmann/json.hpp"
#include "imgui.h"

#include "bin/Config.h"
#include "bin/API/WheelerAPI.h"
#include "Wheel.h"
class Wheeler
{
public:
   static void Init()
   {
      // insert an empty wheel
      _wheels.emplace_back(std::make_unique<Wheel>());
      WheelerAPI::SetInitialized(true);
   }

   /// <summary>
   /// Update wheeler, calling draw function etc...
   /// This function can only be invoked by the renderer.
   /// </summary>
   static void Update(float a_deltaTime);
   
   /// <summary>
   /// Resets the user's wheels, freeing them and their members in the hierarchy.
   /// Must be called prior to reloading the wheels.
   ///
   /// Client-managed wheels (created through WheelerAPI) are deliberately NOT
   /// destroyed: they are owned by the client that created them and are excluded
   /// from the co-save by SerializeIntoJsonObj, so wiping one here would be
   /// unrecoverable — there is nothing left to deserialize it back from. Clients
   /// drop their own wheels with DeleteManagedWheelsForClient().
   /// </summary>
   static void Clear();

   static void UpdateCursorPosMouse(float a_deltaX, float a_deltaY);
   static void UpdateCursorPosGamepad(float a_x, float a_y);

   static void ToggleWheeler();
   static void ToggleWheelIfInInventory();
   static void ToggleWheelIfNotInInventory();

   /// <summary>
   /// Close the current wheel, if it's been opened long enough(more than 0.2 seconds).
   /// This collaborates with `Controls` that invokes this function when the user releases the wheel toggle key.
   /// Correct usage of this function and ToggleWheeler() ensures the following behavior:
   ///    When the user presses down the toggle key for the first time, wheeler opens.
   ///      If the user immediately releases the key, wheeler stays open, until the user presses the toggle key again.
   ///      If the user keeps pressing the toggle key for a while and then releases the key, wheeler automatically closes.
   /// This allows the toggle key to simultaneously act as a press-open, press-close toggle, and a hold-open, release-close button.
   /// </summary>
   static void CloseWheelerIfOpenedLongEnough();
   static void CloseWheelerIfOpenedLongEnoughIfInInventory();
   static void CloseWheelerIfOpenedLongEnoughIfNotInInventory();

   
   static void TryOpenWheeler();
   static void TryCloseWheeler();

   /// <summary>
   /// Ask for the wheel to close on the next update, without taking the
   /// wheel-data lock. This is what item activation must use: it already runs
   /// under the lock, so calling TryCloseWheeler() from there would re-enter a
   /// mutex that is not recursive.
   /// </summary>
   static void RequestClose();

   static void OpenWheeler();
   static void CloseWheeler();
   
   static void NextWheel();
   static void PrevWheel();
   static void PrevItemInEntry();
   static void NextItemInEntry();

   static bool GetCursorAngleRadian(float& r_ret);

   /// <summary>
   /// Offset camera rotation with current cusor position. Returns whether a change has been made to the camera's rotation.
   /// </summary>
   static bool OffsetCamera(RE::TESCamera* a_this) = delete;

   /// <summary>
   /// Activate the currently active entry with secondary (left) input, which corresponds to right mouse click or left controller trigger.
   /// If we're in edit mode:
   ///  - the function first checks if there's any entry left. If not, the function calls DeleteCurrentWheel(), given there are more than 1 wheel present(must have at least 1 wheel on stack).
   ///  - if there's some entry left, the function calls the current wheel's ActivateItemSecondary(), which handles deletion of entry items, or the entry via subsequent calls.
   /// If we're not in edit mode, the entry calls the current wheel's ActivateItemSecondary()
   /// </summary>
   static void ActivateHoveredEntrySecondary();

   /// <summary>
   /// Activate the currently active entry with primary (right) input, which corresponds to left mouse click or right controller trigger,
   /// The function simply invokes current entry's ActivateItemPrimary(), which either handles activation of items or addition of items, if in edit mode.
   /// </summary>
   static void ActivateHoveredEntryPrimary();

   static void ActivateHoveredEntrySpecial();

   /// <summary>
   /// Push an empty entry to the current wheel.
   /// </summary>
   static void AddEmptyEntryToCurrentWheel();

   /// <summary>
   /// Add a new empty wheel to the set of wheels.
   /// Wheel is added only if the user is in edit mode.
   /// </summary>
   static void AddWheel();
   
   /// <summary>
   /// Push a new empty wheel to the set of wheels.
   /// Doesn't check for edit mode.
   /// </summary>
   static void PushWheel();

   /// <summary>
   /// Delete the current wheel. The deletion may be performed if and only if the current wheel is empty, and the current wheel is not the last wheel present.
   /// The caller is responsible for checking the wheel's emptiness.
   /// </summary>
   static void DeleteCurrentWheel();

   /// <summary>
   /// Move the currently active entry forward by one in the current wheel. Only available in edit mode.
   /// </summary>
   static void MoveEntryForwardInCurrentWheel();
   /// <summary>
   /// Move the currently active entry backward by one in the current wheel. Only available in edit mode.
   /// </summary>
   static void MoveEntryBackInCurrentWheel();
   
   /// <summary>
   /// Move the currently active wheel forward by one in the set of wheels. Only available in edit mode.
   /// </summary>
   static void MoveWheelForward();
   /// <summary>
   /// Move the currently active wheel backward by one in the set of wheels. Only available in edit mode.
   /// </summary>
   static void MoveWheelBack();
   
   static int GetActiveWheelIndex();
   static void SetActiveWheelIndex(int a_index);
   

   static bool IsWheelerOpen();
   static bool IsInEditMode();

   /// <summary>
   /// Replace the user's wheels with the ones described by a_json, as a single
   /// atomic step under the wheel-data lock. Splitting this into a separate
   /// Clear() plus deserialize would leave a window in which a client calling
   /// CreateManagedWheel() from another thread races the repopulate on _wheels.
   /// Managed wheels survive and keep their relative order at the front.
   /// </summary>
   static void ReloadFromJsonObj(const nlohmann::json& a_json, SKSE::SerializationInterface* a_intfc);
   static void SerializeIntoJsonObj(nlohmann::json& a_json);

   /// <summary>
   /// Set up 2 wheels, each with 4 empty slots.
   /// Used to create a template when a user starts a new game.
   /// </summary>
   static void SetupDefaultWheels();

   // ============================================================================
   // External API Accessors
   // These methods provide access to internal state for the WheelerAPI.
   // They do NOT acquire locks - callers must handle synchronization.
   // ============================================================================
   
   /// <summary>
   /// Get direct access to the wheels vector for API use.
   /// Caller must hold appropriate lock.
   /// </summary>
   static std::vector<std::unique_ptr<Wheel>>& GetWheels() { return _wheels; }
   
   /// <summary>
   /// Get reference to the wheel data lock for API synchronization.
   /// </summary>
   static std::shared_mutex& GetWheelDataLock() { return _wheelDataLock; }
   
   /// <summary>
   /// Get a wheel by index. Returns nullptr if out of range.
   /// Caller must hold appropriate lock.
   /// </summary>
   static Wheel* GetWheelByIndex(int a_index);
   
   /// <summary>
   /// Get the total number of wheels.
   /// </summary>
   static int GetWheelCount() { return static_cast<int>(_wheels.size()); }

private:
   enum class WheelState
   {
      KOpened,
      KClosed,
      KOpening,
      KClosing
   };
   static inline WheelState _state = WheelState::KClosed;
   
   static inline bool _editMode = false;

   static inline const char* _wheelWindowID = "##Wheeler";

   
   static inline ImVec2 _cursorPos = { 0, 0 };

   static ImVec2 getWheelCenter();
   
   static inline std::vector<std::unique_ptr<Wheel>> _wheels;
   static inline int _activeWheelIdx = 0;

   


   static inline float _openTimer = 0;
   static inline float _closeTimer = 0;

   /// <summary>
   /// Guards _wheels and the wheel/UI state reached through it.
   ///
   /// Update() is the one writer permitted to hold this shared: it is the sole
   /// render thread, and every other writer -- input entry points and the whole
   /// API surface -- takes it exclusively, so the two never overlap. The only
   /// other shared holder is SerializeIntoJsonObj(), which is read-only.
   ///
   /// Adding a second writer that takes it shared would silently break that
   /// invariant; take it exclusively instead. The cost is that an input event
   /// waits for the frame in flight, which is not perceptible because the wheel
   /// is redrawn by Update() either way.
   /// </summary>
   static inline std::shared_mutex _wheelDataLock;

   // Whether the wheel should enter edit mode. Edit mode toggles whenever a game inventory UI opens up.
   static bool shouldBeInEditMode(RE::UI* a_ui);
   
   static void hideEditModeVanillaMenus(RE::UI* a_ui);
   static void showEditModeVanillaMenus(RE::UI* a_ui);

   static void enterEditMode();
   static void exitEditMode();

   /// <summary>
   /// Bodies of CloseWheeler() and DeleteCurrentWheel() for callers that already
   /// hold _wheelDataLock. The public wrappers take the lock and delegate here;
   /// calling a wrapper from inside a locked scope would re-enter a mutex that is
   /// not recursive.
   /// </summary>
   static void closeWheelerLocked();
   static void deleteCurrentWheelLocked();
   static void tryCloseWheelerLocked();

   // Set by RequestClose() and consumed by Update(). Atomic because RequestClose()
   // takes no lock at all and Update() holds only a shared one, so the write and
   // the read-then-clear are not mutually excluded.
   static inline std::atomic<bool> _closeRequested{ false };

   /// <summary>
   /// Raise a client notification, or park it if the calling thread is inside a
   /// DeferredNotifications scope. Every Wheeler path that reaches WheelerAPI's
   /// Notify* functions goes through these instead of calling them directly.
   /// </summary>
   static void notifyWheelStateChanged(int32_t a_wheelIndex, bool a_isOpen);
   static void notifyEditModeChanged(bool a_entered);
   static void notifyItemActivated(int32_t a_wheelIndex, int32_t a_entryIndex, int32_t a_itemIndex, uint32_t a_formID, bool a_isPrimary);

   /// <summary>
   /// Withholds client notifications for as long as it is alive, dispatching them
   /// from its destructor. _wheelDataLock is not recursive, and a callback is free
   /// to call back into WheelerAPI — every public API entry re-locks it — so a
   /// notification raised under the lock would hang the thread that raised it.
   /// Declare this *before* the lock guard in the scope that takes the lock, so
   /// that it unwinds last, i.e. after the lock has been released.
   /// </summary>
   struct DeferredNotifications
   {
      DeferredNotifications();
      ~DeferredNotifications();
      DeferredNotifications(const DeferredNotifications&) = delete;
      DeferredNotifications& operator=(const DeferredNotifications&) = delete;
   };

   /// <summary>
   /// Body of Clear(). Caller must already hold _wheelDataLock exclusively.
   /// Destroys every unmanaged wheel and compacts the surviving managed wheels
   /// to the front of _wheels, preserving their relative order.
   /// </summary>
   static void clearUnmanagedLocked();

   /// <summary>
   /// Body of ReloadFromJsonObj(). Caller must already hold _wheelDataLock
   /// exclusively, and must have called clearUnmanagedLocked() first.
   /// </summary>
   static void deserializeLocked(const nlohmann::json& a_json, SKSE::SerializationInterface* a_intfc);

   static float getCursorRadiusMax();
};
