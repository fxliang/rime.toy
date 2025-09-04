#pragma once
#ifndef ACCESSIBILITY_HELPER_H
#define ACCESSIBILITY_HELPER_H

#include <memory>
#include <oleacc.h>
#include <uiautomation.h>
#include <utils.h>
#include <windows.h>
#include <wrl.h>

namespace weasel {

using namespace Microsoft::WRL;

class AccessibilityHelper {
public:
  AccessibilityHelper();
  ~AccessibilityHelper();

  // 主要接口
  bool Initialize();
  bool GetCaretPosition(HWND hwnd, POINT &pt);
  bool IsInitialized() const { return m_initialized; }

private:
  // 各种无障碍检测方法
  bool TryUIAutomation(HWND hwnd, POINT &pt);
  bool TryMSAA(HWND hwnd, POINT &pt);
  bool TryTextPattern(HWND hwnd, POINT &pt);
  bool TryAccessibleObjectFromWindow(HWND hwnd, POINT &pt);

  // 辅助方法
  bool IsValidPosition(const POINT &pt);

  // UI Automation 相关方法
  bool GetFocusedElementRect(RECT &rect);
  bool GetTextSelectionRange(HWND hwnd, POINT &pt);

private:
  // COM 接口
  ComPtr<IUIAutomation> m_pUIAutomation;
  ComPtr<IAccessible> m_pAccessible;

  // 状态标志
  bool m_initialized;

  // 性能统计
  mutable int call_count_;
  mutable int success_count_;
};

} // namespace weasel

#endif // ACCESSIBILITY_HELPER_H