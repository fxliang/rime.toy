#include "WeaselPanel.h"
#include <WeaselUI.h>

namespace weasel {
// ----------------------------------------------------------------------------
class UIImpl {
public:
  WeaselPanel panel;
  UIImpl(UI &ui) : panel(ui), shown(false) {}
  ~UIImpl() {}
  bool IsShown() { return shown; }
  void Refresh() {
    if (!panel.IsWindow())
      return;
    if (timer) {
      Hide();
      KillTimer(panel.m_hWnd, AUTOHIDE_TIMER);
      timer = 0;
    }
    panel.Refresh();
  }
  void Show() {
    if (!panel.IsWindow())
      return;
    panel.ShowWindow(SW_SHOWNA);
    shown = true;
    if (timer) {
      KillTimer(panel.m_hWnd, AUTOHIDE_TIMER);
      timer = 0;
    }
  }
  void Hide() {
    if (!panel.IsWindow())
      return;
    panel.ShowWindow(SW_HIDE);
    shown = false;
    if (timer) {
      KillTimer(panel.m_hWnd, AUTOHIDE_TIMER);
      timer = 0;
    }
  }
  void ShowWithTimeout(size_t millisec);
  bool IsShown() const { return shown; }
  static VOID CALLBACK OnTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent, _In_ DWORD dwTime);
  static const int AUTOHIDE_TIMER = 20241107;
  static UINT_PTR timer;
  bool shown;
};
UINT_PTR UIImpl::timer = 0;

void UIImpl::ShowWithTimeout(size_t millisec) {
  if (!panel.IsWindow())
    return;
  panel.ShowWindow(SW_SHOWNA);
  shown = true;
  SetTimer(panel.m_hWnd, AUTOHIDE_TIMER, static_cast<UINT>(millisec),
           &UIImpl::OnTimer);
  timer = UINT_PTR(this);
}
VOID CALLBACK UIImpl::OnTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                              _In_ UINT_PTR idEvent, _In_ DWORD dwTime) {
  KillTimer(hwnd, idEvent);
  UIImpl *self = (UIImpl *)timer;
  timer = 0;
  if (self) {
    self->Hide();
    self->shown = false;
  }
}
// ----------------------------------------------------------------------------
BOOL UI::IsCountingDown() const { return pimpl_ && pimpl_->timer != 0; };
BOOL UI::IsShown() const { return pimpl_ && pimpl_->IsShown(); }
void UI::UpdateInputPosition(RECT const &rc) {
  if (pimpl_ && pimpl_->panel.IsWindow()) {
    pimpl_->panel.MoveTo(rc);
  }
}
void UI::Update(const Context &ctx, const Status &status) {
  if (ctx_ == ctx && status_ == status)
    return;
  ctx_ = ctx;
  status_ = status;
  Refresh();
}
void UI::Refresh() {
  if (ctx_.empty())
    pimpl_->Hide();
  if (ctx_ != octx_)
    if (pimpl_) {
      pimpl_->Refresh();
    }
}
void UI::ShowWithTimeout(size_t millisec) {
  if (pimpl_) {
    pimpl_->ShowWithTimeout(millisec);
  }
}
void UI::Show() {
  if (pimpl_) {
    pimpl_->Show();
  }
}
void UI::Hide() {
  if (pimpl_) {
    pimpl_->Hide();
  }
}
void UI::Destroy(bool full) {
  if (pimpl_) {
    if (pimpl_->panel.IsWindow())
      pimpl_->panel.DestroyWindow();
    if (full) {
      delete pimpl_;
      pimpl_ = nullptr;
    }
  }
}
void UI::SetText(const wstring &text) {
  ctx_.clear();
  ctx_.aux.str = text;
  Refresh();
}
void UI::SetHorizontal(bool hor) { m_horizontal = hor; }
bool UI::Create(HWND parent) {
  if (pimpl_) {
    return pimpl_->panel.Create(parent);
  }
  pimpl_ = new UIImpl(*this);
  if (!pimpl_)
    return false;
  return pimpl_->panel.Create(parent);
}

} // namespace weasel
