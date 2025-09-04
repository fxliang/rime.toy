#include "accessibility_helper.h"
#include <comdef.h>
#include <utils.h>

namespace weasel {

AccessibilityHelper::AccessibilityHelper()
    : m_initialized(false), call_count_(0), success_count_(0) {}

AccessibilityHelper::~AccessibilityHelper() {
  if (call_count_ > 0) {
    float success_rate = (float)success_count_ / call_count_ * 100.0f;
    DEBUG << "AccessibilityHelper stats - Calls: " << call_count_
          << ", Success: " << success_count_
          << ", Success rate: " << success_rate << "%";
  }
}

bool AccessibilityHelper::Initialize() {
  if (m_initialized)
    return true;

  // 初始化 COM (如果尚未初始化)
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    DEBUG << "Failed to initialize COM: " << hr;
    return false;
  }

  // 创建 UI Automation 实例
  hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&m_pUIAutomation));

  if (SUCCEEDED(hr)) {
    m_initialized = true;
    DEBUG << "UI Automation initialized successfully";
  } else {
    DEBUG << "Failed to initialize UI Automation: " << hr;
    // 即使 UI Automation 失败，我们仍然可以尝试其他方法
    m_initialized = true;
  }

  return m_initialized;
}

bool AccessibilityHelper::GetCaretPosition(HWND hwnd, POINT &pt) {
  call_count_++;

  if (!Initialize())
    return false;

  // 按优先级尝试不同的无障碍方法
  if (TryUIAutomation(hwnd, pt)) {
    success_count_++;
    DEBUG << "Caret position found using UI Automation";
    return true;
  }

  if (TryAccessibleObjectFromWindow(hwnd, pt)) {
    success_count_++;
    DEBUG << "Caret position found using AccessibleObjectFromWindow";
    return true;
  }

  if (TryTextPattern(hwnd, pt)) {
    success_count_++;
    DEBUG << "Caret position found using Text Pattern";
    return true;
  }

  if (TryMSAA(hwnd, pt)) {
    success_count_++;
    DEBUG << "Caret position found using MSAA";
    return true;
  }

  return false;
}

bool AccessibilityHelper::TryUIAutomation(HWND hwnd, POINT &pt) {
  if (!m_pUIAutomation)
    return false;

  try {
    // 从窗口句柄获取元素
    ComPtr<IUIAutomationElement> element;
    HRESULT hr = m_pUIAutomation->ElementFromHandle(hwnd, &element);
    if (FAILED(hr))
      return false;

    // 获取当前焦点元素
    ComPtr<IUIAutomationElement> focusedElement;
    hr = m_pUIAutomation->GetFocusedElement(&focusedElement);
    if (FAILED(hr)) {
      // 如果无法获取焦点元素，尝试使用传入的窗口元素
      focusedElement = element;
    }

    // 获取元素的边界矩形
    RECT rect;
    hr = focusedElement->get_CurrentBoundingRectangle(&rect);
    if (SUCCEEDED(hr)) {
      // 使用矩形的左下角作为光标位置
      pt.x = rect.left + 5;
      pt.y = rect.bottom + 2;

      if (IsValidPosition(pt)) {
        return true;
      }
    }

    // 尝试获取文本模式
    ComPtr<IUnknown> patternObject;
    hr = focusedElement->GetCurrentPattern(UIA_TextPatternId, &patternObject);
    if (SUCCEEDED(hr) && patternObject) {
      ComPtr<IUIAutomationTextPattern> textPattern;
      hr = patternObject.As(&textPattern);
      if (SUCCEEDED(hr)) {
        // 获取选择范围
        ComPtr<IUIAutomationTextRangeArray> selectionRanges;
        hr = textPattern->GetSelection(&selectionRanges);
        if (SUCCEEDED(hr)) {
          int rangeCount;
          hr = selectionRanges->get_Length(&rangeCount);
          if (SUCCEEDED(hr) && rangeCount > 0) {
            ComPtr<IUIAutomationTextRange> firstRange;
            hr = selectionRanges->GetElement(0, &firstRange);
            if (SUCCEEDED(hr)) {
              // 获取范围的边界矩形
              SAFEARRAY *rectArray;
              hr = firstRange->GetBoundingRectangles(&rectArray);
              if (SUCCEEDED(hr) && rectArray) {
                double *rects = nullptr;
                hr = SafeArrayAccessData(rectArray, (void **)&rects);
                if (SUCCEEDED(hr) && rects) {
                  pt.x = (LONG)rects[0] + 2; // left + 2
                  pt.y =
                      (LONG)rects[1] + (LONG)rects[3] + 2; // top + height + 2

                  SafeArrayUnaccessData(rectArray);
                  SafeArrayDestroy(rectArray);

                  if (IsValidPosition(pt)) {
                    return true;
                  }
                }
              }
            }
          }
        }
      }
    }
  } catch (...) {
    DEBUG << "Exception in TryUIAutomation";
  }

  return false;
}

