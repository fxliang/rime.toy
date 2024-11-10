// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "RimeWithToy.h"
#include "keymodule.h"
#include "trayicon.h"
#include <ShellScalingApi.h>
#include <WeaselIPCData.h>
#include <WeaselUI.h>
#include <resource.h>
#include <sstream>

using namespace std;
using namespace weasel;
// ----------------------------------------------------------------------------
HHOOK hHook = NULL;
std::unique_ptr<UI> m_ui;
std::unique_ptr<RimeWithToy> m_toy;
// ----------------------------------------------------------------------------
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  static KeyEvent prevKeyEvent;
  static BOOL prevfEaten = FALSE;
  static int keyCountToSimulate = 0;
  static HWND hwnd_previous = nullptr;
  static Status status;
  static bool committed = true;
  static wstring commit_str = L"";

  HWND hwnd = GetForegroundWindow();
  if (hwnd != hwnd_previous) {
    hwnd_previous = hwnd;
    m_ui->ctx().clear();
    m_ui->Hide();
  }
  if (nCode == HC_ACTION) {
    // update keyState table
    update_keystates(wParam, lParam);

    KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
    // get KBDLLHOOKSTRUCT info, generate keyinfo
    KeyInfo ki = parse_key(wParam, lParam);
    KeyEvent ke;
    if (ConvertKeyEvent(pKeyboard, ki, ke)) {
      bool eat = false;
      if (!keyCountToSimulate)
        eat = m_toy->ProcessKeyEvent(ke, commit_str);
      // make capslock bindable for rime
      if (ke.keycode == ibus::Caps_Lock) {
        if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
            (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
          if (GetKeyState(VK_CAPITAL) & 0x01) {
            if (committed || (!eat && status.composing)) {
              keyCountToSimulate = 2;
              INPUT inputs[2];
              inputs[0].type = INPUT_KEYBOARD;
              inputs[0].ki = {VK_CAPITAL, 0, 0, 0, 0};
              inputs[1].type = INPUT_KEYBOARD;
              inputs[1].ki = {VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0, 0};
              ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs,
                          sizeof(INPUT));
            }
          }
          eat = TRUE;
        }
        if (keyCountToSimulate)
          keyCountToSimulate--;
      }
      // make capslock bindable for rime ends
      prevfEaten = eat;
      prevKeyEvent = ke;

      m_toy->UpdateUI();
      status = m_ui->status();

      // update position
      POINT pt;
      if (GetCursorPos(&pt)) {
        m_ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
      } else {
        RECT rect;
        if (hwnd)
          GetWindowRect(hwnd, &rect);
        pt.x = rect.left + (rect.right - rect.left) / 2 - 150;
        pt.y = rect.bottom - (rect.bottom - rect.top) / 2 - 100;
        m_ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
      }
      if (!commit_str.empty()) {
        send_input_to_window(hwnd, commit_str);
        commit_str.clear();
        if (!status.composing)
          m_ui->Hide();
        committed = true;
      } else
        committed = false;
      if ((ke.keycode == ibus::Caps_Lock && keyCountToSimulate) ||
          status.ascii_mode) {
        goto skip;
      }
      if (eat)
        return 1;
    }
  }
skip:
  return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void set_hook() {
  hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (hHook == NULL) {
    OutputDebugString(L"Failed to install hook!");
    exit(1);
  }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
  wstring cmdLine(lpCmdLine), arg;
  wistringstream wiss(cmdLine);
  bool horizontal = false;
  while (wiss >> arg) {
    if (arg == L"/h")
      horizontal = true;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  HICON ime_icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN));
  HICON ascii_icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_ASCII));
  std::unique_ptr<TrayIcon> trayIcon;
  m_ui = std::make_unique<UI>();
  m_toy = std::make_unique<RimeWithToy>(m_ui.get());
  m_toy->SetTrayIconCallBack([&](const Status &sta) {
    trayIcon->SetIcon(sta.ascii_mode ? ascii_icon : ime_icon);
  });
  m_ui->SetHorizontal(horizontal);
  m_ui->Create(nullptr);
  // --------------------------------------------------------------------------
  auto tooltip = L"rime.toy\n左键点击切换ASCII\n右键菜单可退出^_^";
  trayIcon = std::make_unique<TrayIcon>(hInstance, tooltip);
  trayIcon->SetDeployFunc([&]() {
    OutputDebugString(L"Deploy Menu clicked");
    m_toy->Initialize();
  });
  trayIcon->SetSwichAsciiFunc([&]() { m_toy->SwitchAsciiMode(); });
  trayIcon->SetIcon(ime_icon);
  trayIcon->Show();
  // --------------------------------------------------------------------------
  set_hook();
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  m_toy->Finalize();
  UnhookWindowsHookEx(hHook);
  return 0;
}
