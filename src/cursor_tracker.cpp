#include "cursor_tracker.h"
#include "accessibility_helper.h"
#include <algorithm>
#include <imm.h>
#include <oleacc.h>

namespace weasel {

CursorTracker::CursorTracker()
    : enabled_(true), update_threshold_(5), cache_timeout_ms_(50),
      debug_output_(false), last_target_window_(nullptr), call_count_(0),
      cache_hit_count_(0) {

  DebugLog(L"CursorTracker initialized");
}

CursorTracker::~CursorTracker() {
  if (debug_output_ && call_count_ > 0) {
    float cache_hit_rate = (float)cache_hit_count_ / call_count_ * 100.0f;
    DebugLog(L"CursorTracker stats - Calls: " + std::to_wstring(call_count_) +
             L", Cache hits: " + std::to_wstring(cache_hit_count_) +
             L", Hit rate: " + std::to_wstring(cache_hit_rate) + L"%");
  }
}

CursorPosition CursorTracker::GetCursorPosition(HWND targetWindow) {
  call_count_++;

  if (!enabled_) {
    CursorPosition pos;
    if (TryGetMousePosition(pos.point)) {
      pos.valid = true;
      pos.method = CursorDetectionMethod::MOUSE_FALLBACK;
      pos.targetWindow = targetWindow;
    }
    return pos;
  }

  // 检查缓存是否有效
  auto now = std::chrono::steady_clock::now();
  if (cached_position_.valid && cached_position_.targetWindow == targetWindow &&
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - cached_position_.timestamp)
              .count() < cache_timeout_ms_) {
    cache_hit_count_++;
    return cached_position_;
  }

  CursorPosition result;
  result.targetWindow = targetWindow;
  result.timestamp = now;

  // 按优先级尝试各种检测方法
  if (TryGetGUIThreadInfo(targetWindow, result.point)) {
    result.method = CursorDetectionMethod::GUI_THREAD_INFO;
    result.valid = true;
    DebugLog(L"Cursor found using GetGUIThreadInfo");
  } else if (TryGetIMEComposition(targetWindow, result.point)) {
    result.method = CursorDetectionMethod::IME_COMPOSITION;
    result.valid = true;
    DebugLog(L"Cursor found using IME Composition");
  } else if (TryGetCaretPos(targetWindow, result.point)) {
    result.method = CursorDetectionMethod::CARET_POS;
    result.valid = true;
    DebugLog(L"Cursor found using GetCaretPos");
  } else if (TryGetAccessibility(targetWindow, result.point)) {
    result.method = CursorDetectionMethod::ACCESSIBILITY;
    result.valid = true;
    DebugLog(L"Cursor found using Accessibility");
  } else if (TryGetMousePosition(result.point)) {
    result.method = CursorDetectionMethod::MOUSE_FALLBACK;
    result.valid = true;
    DebugLog(L"Fallback to mouse position");
  }

  if (result.valid) {
    // 调整位置以避免遮挡
    AdjustPositionForWindow(result.point, targetWindow);

    if (ShouldUpdatePosition(result)) {
      UpdateCache(result);
      DebugLog(L"Position updated to (" + std::to_wstring(result.point.x) +
               L", " + std::to_wstring(result.point.y) + L")");
    }
  }

  return result.valid ? result : cached_position_;
}

bool CursorTracker::TryGetGUIThreadInfo(HWND hwnd, POINT &pt) {
  if (!hwnd)
    return false;

  DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
  if (!threadId)
    return false;

  GUITHREADINFO gti = {};
  gti.cbSize = sizeof(GUITHREADINFO);

  if (!GetGUIThreadInfo(threadId, &gti)) {
    return false;
  }

  // 检查是否有活动的光标
  if (gti.flags & GUI_CARETBLINKING && gti.hwndCaret) {
    pt.x = gti.rcCaret.left;
    pt.y = gti.rcCaret.bottom + 2; // 光标下方2像素，避免遮挡

    // 转换为屏幕坐标
    if (ClientToScreen(gti.hwndCaret, &pt)) {
      return IsPositionValid(pt, hwnd);
    }
  }

  // 如果没有光标但有焦点窗口，尝试使用焦点窗口的位置
  if (gti.hwndFocus) {
    RECT rect;
    if (GetWindowRect(gti.hwndFocus, &rect)) {
      pt.x = rect.left + 10; // 窗口左上角偏移
      pt.y = rect.top + 25;
      return IsPositionValid(pt, hwnd);
    }
  }

  return false;
}

