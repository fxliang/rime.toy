#include "trayicon.h"
#include <resource.h>
#include <shellapi.h>
#include <utils.h>

TrayIcon::TrayIcon(HINSTANCE hInstance, const std::wstring &tooltip)
    : hInst(hInstance), hMenu(NULL), deploy_func(nullptr),
      switch_ascii(nullptr), enable_debug(false) {
  CreateHwnd();
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = m_hWnd;
  nid.uID = 1;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_USER + 1;
  wcsncpy_s(nid.szTip, tooltip.c_str(), _countof(nid.szTip) - 1);
  nid.szTip[_countof(nid.szTip) - 1] = L'\0'; // 确保以空字符结尾
}

TrayIcon::~TrayIcon() {
  Hide();
  DestroyWindow(m_hWnd);
}

void TrayIcon::Show() { Shell_NotifyIcon(NIM_ADD, &nid); }

void TrayIcon::Hide() { Shell_NotifyIcon(NIM_DELETE, &nid); }

void TrayIcon::CreateHwnd() {
  WNDCLASS wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.lpszClassName = L"TrayIcon";
  wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_MAIN));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  ::RegisterClass(&wc);
  m_hWnd =
      CreateWindow(L"TrayIcon", L"TrayIcon", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                   CW_USEDEFAULT, 0, 0, NULL, NULL, hInst, this);
  ShowWindow(m_hWnd, SW_HIDE);
}

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

void TrayIcon::CreateContextMenu() {
  if (hMenu == NULL) {
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING | (enable_debug ? MF_CHECKED : MFS_UNCHECKED),
               1004, L"调试信息");
    AppendMenu(hMenu, MF_STRING, 1003, L"重新部署");
    AppendMenu(hMenu, MF_STRING, 1002, L"退出");
  }
}

void TrayIcon::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam,
                              LPARAM lParam) {
  switch (msg) {
  case WM_USER + 1:
    if (lParam == WM_RBUTTONUP) {
      CreateContextMenu();
      POINT pt;
      GetCursorPos(&pt);
      SetForegroundWindow(hwnd);
      TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    } else if (lParam == WM_LBUTTONUP) {
      if (switch_ascii)
        switch_ascii();
    }
    break;

  case WM_COMMAND:
    if (LOWORD(wParam) == 1002) { // Exit
      Hide();
      PostQuitMessage(0);
    } else if (LOWORD(wParam) == 1003) {
      if (deploy_func)
        deploy_func();
    } else if (LOWORD(wParam) == 1004) {
      enable_debug = !enable_debug;

      if (hMenu) {
        DEBUG << "yes";
        CheckMenuItem(hMenu, 1004, enable_debug ? MF_CHECKED : MF_UNCHECKED);
      } else {
        DEBUG << "No";
      }
      InvalidateRect(hwnd, NULL, true);
    }
    break;
  }
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam) {
  TrayIcon *self;
  if (msg == WM_NCCREATE) {
    self = static_cast<TrayIcon *>(
        reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<TrayIcon *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }
  if (self)
    self->ProcessMessage(hwnd, msg, wParam, lParam);
  return DefWindowProc(hwnd, msg, wParam, lParam);
}
