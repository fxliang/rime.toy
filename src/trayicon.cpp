#include "trayicon.h"
#include "keymodule.h"
#include <resource.h>
#include <shellapi.h>
#include <utils.h>
#include <winuser.h>

#define MENU_QUIT 1002
#define MENU_DEPLOY 1003
#define MENU_SYNC 1004
#define MENU_DEBUG 1005
#define MENU_SHARED_DIR 1006
#define MENU_USER_DIR 1007
#define MENU_LOG_DIR 1008
#define MENU_RIME_TOY_EN 1009

bool rime_toy_enabled = true;
HICON icon_error = LoadIcon(NULL, IDI_ERROR);

TrayIcon::TrayIcon(HINSTANCE hInstance, const std::wstring &tooltip)
    : hInst(hInstance), hMenu(NULL), deploy_func(nullptr),
      switch_ascii(nullptr), enable_debug(false) {
  current_dark_mode = IsUserDarkMode();
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
  DestroyIcon(icon_error);
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
    AppendMenu(hMenu,
               MF_STRING | (rime_toy_enabled ? MF_CHECKED : MFS_UNCHECKED),
               MENU_RIME_TOY_EN, L"使用rime.toy");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, MENU_LOG_DIR, L"日志目录");
    AppendMenu(hMenu, MF_STRING, MENU_SHARED_DIR, L"共享目录");
    AppendMenu(hMenu, MF_STRING, MENU_USER_DIR, L"用户目录");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING | (enable_debug ? MF_CHECKED : MFS_UNCHECKED),
               MENU_DEBUG, L"调试信息");
    AppendMenu(hMenu, MF_STRING, MENU_SYNC, L"同步数据");
    AppendMenu(hMenu, MF_STRING, MENU_DEPLOY, L"重新部署");
    AppendMenu(hMenu, MF_STRING, MENU_QUIT, L"退出");
  }
}

void TrayIcon::ShowBalloonTip(const std::wstring &title,
                              const std::wstring &message, DWORD timeout) {
  if (nid.uFlags | NIF_INFO) {
    OnBalloonTimeout();
  }
  // 设置 NIF_INFO 来显示气泡提示
  nid.uFlags |= NIF_INFO;
  // 设置气泡提示的标题和内容
  wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
  wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
  // 设置气泡提示的显示时间
  nid.uTimeout = timeout;      // 以毫秒为单位
  nid.dwInfoFlags = NIIF_INFO; // 设定气泡提示的类型（信息级别）
  // 发送更新托盘图标的消息并显示气泡提示
  Shell_NotifyIcon(NIM_MODIFY, &nid);
  // 启动定时器，超时后清除气泡提示
  SetTimer(m_hWnd, TIMER_BALLOON_TIMEOUT, timeout, NULL);
}

void TrayIcon::OnBalloonTimeout() {
  KillTimer(m_hWnd, TIMER_BALLOON_TIMEOUT);
  // 清除气泡提示内容
  nid.uFlags &= ~NIF_INFO;                             // 移除 NIF_INFO 标志
  memset(nid.szInfoTitle, 0, sizeof(nid.szInfoTitle)); // 清空标题
  memset(nid.szInfo, 0, sizeof(nid.szInfo));           // 清空提示内容
  // 更新托盘图标，清除气泡提示
  Shell_NotifyIcon(NIM_MODIFY, &nid);
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
      if (switch_ascii && rime_toy_enabled)
        switch_ascii();
    }
    break;

  case WM_SETTINGCHANGE:
    if (current_dark_mode != IsUserDarkMode()) {
      current_dark_mode = !current_dark_mode;
      if (switch_dark)
        switch_dark();
    }
    break;

  case WM_TIMER:
    if (wParam == TIMER_BALLOON_TIMEOUT)
      OnBalloonTimeout();
    break;
  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case MENU_QUIT: {
      Hide();
      PostQuitMessage(0);
      break;
    }
    case MENU_DEPLOY: {
      if (deploy_func)
        deploy_func();
      break;
    }
    case MENU_DEBUG: {
      enable_debug = !enable_debug;
      if (hMenu)
        CheckMenuItem(hMenu, MENU_DEBUG,
                      enable_debug ? MF_CHECKED : MF_UNCHECKED);
      InvalidateRect(hwnd, NULL, true);
      break;
    }
    case MENU_SHARED_DIR: {
      if (open_shareddir)
        open_shareddir();
      break;
    }
    case MENU_USER_DIR: {
      if (open_userdir)
        open_userdir();
      break;
    }
    case MENU_LOG_DIR: {
      if (open_logdir)
        open_logdir();
      break;
    }
    case MENU_SYNC: {
      if (sync_data)
        sync_data();
      break;
    }
    case MENU_RIME_TOY_EN: {
      rime_toy_enabled = !rime_toy_enabled;
      if (!rime_toy_enabled)
        memset(weasel::keyState, 0, sizeof(weasel::keyState));
      if (hMenu)
        CheckMenuItem(hMenu, MENU_RIME_TOY_EN,
                      rime_toy_enabled ? MF_CHECKED : MF_UNCHECKED);
      if (rime_toy_enabled && refresh_icon)
        refresh_icon();
      else
        SetIcon(icon_error);
      InvalidateRect(hwnd, NULL, true);
    }
    }
  } break;
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
