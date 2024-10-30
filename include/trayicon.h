#pragma once
#include <shellapi.h>
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
  void AddMenuItem(UINT id, const std::wstring &itemText);
  void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
  HINSTANCE hInst;
  NOTIFYICONDATA nid;
  HMENU hMenu;

  void CreateContextMenu();
};

TrayIcon::TrayIcon(HINSTANCE hInstance, HWND hwnd, const std::wstring &tooltip)
    : hInst(hInstance), hMenu(NULL) {
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = hwnd;
  nid.uID = 1;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_USER + 1;
  wcsncpy_s(nid.szTip, tooltip.c_str(), _countof(nid.szTip) - 1);
  nid.szTip[_countof(nid.szTip) - 1] = L'\0'; // 确保以空字符结尾
}

TrayIcon::~TrayIcon() { Hide(); }

void TrayIcon::Show() { Shell_NotifyIcon(NIM_ADD, &nid); }

void TrayIcon::Hide() { Shell_NotifyIcon(NIM_DELETE, &nid); }

void TrayIcon::SetIcon(HICON hIcon) {
  nid.hIcon = hIcon;
  Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void TrayIcon::SetTooltip(const std::wstring &tooltip) {
  // 使用 _countof 宏确保数组大小正确
  wcsncpy_s(nid.szTip, tooltip.c_str(), _countof(nid.szTip) - 1);
  nid.szTip[_countof(nid.szTip) - 1] = L'\0'; // 确保以空字符结尾
  Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void TrayIcon::AddMenuItem(UINT id, const std::wstring &itemText) {
  if (hMenu == NULL) {
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, id, itemText.c_str());
  }
}

void TrayIcon::CreateContextMenu() {
  // 在这里可以创建其他菜单项
  // 例如，添加退出菜单项
  AddMenuItem(1002, L"退出");
}

void TrayIcon::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam,
                              LPARAM lParam) {
  switch (msg) {
  case WM_USER + 1:
    if (lParam == WM_RBUTTONUP) {
      CreateContextMenu();
      POINT curPoint;
      GetCursorPos(&curPoint);
      SetForegroundWindow(hwnd);
      TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, curPoint.x, curPoint.y, 0, hwnd,
                     NULL);
    }
    break;

  case WM_COMMAND:
    if (LOWORD(wParam) == 1002) { // Exit
      Hide();
      PostQuitMessage(0);
    }
    break;
  }
}