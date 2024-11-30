#pragma once
#include <functional>
#include <string>
#include <windows.h>

class TrayIcon {
public:
  TrayIcon(HINSTANCE hInstance, const std::wstring &tooltip);
  ~TrayIcon();
  void Show();
  void Hide();
  void SetIcon(HICON hIcon);
  void SetTooltip(const std::wstring &tooltip);
  void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void SetDeployFunc(const std::function<void(void)> &func) {
    deploy_func = func;
  }
  void SetSwichAsciiFunc(const std::function<void(void)> &func) {
    switch_ascii = func;
  }
  void SetSwichDarkFunc(const std::function<void(void)> &func) {
    switch_dark = func;
  }
  bool debug() { return enable_debug; }

private:
  HINSTANCE hInst;
  NOTIFYICONDATA nid;
  HMENU hMenu;
  HWND m_hWnd;
  std::function<void(void)> deploy_func;
  std::function<void(void)> switch_ascii;
  std::function<void(void)> switch_dark;
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
