#include "Texts.h"

static void loadTextFromIni(CSimpleIniA& a_ini, Texts::TextEntry& r_entry)
{
   r_entry.text = std::string(a_ini.GetValue("Texts", r_entry.iniKey.data(), r_entry.text.data()));
}
void Texts::LoadTranslations()
{
   CSimpleIniA ini;
   ini.SetUnicode();
#define TEXTS_PATH "Data\\SKSE\\Plugins\\wheeler\\Texts.ini"
   ini.LoadFile(TEXTS_PATH);
   try {
      for (auto& [textType, entry] : _textData) {
      loadTextFromIni(ini, entry);
      }
   }
   catch (std::exception e) {
      ERROR("Error loading from Texts.ini: {}", e.what());
   }

}

const char* Texts::GetText(TextType a_textType)
{
   return _textData[a_textType].text.data();
}
