#include "cursor_tracker.h"
#include "accessibility_helper.h"
#include <algorithm>
#include <cctype>
#include <imm.h>
#include <locale>
#include <oleacc.h>
#include <psapi.h>
#include <utils.h>

namespace weasel {

CursorTracker::CursorTracker()
    : enabled_(true), update_threshold_(5), cache_timeout_ms_(50),
      last_target_window_(nullptr), call_count_(0), cache_hit_count_(0) {

  DEBUG << "CursorTracker initialized";
}

CursorTracker::~CursorTracker() {
  if (call_count_ > 0) {
    float cache_hit_rate = (float)cache_hit_count_ / call_count_ * 100.0f;
    DEBUG << "CursorTracker stats - Calls: " << call_count_
          << ", Cache hits: " << cache_hit_count_
          << ", Hit rate: " << cache_hit_rate << "%";
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

  // 检测应用类型并使用相应的检测策略
  ApplicationType appType = DetectApplicationType(targetWindow);
  DEBUG << "Detected application type: " << (int)appType;

  // 根据应用类型选择最佳检测方法
  if (TryDetectByApplicationType(targetWindow, result.point, appType)) {
    // 验证检测结果
    if (ValidateDetectionResult(result.point, targetWindow, appType)) {
      result.valid = true;
      DEBUG << "Cursor found using app-specific method";
    } else {
      DEBUG << "App-specific detection result invalid, trying fallback";
      // 如果应用特定方法失败，尝试鼠标位置
      if (TryGetMousePosition(result.point)) {
        result.method = CursorDetectionMethod::MOUSE_FALLBACK;
        result.valid = true;
        DEBUG << "Using mouse position as fallback";
      }
    }
  } else {
    // 应用特定检测失败，使用原有的通用检测链
    if (TryGetGUIThreadInfo(targetWindow, result.point)) {
      result.method = CursorDetectionMethod::GUI_THREAD_INFO;
      result.valid = true;
      DEBUG << "Cursor found using GetGUIThreadInfo";
    } else if (TryGetIMEComposition(targetWindow, result.point)) {
      result.method = CursorDetectionMethod::IME_COMPOSITION;
      result.valid = true;
      DEBUG << "Cursor found using IME Composition";
    } else if (TryGetCaretPos(targetWindow, result.point)) {
      result.method = CursorDetectionMethod::CARET_POS;
      result.valid = true;
      DEBUG << "Cursor found using GetCaretPos";
    } else if (TryGetAccessibility(targetWindow, result.point)) {
      result.method = CursorDetectionMethod::ACCESSIBILITY;
      result.valid = true;
      DEBUG << "Cursor found using Accessibility";
    } else if (TryGetMousePosition(result.point)) {
      result.method = CursorDetectionMethod::MOUSE_FALLBACK;
      result.valid = true;
      DEBUG << "Fallback to mouse position";
    }
  }

  if (result.valid) {
    // 调整位置以避免遮挡
    AdjustPositionForWindow(result.point, targetWindow);

    if (ShouldUpdatePosition(result)) {
      UpdateCache(result);
      DEBUG << "Position updated to (" << result.point.x << ", "
            << result.point.y << ")";
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
      DEBUG << "IME position from CompositionWindow: (" << pt.x << ", " << pt.y
            << ")";
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
        DEBUG << "IME position from CandidateWindow: (" << pt.x << ", " << pt.y
              << ")";
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
        DEBUG << "IME position from StatusWindow: (" << pt.x << ", " << pt.y
              << ")";
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
          DEBUG << "IME position estimated from font info: (" << pt.x << ", "
                << pt.y << ")";
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
  }

  if (accessibility_helper_->GetCaretPosition(hwnd, pt)) {
    DEBUG << "Cursor found using Accessibility API at (" << pt.x << ", " << pt.y
          << ")";
    return IsPositionValid(pt, hwnd);
  }

  return false;
}

bool CursorTracker::TryGetMousePosition(POINT &pt) { return GetCursorPos(&pt); }

bool CursorTracker::IsPositionValid(const POINT &pt, HWND hwnd) {
  // 1. 拒绝接近原点的垃圾值
  if (pt.x <= 10 && pt.y <= 10) {
    return false;
  }

  // 2. 确保点在某个有效的显示器上
  if (MonitorFromPoint(pt, MONITOR_DEFAULTTONULL) == NULL) {
    return false;
  }

  // 3. 检查是否在虚拟屏幕的合理范围内 (可选，但可以防止极端异常值)
  int virtualScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int virtualScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int virtualScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int virtualScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

  RECT virtualScreenRect = {virtualScreenX, virtualScreenY,
                          virtualScreenX + virtualScreenWidth,
                          virtualScreenY + virtualScreenHeight};

  // 允许一定的边界外区域
  InflateRect(&virtualScreenRect, 100, 100);

  return PtInRect(&virtualScreenRect, pt);
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

ApplicationType CursorTracker::DetectApplicationType(HWND hwnd) {
  if (!hwnd)
    return ApplicationType::UNKNOWN;

  wchar_t className[256] = {0};
  wchar_t windowTitle[256] = {0};
  wchar_t processName[256] = {0};

  GetClassName(hwnd, className, 255);
  GetWindowText(hwnd, windowTitle, 255);

  // 获取进程名
  DWORD processId;
  GetWindowThreadProcessId(hwnd, &processId);
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                FALSE, processId);
  if (hProcess) {
    GetModuleBaseName(hProcess, NULL, processName, 255);
    CloseHandle(hProcess);
  }

  std::wstring classStr(className);
  std::wstring titleStr(windowTitle);
  std::wstring procStr(processName);

  // 转换为小写进行比较
  std::transform(classStr.begin(), classStr.end(), classStr.begin(),
                 ::towlower);
  std::transform(titleStr.begin(), titleStr.end(), titleStr.begin(),
                 ::towlower);
  std::transform(procStr.begin(), procStr.end(), procStr.begin(), ::towlower);

  // 终端应用检测
  if (procStr.find(L"windowsterminal") != std::wstring::npos ||
      procStr.find(L"conhost") != std::wstring::npos ||
      procStr.find(L"cmd") != std::wstring::npos ||
      procStr.find(L"powershell") != std::wstring::npos ||
      classStr.find(L"consolewindowclass") != std::wstring::npos) {
    return ApplicationType::TERMINAL;
  }

  // 浏览器应用检测
  if (procStr.find(L"chrome") != std::wstring::npos ||
      procStr.find(L"msedge") != std::wstring::npos ||
      procStr.find(L"firefox") != std::wstring::npos ||
      procStr.find(L"iexplore") != std::wstring::npos ||
      classStr.find(L"chrome") != std::wstring::npos ||
      classStr.find(L"mozilla") != std::wstring::npos) {
    return ApplicationType::BROWSER;
  }

  // 文件管理器检测
  if (procStr.find(L"explorer") != std::wstring::npos ||
      classStr.find(L"cabinetwclass") != std::wstring::npos ||
      classStr.find(L"explorerframe") != std::wstring::npos) {
    return ApplicationType::FILE_MANAGER;
  }

  // Office 应用检测
  if (procStr.find(L"winword") != std::wstring::npos ||
      procStr.find(L"excel") != std::wstring::npos ||
      procStr.find(L"powerpnt") != std::wstring::npos ||
      procStr.find(L"outlook") != std::wstring::npos) {
    return ApplicationType::OFFICE;
  }

  return ApplicationType::STANDARD_WIN32;
}

bool CursorTracker::TryDetectByApplicationType(HWND hwnd, POINT &pt,
                                               ApplicationType appType) {
  switch (appType) {
  case ApplicationType::TERMINAL:
    return TryTerminalSpecific(hwnd, pt);

  case ApplicationType::BROWSER:
    return TryBrowserSpecific(hwnd, pt);

  case ApplicationType::FILE_MANAGER:
    return TryFileManagerSpecific(hwnd, pt);

  case ApplicationType::OFFICE:
    // Office 应用优先使用 IME 检测
    return TryGetIMEComposition(hwnd, pt) || TryGetGUIThreadInfo(hwnd, pt);

  case ApplicationType::STANDARD_WIN32:
    // 标准应用使用完整检测链
    return TryGetGUIThreadInfo(hwnd, pt) || TryGetIMEComposition(hwnd, pt);

  default:
    return false;
  }
}

bool CursorTracker::ValidateDetectionResult(const POINT &pt, HWND hwnd,
                                            ApplicationType appType) {
  // 检查 (0,0) 位置 - 通常是无效的
  if (pt.x == 0 && pt.y == 0) {
    DEBUG << "Invalid position (0,0) detected";
    return false;
  }

  // 检查是否在合理的屏幕范围内
  if (!IsPositionValid(pt, hwnd)) {
    DEBUG << "Position outside valid screen area";
    return false;
  }

  // 对于终端应用，允许更宽松的验证
  if (appType == ApplicationType::TERMINAL) {
    return true;
  }

  // 检查位置是否在窗口范围内（允许一定的偏移）
  RECT windowRect;
  if (GetWindowRect(hwnd, &windowRect)) {
    // 扩展窗口边界，允许候选窗口显示在窗口外
    InflateRect(&windowRect, 200, 200);

    if (!PtInRect(&windowRect, pt)) {
      DEBUG << "Position too far from window bounds";
      return false;
    }
  }

  return true;
}

bool CursorTracker::TryTerminalSpecific(HWND hwnd, POINT &pt) {
  // 终端应用的特殊处理
  // 1. 首先尝试获取控制台光标位置
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hConsole != INVALID_HANDLE_VALUE &&
      GetConsoleScreenBufferInfo(hConsole, &csbi)) {
    // 获取控制台窗口位置
    RECT consoleRect;
    if (GetWindowRect(hwnd, &consoleRect)) {
      // 估算字符位置
      int charWidth = 8;   // 估算字符宽度
      int charHeight = 16; // 估算字符高度

      pt.x = consoleRect.left + csbi.dwCursorPosition.X * charWidth + 10;
      pt.y = consoleRect.top + csbi.dwCursorPosition.Y * charHeight + 30;

      DEBUG << "Terminal cursor from console API: (" << pt.x << ", " << pt.y
            << ")";
      return true;
    }
  }

  // 2. 如果控制台 API 失败，使用鼠标位置
  if (TryGetMousePosition(pt)) {
    DEBUG << "Terminal using mouse position fallback";
    return true;
  }

  return false;
}

bool CursorTracker::TryBrowserSpecific(HWND hwnd, POINT &pt) {
  // 浏览器应用的特殊处理
  // 1. 优先使用无障碍接口
  if (TryGetAccessibility(hwnd, pt)) {
    DEBUG << "Browser cursor from accessibility API";
    return true;
  }

  // 2. 尝试 GUI 线程信息，但需要额外验证
  if (TryGetGUIThreadInfo(hwnd, pt)) {
    // 对于浏览器，GUI 线程信息可能不准确，需要额外调整
    RECT windowRect;
    if (GetWindowRect(hwnd, &windowRect)) {
      // 如果位置在窗口顶部区域（可能是地址栏），进行调整
      if (pt.y < windowRect.top + 100) {
        pt.y = windowRect.top + 80; // 调整到地址栏下方
      }
    }
    DEBUG << "Browser cursor from GUI thread info (adjusted)";
    return true;
  }

  // 3. 最后使用鼠标位置
  if (TryGetMousePosition(pt)) {
    DEBUG << "Browser using mouse position fallback";
    return true;
  }

  return false;
}

bool CursorTracker::TryFileManagerSpecific(HWND hwnd, POINT &pt) {
  // 文件管理器的特殊处理
  // 避免使用 IME 检测，因为可能干扰正常检测

  // 1. 优先使用 GUI 线程信息
  if (TryGetGUIThreadInfo(hwnd, pt)) {
    DEBUG << "File manager cursor from GUI thread info";
    return true;
  }

  // 2. 尝试无障碍接口
  if (TryGetAccessibility(hwnd, pt)) {
    DEBUG << "File manager cursor from accessibility API";
    return true;
  }

  // 3. 使用鼠标位置作为回退
  if (TryGetMousePosition(pt)) {
    DEBUG << "File manager using mouse position fallback";
    return true;
  }

  return false;
}

} // namespace weasel