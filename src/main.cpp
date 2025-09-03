// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "RimeWithToy.h"
#include "cursor_tracker.h"
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
std::unique_ptr<weasel::CursorTracker> g_cursor_tracker;
extern PositionType position_type;

void update_position(HWND hwnd) {
  POINT pt;
  bool cursor_found = false;

  // 尝试使用光标跟踪器获取真实光标位置
  if (g_cursor_tracker && g_cursor_tracker->IsEnabled()) {
    auto cursor_pos = g_cursor_tracker->GetCursorPosition(hwnd);
    if (cursor_pos.valid) {
      pt = cursor_pos.point;
      cursor_found = true;

      // 调试输出
      DEBUG << L"Cursor found using method: " << (int)cursor_pos.method
            << L" at (" << pt.x << L", " << pt.y << L")";
    }
  }

  // 回退到原有逻辑：鼠标位置或窗口中心
  if (!cursor_found) {
    if (!GetCursorPos(&pt)) {
      RECT rect;
      if (hwnd)
        GetWindowRect(hwnd, &rect);
      pt.x = rect.left + (rect.right - rect.left) / 2 - 150;
      pt.y = rect.bottom - (rect.bottom - rect.top) / 2 - 100;
    }
    DEBUG << L"Using fallback position: (" << pt.x << L", " << pt.y << L")";
  }
  if (position_type != PositionType::kMousePos) {
    HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    RECT rcWork;
    mi.cbSize = sizeof(MONITORINFO);
    HWND panel = m_toy->UIHwnd();
    RECT rcPanel;
    if (panel)
      GetWindowRect(panel, &rcPanel);
    if (GetMonitorInfo(hMonitor, &mi)) {
      rcWork = mi.rcWork;
      int panelWidth = rcPanel.right - rcPanel.left;
      if (position_type == PositionType::kTopLeft) {
        pt.x = rcWork.left;
        pt.y = rcWork.top;
      } else if (position_type == PositionType::kTopCenter) {
        pt.x = rcWork.left + (rcWork.right - rcWork.left - panelWidth) / 2;
        pt.y = rcWork.top;
      } else if (position_type == PositionType::kTopRight) {
        pt.x = rcWork.right;
        pt.y = rcWork.top;
      } else if (position_type == PositionType::kBottomLeft) {
        pt.x = rcWork.left;
        pt.y = rcWork.bottom;
      } else if (position_type == PositionType::kBottomCenter) {
        pt.x = rcWork.left + (rcWork.right - rcWork.left - panelWidth) / 2;
        pt.y = rcWork.bottom;
      } else if (position_type == PositionType::kBottomRight) {
        pt.x = rcWork.right;
        pt.y = rcWork.bottom;
      } else if (position_type == PositionType::kCenter) {
        int panelHeight = rcPanel.bottom - rcPanel.top;
        pt.x = rcWork.left + (rcWork.right - rcWork.left - panelWidth) / 2;
        pt.y = rcWork.top + (rcWork.bottom - rcWork.top - panelHeight) / 2;
      }
    }
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
  HANDLE hDeployerMutex =
      CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
  if (::GetLastError() == ERROR_ALREADY_EXISTS ||
      ::GetLastError() == ERROR_ACCESS_DENIED)
    return 0;
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  HR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

  // 初始化光标跟踪器
  g_cursor_tracker = std::make_unique<weasel::CursorTracker>();
#ifdef _DEBUG
  g_cursor_tracker->EnableDebugOutput(true);
#endif

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

  // 清理光标跟踪器
  g_cursor_tracker.reset();

  CoUninitialize();
  return 0;
}
