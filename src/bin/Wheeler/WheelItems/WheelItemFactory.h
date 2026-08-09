class WheelItem;
class WheelItemFactory
{
public:
   /// <summary>
   /// Creates a new wheel item that's either a:
   /// WheelItemSpell, WheelItemWeapon, WheelItemArmor, WheelItemAmmo, WheelItemPower, WheelItemShout,
   /// based on the item that's currently being hovered on in the inventory.
   /// Returns nullptr if no item is being currently hovered, or the item do not match any of the above types.
   /// </summary>
   /// <returns>Created item, or null if no item is applicable.</returns>
   static std::shared_ptr<WheelItem> MakeWheelItemFromMenuHovered();

   /// <summary>
   /// Creates a new wheel item that's either a:
   /// WheelItemSpell, WheelItemWeapon, WheelItemArmor, WheelItemAmmo, WheelItemPower, WheelItemShout,
   /// based on a .json object.
   /// Returns nullptr if no item is being currently hovered, or the item do not match any of the above types.
   /// </summary>
   /// <returns>Created item, or null if no item is applicable.</returns>
   static std::shared_ptr<WheelItem> MakeWheelItemFromJsonObject(nlohmann::json a_json, SKSE::SerializationInterface* a_intfc);

   /// <summary>
   /// Creates a new wheel item from a FormID.
   /// For weapons/armor, uniqueID is required to identify the specific inventory item.
   /// Returns nullptr if form not found or form type is unsupported.
   /// </summary>
   /// <param name="a_formID">The FormID of the game form</param>
   /// <param name="a_uniqueID">UniqueID for weapons/armor (ignored for other types)</param>
   /// <returns>Created item, or null if not applicable.</returns>
   static std::shared_ptr<WheelItem> MakeWheelItemFromFormID(RE::FormID a_formID, uint16_t a_uniqueID = 0);
};
