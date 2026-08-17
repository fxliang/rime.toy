#pragma once
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

typedef std::function<void(void)> vhandler;

struct SchemaItem {
  std::wstring schema_id;
  std::wstring name;
};

struct OptionSwitchItem {
  std::wstring option_name;
  std::wstring label;
  bool checked;
  bool radio_group;
};

typedef std::function<std::vector<SchemaItem>()> schema_list_handler;
typedef std::function<std::vector<OptionSwitchItem>()> option_list_handler;
typedef std::function<std::wstring()> current_schema_handler;
typedef std::function<void(const std::wstring &)> string_handler;
typedef std::function<void(int)> int_handler;

extern bool rime_toy_enabled;

class TrayIcon {
public:
  TrayIcon(HINSTANCE hInstance, const std::wstring &tooltip);
  ~TrayIcon();
  void Show();
  void Hide();
  void SetIcon(HICON hIcon);
  void SetTooltip(const std::wstring &tooltip);
  void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void SetDeployFunc(const vhandler &func) { deploy_func = func; }
  void SetSwichAsciiFunc(const vhandler &func) { switch_ascii = func; }
  void SetSwichDarkFunc(const vhandler &func) { switch_dark = func; }
  void SetOpenSharedDirFunc(const vhandler &func) { open_shareddir = func; }
  void SetOpenUserdDirFunc(const vhandler &func) { open_userdir = func; }
  void SetOpenLogDirFunc(const vhandler &func) { open_logdir = func; }
  void SetSyncFunc(const vhandler &func) { sync_data = func; }
  void SetRefreshIconFunc(const vhandler &func) { refresh_icon = func; }
  void SetQuitHandler(const vhandler &func) { quit_app = func; }
  void SetSchemaListFunc(const schema_list_handler &func) {
    get_schema_list = func;
  }
  void SetSwitchSchemaFunc(const string_handler &func) { switch_schema = func; }
  void SetCurrentSchemaFunc(const current_schema_handler &func) {
    get_current_schema = func;
  }
  void SetOptionListFunc(const option_list_handler &func) {
    get_option_list = func;
  }
  void SetToggleOptionFunc(const string_handler &func) { toggle_option = func; }
  void SetSwitchLanguageFunc(const int_handler &func) {
    switch_language = func;
  }
  void RefreshIcon();
  bool debug() { return enable_debug; }
  void ShowBalloonTip(const std::wstring &title, const std::wstring &message,
                      DWORD timeout = 1000);
  void Deploy() {
    if (deploy_func)
      deploy_func();
  }

private:
  void OnBalloonTimeout();
  static const UINT TIMER_BALLOON_TIMEOUT = 20241202;
  HINSTANCE hInst;
  NOTIFYICONDATA nid;
  HMENU hMenu;
  HWND m_hWnd;
  vhandler deploy_func;
  vhandler switch_ascii;
  vhandler switch_dark;
  vhandler open_userdir;
  vhandler open_shareddir;
  vhandler open_logdir;
  vhandler sync_data;
  vhandler refresh_icon;
  vhandler quit_app;
  schema_list_handler get_schema_list;
  string_handler switch_schema;
  current_schema_handler get_current_schema;
  option_list_handler get_option_list;
  string_handler toggle_option;
  int_handler switch_language;
  std::vector<std::wstring> m_schema_ids;
  std::vector<std::wstring> m_option_names;
  bool enable_debug;
  bool current_dark_mode;

  void CreateContextMenu();
  void CreateHwnd();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
};

inline bool IsUserDarkMode() {
  constexpr const LPCWSTR key =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
  constexpr const LPCWSTR value = L"AppsUseLightTheme";

  DWORD type;
  DWORD data;
  DWORD size = sizeof(DWORD);
  LSTATUS st = RegGetValue(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD,
                           &type, &data, &size);

  if (st == ERROR_SUCCESS && type == REG_DWORD)
    return data == 0;
  return false;
}
