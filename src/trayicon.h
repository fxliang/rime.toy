#pragma once
#include <functional>
#include <string>
#include <windows.h>

class TrayIcon {
public:
  TrayIcon(HINSTANCE hInstance, HWND hwnd, const std::wstring &tooltip);
  ~TrayIcon();
  void Show();
  void Hide();
  void SetIcon(HICON hIcon);
  void SetTooltip(const std::wstring &tooltip);
  void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void SetDeployFunc(const std::function<void(void)> func) {
    deploy_func = func;
  }
  void SetSwichAsciiFunc(const std::function<void(void)> func) {
    switch_ascii = func;
  }

private:
  HINSTANCE hInst;
  NOTIFYICONDATA nid;
  HMENU hMenu;
  std::function<void(void)> deploy_func;
  std::function<void(void)> switch_ascii;

  void CreateContextMenu();
};