bool CursorTracker::TryGetIMEComposition(HWND hwnd, POINT &pt) {
  if (!hwnd)
    return false;

  HIMC hIMC = ImmGetContext(hwnd);
  if (!hIMC)
    return false;

  bool success = false;

  // 方法1: 获取合成窗口位置 (最准确)
  COMPOSITIONFORM cf = {};
  if (ImmGetCompositionWindow(hIMC, &cf)) {
    pt.x = cf.ptCurrentPos.x;
    pt.y = cf.ptCurrentPos.y + 20; // 合成位置下方

    if (ClientToScreen(hwnd, &pt) && IsPositionValid(pt, hwnd)) {
      success = true;
      DebugLog(L"IME position from CompositionWindow: (" +
               std::to_wstring(pt.x) + L", " + std::to_wstring(pt.y) + L")");
    }
  }

  // 方法2: 获取候选窗口位置 (如果方法1失败)
  if (!success) {
    CANDIDATEFORM candidateForm = {};
    if (ImmGetCandidateWindow(hIMC, 0, &candidateForm)) {
      pt.x = candidateForm.ptCurrentPos.x;
      pt.y = candidateForm.ptCurrentPos.y - 5; // 候选窗口上方

      if (ClientToScreen(hwnd, &pt) && IsPositionValid(pt, hwnd)) {
        success = true;
        DebugLog(L"IME position from CandidateWindow: (" +
                 std::to_wstring(pt.x) + L", " + std::to_wstring(pt.y) + L")");
      }
    }
  }

  // 方法3: 获取状态窗口位置 (最后尝试)
  if (!success) {
    POINT statusPos;
    if (ImmGetStatusWindowPos(hIMC, &statusPos)) {
      pt.x = statusPos.x;
      pt.y = statusPos.y + 30; // 状态窗口下方

      if (IsPositionValid(pt, hwnd)) {
        success = true;
        DebugLog(L"IME position from StatusWindow: (" + std::to_wstring(pt.x) +
                 L", " + std::to_wstring(pt.y) + L")");
      }
    }
  }

  // 方法4: 尝试获取输入上下文的字体信息来推算位置
  if (!success) {
    LOGFONT logFont;
    if (ImmGetCompositionFont(hIMC, &logFont)) {
      // 使用字体高度来估算合适的偏移
      int fontHeight = abs(logFont.lfHeight);
      if (fontHeight == 0)
        fontHeight = 16; // 默认字体高度

      // 获取窗口客户区左上角作为基准点
      RECT clientRect;
      if (GetClientRect(hwnd, &clientRect)) {
        pt.x = clientRect.left + 10;
        pt.y = clientRect.top + fontHeight + 5;

        if (ClientToScreen(hwnd, &pt) && IsPositionValid(pt, hwnd)) {
          success = true;
          DebugLog(L"IME position estimated from font info: (" +
                   std::to_wstring(pt.x) + L", " + std::to_wstring(pt.y) +
                   L")");
        }
      }
    }
  }

  ImmReleaseContext(hwnd, hIMC);
  return success;
}

bool CursorTracker::TryGetCaretPos(HWND hwnd, POINT &pt) {
  // GetCaretPos 只能在同一线程中使用
  DWORD currentThreadId = GetCurrentThreadId();
  DWORD targetThreadId = GetWindowThreadProcessId(hwnd, nullptr);

  if (currentThreadId != targetThreadId) {
    return false;
  }

  if (GetCaretPos(&pt)) {
    pt.y += 20; // 光标下方显示候选窗口
    return ClientToScreen(hwnd, &pt) && IsPositionValid(pt, hwnd);
  }

  return false;
}

bool CursorTracker::TryGetAccessibility(HWND hwnd, POINT &pt) {
  // 延迟初始化无障碍助手
  if (!accessibility_helper_) {
    accessibility_helper_ = std::make_unique<AccessibilityHelper>();
    if (debug_output_) {
      accessibility_helper_->EnableDebugOutput(true);
    }
  }

  if (accessibility_helper_->GetCaretPosition(hwnd, pt)) {
    DebugLog(L"Cursor found using Accessibility API at (" +
             std::to_wstring(pt.x) + L", " + std::to_wstring(pt.y) + L")");
    return IsPositionValid(pt, hwnd);
  }

  return false;
}

bool CursorTracker::TryGetMousePosition(POINT &pt) { return GetCursorPos(&pt); }

bool CursorTracker::IsPositionValid(const POINT &pt, HWND hwnd) {
  // 检查位置是否在屏幕范围内
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  if (pt.x < -100 || pt.x > screenWidth + 100 || pt.y < -100 ||
      pt.y > screenHeight + 100) {
    return false;
  }

  // 检查是否在多显示器环境中的有效区域
  HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
  if (!hMonitor) {
    return false;
  }

  return true;
}

bool CursorTracker::ShouldUpdatePosition(const CursorPosition &newPos) {
  if (!cached_position_.valid)
    return true;

  // 如果窗口改变，总是更新
  if (newPos.targetWindow != cached_position_.targetWindow) {
    return true;
  }

  // 检查位置变化是否超过阈值
  int dx = abs(newPos.point.x - cached_position_.point.x);
  int dy = abs(newPos.point.y - cached_position_.point.y);

  return (dx >= update_threshold_ || dy >= update_threshold_);
}

void CursorTracker::UpdateCache(const CursorPosition &pos) {
  cached_position_ = pos;
  last_target_window_ = pos.targetWindow;
}

void CursorTracker::AdjustPositionForWindow(POINT &pt, HWND hwnd) {
  if (!hwnd)
    return;

  // 获取目标窗口信息
  RECT windowRect;
  if (!GetWindowRect(hwnd, &windowRect))
    return;

  // 获取显示器工作区域
  HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(MONITORINFO);

  if (GetMonitorInfo(hMonitor, &mi)) {
    RECT &workArea = mi.rcWork;

    // 确保候选窗口不会超出工作区域
    // 假设候选窗口大小约为 300x150
    const int candidateWidth = 300;
    const int candidateHeight = 150;

    // 水平位置调整
    if (pt.x + candidateWidth > workArea.right) {
      pt.x = workArea.right - candidateWidth;
    }
    if (pt.x < workArea.left) {
      pt.x = workArea.left;
    }

    // 垂直位置调整
    if (pt.y + candidateHeight > workArea.bottom) {
      pt.y = pt.y - candidateHeight - 25; // 显示在光标上方
    }
    if (pt.y < workArea.top) {
      pt.y = workArea.top;
    }
  }
}

bool CursorTracker::IsPointInWindow(const POINT &pt, HWND hwnd) {
  if (!hwnd)
    return false;

  RECT rect;
  if (GetWindowRect(hwnd, &rect)) {
    return (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top &&
            pt.y <= rect.bottom);
  }

  return false;
}

void CursorTracker::DebugLog(const std::wstring &message) {
  if (debug_output_) {
    DEBUG << L"[CursorTracker] " << message;
  }
}

} // namespace weasel