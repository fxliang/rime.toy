#include "i18n.h"
#include <nlohmann/json.hpp>
#include <resource.h>

namespace i18n {

using json = nlohmann::json;

static HINSTANCE g_hInstance = NULL;
static AppLanguage g_language = AppLanguage::English;
static json g_translations;

static std::wstring utf8_to_wstring(const std::string &str) {
  if (str.empty())
    return L"";
  int len =
      MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
  if (len <= 0)
    return L"";
  std::wstring result(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0],
                      len);
  return result;
}

static AppLanguage DetectSystemLanguage() {
  LANGID lang = GetUserDefaultUILanguage();
  if (lang == 0)
    lang = GetSystemDefaultUILanguage();
  switch (PRIMARYLANGID(lang)) {
  case LANG_CHINESE:
    switch (SUBLANGID(lang)) {
    case SUBLANG_CHINESE_TRADITIONAL:
    case SUBLANG_CHINESE_HONGKONG:
    case SUBLANG_CHINESE_MACAU:
      return AppLanguage::TraditionalChinese;
    default:
      return AppLanguage::SimplifiedChinese;
    }
  default:
    return AppLanguage::English;
  }
}

static const char *LangKey(AppLanguage lang) {
  switch (lang) {
  case AppLanguage::SimplifiedChinese:
    return "zh-Hans";
  case AppLanguage::TraditionalChinese:
    return "zh-Hant";
  default:
    return "en";
  }
}

static void LoadTranslations() {
  if (!g_translations.empty())
    return;
  HRSRC hrsrc =
      FindResourceW(g_hInstance, MAKEINTRESOURCE(IDR_I18N_JSON), RT_RCDATA);
  if (!hrsrc)
    return;
  HGLOBAL hglob = LoadResource(g_hInstance, hrsrc);
  if (!hglob)
    return;
  const char *data = (const char *)LockResource(hglob);
  DWORD size = SizeofResource(g_hInstance, hrsrc);
  if (!data || size == 0)
    return;
  try {
    g_translations = json::parse(data, data + size);
  } catch (...) {
    g_translations = json();
  }
}

void Initialize(HINSTANCE hInstance, int language) {
  g_hInstance = hInstance;
  LoadTranslations();
  if (language == kLanguageAuto)
    g_language = DetectSystemLanguage();
  else
    g_language = static_cast<AppLanguage>(language);
}

AppLanguage Current() { return g_language; }

std::wstring Get(const std::string &key) {
  std::string str;
  const char *lang_key = LangKey(g_language);
  if (g_translations.contains(lang_key) &&
      g_translations[lang_key].is_object() &&
      g_translations[lang_key].contains(key))
    str = g_translations[lang_key][key].get<std::string>();
  else if (g_translations.contains("en") && g_translations["en"].is_object() &&
           g_translations["en"].contains(key))
    str = g_translations["en"][key].get<std::string>();
  return utf8_to_wstring(str);
}

std::wstring LanguageDisplayName(AppLanguage language) {
  switch (language) {
  case AppLanguage::SimplifiedChinese:
    return L"简体中文";
  case AppLanguage::TraditionalChinese:
    return L"繁體中文";
  default:
    return L"English";
  }
}

int LanguageFromString(const std::string &str) {
  if (str == "en" || str == "en_US" || str == "en-US")
    return (int)AppLanguage::English;
  if (str == "zh-Hant" || str == "zh-TW" || str == "zh_TW" || str == "zh-HK" ||
      str == "zh-MO")
    return (int)AppLanguage::TraditionalChinese;
  if (str == "zh-Hans" || str == "zh-CN" || str == "zh_CN" || str == "zh")
    return (int)AppLanguage::SimplifiedChinese;
  return kLanguageAuto;
}

const char *LanguageString(int language) {
  switch (language) {
  case (int)AppLanguage::SimplifiedChinese:
    return "zh-Hans";
  case (int)AppLanguage::TraditionalChinese:
    return "zh-Hant";
  default:
    return "en";
  }
}

bool SetLanguage(int language) {
  if (language != (int)AppLanguage::English &&
      language != (int)AppLanguage::SimplifiedChinese &&
      language != (int)AppLanguage::TraditionalChinese)
    return false;
  AppLanguage lang = static_cast<AppLanguage>(language);
  if (lang == g_language)
    return false;
  g_language = lang;
  return true;
}

} // namespace i18n