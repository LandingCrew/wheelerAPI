#pragma once
class Texts
{
public:
   enum class TextType
   {
      AlchemyDynamicIDConsumptionWarning,
      NoWheelPresent,
      SoulGemEmptyWarning,
      SoulGemNoEnchantedWeapon,
      SoulGemWeaponFullyCharged,
      SoulGemRecharged,
      Total
   };

   // Each entry is { key in Texts.ini, text to display }. The key stays the enum
   // name so existing Texts.ini files keep resolving, while the display text is
   // what the player sees when the ini has no override for that key.
   struct TextEntry
   {
      std::string iniKey;
      std::string text;
   };

   static void LoadTranslations();
   static const char* GetText(TextType a_textType);

private:
#define MAP_ENTRY(textTypeName, defaultText) \
   {                                           \
      TextType::textTypeName, { #textTypeName, defaultText } \
   }

   static inline std::unordered_map<TextType, TextEntry> _textData = {
      // These two predate per-entry defaults and displayed their own key when
      // unset; left as-is so their on-screen text does not change silently.
      MAP_ENTRY(AlchemyDynamicIDConsumptionWarning, "AlchemyDynamicIDConsumptionWarning"),
      MAP_ENTRY(NoWheelPresent, "NoWheelPresent"),
      MAP_ENTRY(SoulGemEmptyWarning, "This soul gem holds no soul."),
      MAP_ENTRY(SoulGemNoEnchantedWeapon, "No enchanted weapon equipped."),
      MAP_ENTRY(SoulGemWeaponFullyCharged, "Your weapon is already fully charged."),
      MAP_ENTRY(SoulGemRecharged, "Weapon recharged.")
   };
};
