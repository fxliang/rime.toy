#pragma once
#ifndef CURSOR_TRACKER_H
#define CURSOR_TRACKER_H

#include <chrono>
#include <memory>
#include <utils.h>
#include <windows.h>

// 前向声明
namespace weasel {
class AccessibilityHelper;
}

namespace weasel {

enum class CursorDetectionMethod {
  GUI_THREAD_INFO, // GetGUIThreadInfo - 最通用的方法
  IME_COMPOSITION, // ImmGetCompositionWindow - IME 专用
  ACCESSIBILITY,   // 无障碍接口 - 兼容性好
  CARET_POS,       // GetCaretPos - 同线程专用
  MOUSE_FALLBACK   // 鼠标位置回退 - 最后手段
};

enum class ApplicationType {
  UNKNOWN,
  TERMINAL,      // 终端应用 (Windows Terminal, CMD, PowerShell)
  BROWSER,       // 浏览器应用 (Chrome, Edge, Firefox)
  FILE_MANAGER,  // 文件管理器 (Explorer)
  OFFICE,        // Office 应用 (Word, Excel, PowerPoint)
  STANDARD_WIN32 // 标准 Win32 应用
};

struct CursorPosition {
  POINT point;                                     // 光标位置
  bool valid;                                      // 位置是否有效
  CursorDetectionMethod method;                    // 检测方法
  HWND targetWindow;                               // 目标窗口
  std::chrono::steady_clock::time_point timestamp; // 时间戳

  CursorPosition()
      : point{0, 0}, valid(false),
        method(CursorDetectionMethod::MOUSE_FALLBACK), targetWindow(nullptr),
        timestamp(std::chrono::steady_clock::now()) {}

  CursorPosition(const POINT &pt, CursorDetectionMethod m, HWND hwnd)
      : point(pt), valid(true), method(m), targetWindow(hwnd),
        timestamp(std::chrono::steady_clock::now()) {}
};

class CursorTracker {
public:
  CursorTracker();
  ~CursorTracker();

  // 主要接口
  CursorPosition GetCursorPosition(HWND targetWindow);

  // 配置接口
  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  void SetUpdateThreshold(int pixels) { update_threshold_ = pixels; }
  void SetCacheTimeout(int milliseconds) { cache_timeout_ms_ = milliseconds; }

  // 调试接口
  const CursorPosition &GetLastPosition() const { return cached_position_; }

private:
  // 各种检测方法实现
  bool TryGetGUIThreadInfo(HWND hwnd, POINT &pt);
  bool TryGetIMEComposition(HWND hwnd, POINT &pt);
  bool TryGetAccessibility(HWND hwnd, POINT &pt);
  bool TryGetCaretPos(HWND hwnd, POINT &pt);
  bool TryGetMousePosition(POINT &pt);

  // 辅助方法
  bool IsPositionValid(const POINT &pt, HWND hwnd);
  bool ShouldUpdatePosition(const CursorPosition &newPos);
  void UpdateCache(const CursorPosition &pos);

  // 位置调整方法
  void AdjustPositionForWindow(POINT &pt, HWND hwnd);
  bool IsPointInWindow(const POINT &pt, HWND hwnd);

  // 应用类型检测和专门处理
  ApplicationType DetectApplicationType(HWND hwnd);
  bool TryDetectByApplicationType(HWND hwnd, POINT &pt,
                                  ApplicationType appType);
  bool ValidateDetectionResult(const POINT &pt, HWND hwnd,
                               ApplicationType appType);

  // 应用特定的检测方法
  bool TryTerminalSpecific(HWND hwnd, POINT &pt);
  bool TryBrowserSpecific(HWND hwnd, POINT &pt);
  bool TryFileManagerSpecific(HWND hwnd, POINT &pt);

private:
  // 配置参数
  bool enabled_;         // 是否启用光标跟踪
  int update_threshold_; // 位置变化阈值(像素)
  int cache_timeout_ms_; // 缓存超时时间(毫秒)

  // 状态数据
  CursorPosition cached_position_; // 缓存的光标位置
  HWND last_target_window_;        // 上次的目标窗口

  // 无障碍接口助手
  std::unique_ptr<AccessibilityHelper> accessibility_helper_;

  // 性能统计
  mutable int call_count_;      // 调用次数
  mutable int cache_hit_count_; // 缓存命中次数
};

} // namespace weasel

#endif // CURSOR_TRACKER_H