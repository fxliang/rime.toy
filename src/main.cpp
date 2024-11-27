// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "RimeWithToy.h"
#include "keymodule.h"
#include <ShellScalingApi.h>
#include <WeaselIPCData.h>
#include <WeaselUI.h>

using namespace std;
using namespace weasel;
// ----------------------------------------------------------------------------
HHOOK hHook = NULL;
std::unique_ptr<UI> m_ui;
std::unique_ptr<RimeWithToy> m_toy;

void update_position(HWND hwnd) {
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
};

// ----------------------------------------------------------------------------
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
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
      // reverse up / down when is to reposition
      if (m_ui->GetIsReposition()) {
        if (ke.keycode == ibus::Up)
          ke.keycode = ibus::Down;
        else if (ke.keycode == ibus::Down)
          ke.keycode = ibus::Up;
      }
      eat = m_toy->ProcessKeyEvent(ke, commit_str);

      m_toy->UpdateUI();
      status = m_ui->status();
      update_position(hwnd);
      committed = !commit_str.empty();
      if (!commit_str.empty()) {
        send_input_to_window(hwnd, commit_str);
        commit_str.clear();
        if (!status.composing)
          m_ui->Hide();
        else {
          m_toy->UpdateUI();
          update_position(hwnd);
        }
      }

      if (ke.keycode == ibus::Caps_Lock && eat) {
        if (keyState[VK_CAPITAL] & 0x01) {
          keyState[VK_CAPITAL] = 0;
          SetKeyboardState(keyState);
          if (committed || status.composing)
            return 1;
          goto skip;
        }
      }
      if (eat)
        return 1;
    }
  }
skip:
  return CallNextHookEx(hHook, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  HR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
  m_ui = std::make_unique<UI>();
  m_toy = std::make_unique<RimeWithToy>(m_ui.get(), hInstance);
  m_ui->Create(nullptr);
  // --------------------------------------------------------------------------
  hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (!hHook) {
    DEBUG << L"Failed to install hook!";
    exit(1);
  }
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  m_toy->Finalize();
  UnhookWindowsHookEx(hHook);
  CoUninitialize();
  return 0;
}
