#pragma once
#include <WeaselIPCData.h>
#include <string>
#include <windows.h>
#include <wrl.h>

namespace weasel {
using namespace Microsoft::WRL;
using wstring = std::wstring;
using string = std::string;

struct ComException {
  HRESULT result;
  ComException(HRESULT const value) : result(value) {}
};
inline void HR(HRESULT const result) {
  if (S_OK != result) {
    throw ComException(result);
  }
}

class UIImpl;
class UI {
public:
  UI() : pimpl_(nullptr) {}
  virtual ~UI() {
    if (pimpl_)
      Destroy(true);
  }
  // 创建输入法界面
  bool Create(HWND parent);
  // 销毁界面
  void Destroy(bool full = false);
  // 界面显隐
  void Show();
  void Hide();
  void ShowWithTimeout(size_t millisec);
  BOOL IsCountingDown() const;
  BOOL IsShown() const;
  // 重绘界面
  void Refresh();
  // 置输入焦点位置（光标跟随时移动候选窗）但不重绘
  void UpdateInputPosition(RECT const &rc);
  // 更新界面显示内容
  void Update(Context const &ctx, Status const &status);
  Context &ctx() { return ctx_; }
  Context &octx() { return octx_; }
  Status &status() { return status_; }
  BOOL &horizontal() { return m_horizontal; }
  UIStyle &style() { return style_; }
  UIStyle &ostyle() { return ostyle_; }
  void SetText(const wstring &text);
  void SetHorizontal(bool hor);
  // bool GetIsReposition();

private:
  UIImpl *pimpl_;
  Context ctx_;
  Context octx_;
  Status status_;
  BOOL m_horizontal;
  UIStyle style_;
  UIStyle ostyle_;
};
} // namespace weasel
