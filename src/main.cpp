// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "RimeWithToy.h"
#include "caret.h"
#include "i18n.h"
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
extern PositionType position_type;

static HANDLE g_hDeployerMutex = NULL;

namespace weasel {
bool AcquireDeployerMutex() {
  if (g_hDeployerMutex)
    return true;
  HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
  DWORD error = ::GetLastError();
  if (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED) {
    CloseHandle(hMutex);
    return false;
  }
  g_hDeployerMutex = hMutex;
  return true;
}

void ReleaseDeployerMutex() {
  if (g_hDeployerMutex) {
    ReleaseMutex(g_hDeployerMutex);
    CloseHandle(g_hDeployerMutex);
    g_hDeployerMutex = NULL;
  }
}
} // namespace weasel

void update_position(HWND hwnd) {
  if (m_toy)
    m_toy->RefreshInputPosition(hwnd);
}

static HWND hwnd_previous = nullptr;
static HWINEVENTHOOK g_foregroundHook = nullptr;
static bool caps_key_down = false;

static void handle_window_change(HWND hwnd) {
  if (!m_toy || !hwnd || hwnd == hwnd_previous)
    return;
  hwnd_previous = hwnd;
  m_toy->DestroyUI();
}

// ----------------------------------------------------------------------------
void CALLBACK ForegroundWindowEventHook(HWINEVENTHOOK hook, DWORD event,
                                        HWND hwnd, LONG idObject, LONG idChild,
                                        DWORD idEventThread,
                                        DWORD dwmsEventTime) {
  if (event == EVENT_SYSTEM_FOREGROUND)
    handle_window_change(hwnd);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (!rime_toy_enabled)
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
  HWND hwnd = GetForegroundWindow();
  handle_window_change(hwnd);
  // ensure ime keyboard not open, not ok yet to Weasel
  HWND hImcWnd = ImmGetDefaultIMEWnd(hwnd);
  if (hImcWnd) {
    if (SendMessage(hImcWnd, WM_IME_CONTROL, 5, 0))
      SendMessage(hImcWnd, WM_IME_CONTROL, 6, 0);
  }
  if (nCode == HC_ACTION) {
    // update keyState table
    update_keystates(wParam, lParam);
    KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
    if (!pKeyboard->vkCode)
      goto skip;
    // get KBDLLHOOKSTRUCT info, generate keyinfo
    KeyInfo ki = parse_key(wParam, lParam);
    KeyEvent ke;
    if (ConvertKeyEvent(pKeyboard, ki, ke)) {
      bool eat = false;
      m_toy->StartUI();
      eat = m_toy->ProcessKeyEvent(ke);

      auto committed = m_toy->CheckCommit(false);
      update_position(hwnd);
      if (committed)
        m_toy->UpdateUI();
      if (ke.keycode == ibus::Caps_Lock) {
        if (!(ke.mask & ibus::RELEASE_MASK)) {
          // fresh press (not auto-repeat): mirror the system caps toggle
          if (!caps_key_down && !eat)
            caps_lock_on = !caps_lock_on;
          caps_key_down = true;
          if (eat) {
            // key eaten (candidate selection / mode switch): revert the caps
            // toggle the system applied before the hook
            keyState[VK_CAPITAL] = 0;
            SetKeyboardState(keyState);
            if (committed || m_toy->GetRimeStatus().composing)
              return 1;
            goto skip;
          }
        } else {
          caps_key_down = false;
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
  // A relaunched instance (--restart <pid>) waits for the previous instance to
  // exit before acquiring the single-instance mutex, so the restart succeeds.
  if (lpCmdLine) {
    std::wstring cmd(lpCmdLine);
    size_t pos = cmd.find(L"--restart");
    if (pos != std::wstring::npos) {
      size_t p = pos + 9; // length of "--restart"
      while (p < cmd.size() && (cmd[p] == L' ' || cmd[p] == L'\t'))
        ++p;
      DWORD oldPid = 0;
      try {
        oldPid = static_cast<DWORD>(std::stoul(cmd.substr(p)));
      } catch (...) {
        oldPid = 0;
      }
      if (oldPid) {
        HANDLE hOld = OpenProcess(SYNCHRONIZE, FALSE, oldPid);
        if (hOld) {
          WaitForSingleObject(hOld, 20000);
          CloseHandle(hOld);
        }
      }
    }
  }

  HANDLE hMutex = ::CreateMutex(NULL, FALSE, L"rime.toy.single.instance");
  if (::GetLastError() == ERROR_ALREADY_EXISTS ||
      ::GetLastError() == ERROR_ACCESS_DENIED)
    return 0;
  i18n::Initialize(hInstance, i18n::kLanguageAuto);
  if (!AcquireDeployerMutex()) {
    MessageBoxW(NULL, i18n::Get("msgbox_deployer_running").c_str(), L"rime.toy",
                MB_ICONWARNING | MB_OK);
    return 0;
  }
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
  g_foregroundHook =
      SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL,
                      ForegroundWindowEventHook, 0, 0,
                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  if (!g_foregroundHook) {
    DEBUG << L"Failed to install foreground-window hook! " << std::hex
          << GetLastError();
  }
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  UnhookWindowsHookEx(hKeyboardHook);
  UnhookWindowsHookEx(hMouseHook);
  if (g_foregroundHook)
    UnhookWinEvent(g_foregroundHook);
  caret::Shutdown();
  m_toy->Finalize();
  CoUninitialize();
  return 0;
}
