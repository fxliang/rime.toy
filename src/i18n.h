#pragma once
#include <string>
#include <windows.h>

enum class AppLanguage {
  English = 0,
  SimplifiedChinese = 1,
  TraditionalChinese = 2
};

namespace i18n {

constexpr int kLanguageAuto = -1;

// language: kLanguageAuto for system detection, otherwise AppLanguage value
void Initialize(HINSTANCE hInstance, int language);
AppLanguage Current();
// look up a key in the current language, fall back to English
std::wstring Get(const std::string &key);
// language display name shown in its own language (English / 简体中文 /
// 繁體中文)
std::wstring LanguageDisplayName(AppLanguage language);
// parse a language string from config, return kLanguageAuto if unrecognized
int LanguageFromString(const std::string &str);
// canonical config value ("en" / "zh-Hans" / "zh-Hant") for given language
const char *LanguageString(int language);
// change runtime language, return false if not changed
bool SetLanguage(int language);

} // namespace i18n