// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "RimeWithToy.h"
#include "keymodule.h"
#include <ShellScalingApi.h>
#include <WeaselIPCData.h>
#include <WeaselUI.h>
#include <imm.h>

using namespace std;
using namespace weasel;
// ----------------------------------------------------------------------------
HHOOK hKeyboardHook = NULL;
HHOOK hMouseHook = NULL;
std::unique_ptr<RimeWithToy> m_toy;
wstring commit_str = L"";

void update_position(HWND hwnd) {
  POINT pt;
  if (!GetCursorPos(&pt)) {
    RECT rect;
    if (hwnd)
      GetWindowRect(hwnd, &rect);
    pt.x = rect.left + (rect.right - rect.left) / 2 - 150;
    pt.y = rect.bottom - (rect.bottom - rect.top) / 2 - 100;
  }
  m_toy->UpdateInputPosition({pt.x, 0, 0, pt.y});
};

// ----------------------------------------------------------------------------
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  static HWND hwnd_previous = nullptr;
  static Status status;
  static bool committed = true;

  HWND hwnd = GetForegroundWindow();
  if (hwnd != hwnd_previous) {
    hwnd_previous = hwnd;
    m_toy->DestroyUI();
  }
  // ensure ime keyboard not open, not ok yet to Weasel
  HWND hImcWnd = ImmGetDefaultIMEWnd(hwnd);
  if (hImcWnd) {
    if (SendMessage(hImcWnd, WM_IME_CONTROL, 5, 0))
      SendMessage(hImcWnd, WM_IME_CONTROL, 6, 0);
  }
  if (nCode == HC_ACTION && !inserting) {
    // update keyState table
    update_keystates(wParam, lParam);

    KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
    // get KBDLLHOOKSTRUCT info, generate keyinfo
    KeyInfo ki = parse_key(wParam, lParam);
    KeyEvent ke;
    if (ConvertKeyEvent(pKeyboard, ki, ke)) {
      bool eat = false;
      // reverse up / down when is to reposition
      m_toy->StartUI();
      eat = m_toy->ProcessKeyEvent(ke);

      m_toy->UpdateUI();
      status = m_toy->GetRimeStatus();
      update_position(hwnd);
      committed = !commit_str.empty();
      if (!commit_str.empty()) {
        send_input_to_window(commit_str);
        commit_str.clear();
        if (!status.composing)
          m_toy->HideUI();
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
  return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION && m_toy->UIHwnd()) {
    if (wParam == WM_MOUSEWHEEL) {
      PostMessage(m_toy->UIHwnd(), WM_MOUSEWHEEL, wParam, lParam);
    } else if (wParam == WM_MOUSEMOVE) {
      PostMessage(m_toy->UIHwnd(), WM_MOUSEMOVE, wParam, lParam);
    } else if (wParam == WM_MOUSEACTIVATE) {
      PostMessage(m_toy->UIHwnd(), WM_MOUSEACTIVATE, wParam, lParam);
    } else if (wParam == WM_MOUSELEAVE) {
      PostMessage(m_toy->UIHwnd(), WM_MOUSELEAVE, wParam, lParam);
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  HR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
  m_toy = std::make_unique<RimeWithToy>(hInstance, commit_str);
  // --------------------------------------------------------------------------
  hKeyboardHook =
      SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (!hKeyboardHook) {
    DEBUG << L"Failed to install keyboard hook!";
    exit(1);
  }
  hMouseHook =
      SetWindowsHookEx(WH_MOUSE, MouseProc, NULL, GetCurrentThreadId());
  if (!hMouseHook) {
    DEBUG << L"Failed to install mouse hook! " << std::hex << GetLastError();
    exit(1);
  }
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  m_toy->Finalize();
  UnhookWindowsHookEx(hKeyboardHook);
  UnhookWindowsHookEx(hMouseHook);
  CoUninitialize();
  return 0;
}
