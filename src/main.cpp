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

static HWND hwnd_previous = nullptr;
// ----------------------------------------------------------------------------
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (!rime_toy_enabled)
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
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
      m_toy->StartUI();
      eat = m_toy->ProcessKeyEvent(ke);
      update_position(hwnd);

      auto committed = m_toy->CheckCommit();
      if (ke.keycode == ibus::Caps_Lock && eat) {
        if (keyState[VK_CAPITAL] & 0x01) {
          keyState[VK_CAPITAL] = 0;
          SetKeyboardState(keyState);
          if (committed || m_toy->GetRimeStatus().composing)
            return 1;
          goto skip;
        }
      }
      if (eat && !(ke.mask & ibus::RELEASE_MASK))
        return 1;
    }
  }
skip:
  return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (rime_toy_enabled && nCode == HC_ACTION && m_toy->UIHwnd()) {
    HWND hwnd = GetForegroundWindow();
    if (hwnd != hwnd_previous) {
      hwnd_previous = hwnd;
      m_toy->DestroyUI();
      return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
  HANDLE hMutex = ::CreateMutex(NULL, FALSE, L"rime.toy.single.instance");
  if (::GetLastError() == ERROR_ALREADY_EXISTS ||
      ::GetLastError() == ERROR_ACCESS_DENIED)
    return 0;
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  HR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
  m_toy = std::make_unique<RimeWithToy>(hInstance);
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
