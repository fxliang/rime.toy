#pragma once
#include <functional>
#include <string>
#include <windows.h>

typedef std::function<void(void)> vhandler;

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
  bool debug() { return enable_debug; }
  void ShowBalloonTip(const std::wstring &title, const std::wstring &message,
                      DWORD timeout = 1000);

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