bool AccessibilityHelper::TryAccessibleObjectFromWindow(HWND hwnd, POINT &pt) {
  try {
    // 尝试获取光标对象
    ComPtr<IAccessible> accessible;
    VARIANT varChild;
    VariantInit(&varChild);

    HRESULT hr = AccessibleObjectFromWindow(hwnd, OBJID_CARET, IID_IAccessible,
                                            (void **)&accessible);

    if (SUCCEEDED(hr) && accessible) {
      LONG left, top, width, height;
      hr = accessible->accLocation(&left, &top, &width, &height, varChild);

      if (SUCCEEDED(hr)) {
        pt.x = left;
        pt.y = top + height + 2; // 光标下方

        if (IsValidPosition(pt)) {
          return true;
        }
      }
    }

    // 如果获取光标失败，尝试获取焦点对象
    hr = AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, IID_IAccessible,
                                    (void **)&accessible);

    if (SUCCEEDED(hr) && accessible) {
      ComPtr<IDispatch> focusedChild;
      VARIANT varFocused;
      VariantInit(&varFocused);

      hr = accessible->get_accFocus(&varFocused);
      if (SUCCEEDED(hr)) {
        if (varFocused.vt == VT_DISPATCH && varFocused.pdispVal) {
          ComPtr<IAccessible> focusedAccessible;
          hr = varFocused.pdispVal->QueryInterface(IID_IAccessible,
                                                   (void **)&focusedAccessible);
          if (SUCCEEDED(hr)) {
            LONG left, top, width, height;
            VARIANT varChild;
            VariantInit(&varChild);
            varChild.vt = VT_I4;
            varChild.lVal = CHILDID_SELF;

            hr = focusedAccessible->accLocation(&left, &top, &width, &height,
                                                varChild);
            if (SUCCEEDED(hr)) {
              pt.x = left + 5;
              pt.y = top + height + 2;

              if (IsValidPosition(pt)) {
                VariantClear(&varFocused);
                return true;
              }
            }
          }
        }
      }
      VariantClear(&varFocused);
    }
  } catch (...) {
    DEBUG << "Exception in TryAccessibleObjectFromWindow";
  }

  return false;
}

bool AccessibilityHelper::TryTextPattern(HWND hwnd, POINT &pt) {
  // 这个方法主要用于支持现代应用的文本输入
  // 暂时返回 false，后续可以根据需要完善
  return false;
}

bool AccessibilityHelper::TryMSAA(HWND hwnd, POINT &pt) {
  try {
    // 使用传统的 MSAA 接口获取窗口信息
    ComPtr<IAccessible> accessible;
    VARIANT varChild;
    VariantInit(&varChild);
    varChild.vt = VT_I4;
    varChild.lVal = CHILDID_SELF;

    HRESULT hr = AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                            (void **)&accessible);

    if (SUCCEEDED(hr) && accessible) {
      // 获取窗口的位置信息
      LONG left, top, width, height;
      hr = accessible->accLocation(&left, &top, &width, &height, varChild);

      if (SUCCEEDED(hr)) {
        // 使用窗口左上角加上一些偏移作为估算位置
        pt.x = left + 20;
        pt.y = top + 40;

        if (IsValidPosition(pt)) {
          return true;
        }
      }
    }
  } catch (...) {
    DEBUG << "Exception in TryMSAA";
  }

  return false;
}

bool AccessibilityHelper::IsValidPosition(const POINT &pt) {
  // 检查位置是否在合理的屏幕范围内
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  return (pt.x >= -100 && pt.x <= screenWidth + 100 && pt.y >= -100 &&
          pt.y <= screenHeight + 100);
}

} // namespace weasel