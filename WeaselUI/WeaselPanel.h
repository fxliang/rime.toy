#include <WeaselUI.h>

#include "d2d.h"
#include <utils.h>

namespace weasel {

class WeaselPanel {
public:
  WeaselPanel(UI &ui);
  ~WeaselPanel() {}
  void MoveTo(RECT rc);
  void Refresh();

  BOOL IsWindow() const;
  void ShowWindow(int nCmdShow);
  void DestroyWindow();
  BOOL Create(HWND parent);

  HWND m_hWnd;

private:
  void FillRoundRect(const RECT &rect, float radius, uint32_t border,
                     uint32_t back_color, uint32_t border_color);
  D2D1::ColorF D2d1ColorFromColorRef(uint32_t color);
  void Render();
  void DrawTextAt(const wstring &text, size_t x, size_t y,
                  ComPtr<IDWriteTextFormat1> &pTextFormat);
  void ResizeVertical();
  void ResizeHorizontal();
  void DrawHorizontal();
  void DrawUIVertical();
  void OnPaint();
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);

  wstring m_text;
  SIZE m_winSize{0, 0};
  RECT m_inputPos{0, 0, 0, 0};
  float m_dpiScale = 96.0f / 72.0f;

  Context &m_ctx;
  Status &m_status;
  UIStyle &m_style;
  UIStyle &m_ostyle;
  UINT m_padding = 10;
  BOOL &m_horizontal;
  // ------------------------------------------------------------
  std::shared_ptr<D2D> m_pD2D;
};
} // namespace weasel
