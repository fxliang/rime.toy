#include "WeaselPanel.h"
#include <DWrite.h>
#include <dwrite_2.h>
#include <map>
#include <regex>
#include <string>
#include <utils.h>
#include <vector>
#include <wrl/client.h>

using namespace weasel;

WeaselPanel::WeaselPanel(UI &ui)
    : m_hWnd(nullptr), m_ctx(ui.ctx()), m_status(ui.status()),
      m_style(ui.style()), m_ostyle(ui.ostyle()) {
  Create(nullptr);
}

BOOL WeaselPanel::IsWindow() const { return ::IsWindow(m_hWnd); }

void WeaselPanel::ShowWindow(int nCmdShow) { ::ShowWindow(m_hWnd, nCmdShow); }

void WeaselPanel::DestroyWindow() { ::DestroyWindow(m_hWnd); }

void WeaselPanel::MoveTo(RECT rc) {
  if (m_hWnd) {
    m_inputPos = rc;
    RECT rcWorkArea;
    memset(&rcWorkArea, 0, sizeof(rcWorkArea));
    HMONITOR hMonitor = MonitorFromRect(&m_inputPos, MONITOR_DEFAULTTONEAREST);
    if (hMonitor) {
      MONITORINFO info;
      info.cbSize = sizeof(MONITORINFO);
      if (GetMonitorInfo(hMonitor, &info)) {
        rcWorkArea = info.rcWork;
      }
    }
    RECT rcWindow;
    GetWindowRect(m_hWnd, &rcWindow);
    int width = (rcWindow.right - rcWindow.left);
    int height = (rcWindow.bottom - rcWindow.top);
    rcWorkArea.right -= width;
    rcWorkArea.bottom -= height;
    int x = m_inputPos.left;
    int y = m_inputPos.bottom;
    if (x > rcWorkArea.right)
      x = rcWorkArea.right;
    if (x < rcWorkArea.left)
      x = rcWorkArea.right;
    if (y > rcWorkArea.bottom)
      y = rcWorkArea.bottom;
    if (y < rcWorkArea.top)
      y = rcWorkArea.top;
    m_inputPos.bottom = y;

    SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
  }
}

void WeaselPanel::Refresh() {
  if (m_hWnd) {
    if (m_ostyle != m_style) {
      DEBUG << "style changed";
      m_ostyle = m_style;
      m_pD2D->m_style = m_style;
      m_pD2D->InitDpiInfo();
      m_pD2D->InitFontFormats();
    }
    InvalidateRect(m_hWnd, NULL, TRUE); // 请求重绘
  }
}

BOOL WeaselPanel::Create(HWND parent) {
  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"PopupWindowClass";
  RegisterClass(&wc);
  m_hWnd = CreateWindowEx(
      WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST,
      L"PopupWindowClass", L"PopupWindowPanel", WS_POPUP | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1920, parent, nullptr,
      GetModuleHandle(nullptr), this);
  if (m_hWnd) {
    m_pD2D.reset(new D2D(m_style, m_hWnd));
    m_ostyle = m_style;
  }
  UpdateWindow(m_hWnd);
  return !!m_hWnd;
}

void WeaselPanel::FillRoundRect(const RECT &rect, float radius, uint32_t border,
                                uint32_t color, uint32_t border_color) {
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float b = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float r = (color & 0xFF) / 255.0f;
  // Define a rounded rectangle
  ComPtr<ID2D1RoundedRectangleGeometry> roundedRectGeometry;
  D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
      D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                  static_cast<float>(rect.right),
                  static_cast<float>(rect.bottom)),
      radius, // radiusX
      radius  // radiusY
  );
  HR(m_pD2D->d2Factory->CreateRoundedRectangleGeometry(roundedRect,
                                                       &roundedRectGeometry));
  ComPtr<ID2D1SolidColorBrush> brush;
  HR(m_pD2D->dc->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), &brush));
  m_pD2D->dc->FillGeometry(roundedRectGeometry.Get(), brush.Get());
  float hb = (float)border / 2;
  D2D1_ROUNDED_RECT borderRect =
      D2D1::RoundedRect(D2D1::RectF(static_cast<float>(rect.left) + hb,
                                    static_cast<float>(rect.top) + hb,
                                    static_cast<float>(rect.right) - hb,
                                    static_cast<float>(rect.bottom) - hb),
                        radius - hb, // radiusX
                        radius - hb  // radiusY
      );
  brush->SetColor(D2d1ColorFromColorRef(border_color));
  m_pD2D->dc->DrawRoundedRectangle(&borderRect, brush.Get(), m_style.border);
}

D2D1::ColorF WeaselPanel::D2d1ColorFromColorRef(uint32_t color) {
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float b = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float r = (color & 0xFF) / 255.0f;
  return D2D1::ColorF(r, g, b, a);
}

void WeaselPanel::Render() {
  m_pD2D->dc->BeginDraw();
  m_pD2D->dc->Clear(D2D1::ColorF({0.0f, 0.0f, 0.0f, 0.0f}));
  FillRoundRect({0, 0, m_winSize.cx, m_winSize.cy},
                (float)m_style.round_corner_ex, m_style.border,
                m_style.back_color, m_style.border_color);
  if (m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL) {
    DrawHorizontal();
  } else {
    DrawUIVertical();
  }
  HR(m_pD2D->dc->EndDraw());
  // Make the swap chain available to the composition engine
  HR(m_pD2D->swapChain->Present(1, 0)); // sync
}

void WeaselPanel::DrawTextAt(const wstring &text, size_t x, size_t y,
                             ComPtr<IDWriteTextFormat1> &pTextFormat) {
  ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
  HR(m_pD2D->m_pWriteFactory->CreateTextLayout(
      text.c_str(), text.length(), pTextFormat.Get(), m_winSize.cx,
      m_winSize.cy, pTextLayout.ReleaseAndGetAddressOf()));
  m_pD2D->dc->DrawTextLayout({(float)x, (float)y}, pTextLayout.Get(),
                             m_pD2D->m_pBrush.Get(),
                             D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}
// todo: seperate layout and drawing
void WeaselPanel::ResizeVertical() {
  SIZE textSize{0, 0};
  SIZE winSize{0, 0};
  if (!m_ctx.preedit.empty()) {
    const auto &preedit = (m_ctx.preedit.str);
    m_pD2D->GetTextSize(preedit, preedit.length(), m_pD2D->pTextFormat,
                        &textSize);
    winSize.cx += m_padding + textSize.cx;
    winSize.cy += m_padding + textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = (m_ctx.aux.str);
    m_pD2D->GetTextSize(aux, aux.length(), m_pD2D->pTextFormat, &textSize);
    winSize.cx += m_padding + textSize.cx;
    winSize.cy += m_padding + textSize.cy;
  }
  size_t cand_width = MAX(winSize.cx, 0L);
  if (m_ctx.cinfo.candies.size()) {
    const auto &cinfo = m_ctx.cinfo;
    for (auto i = 0; i < cinfo.candies.size(); i++) {
      const auto &label = (cinfo.labels[i].str);
      const auto &candie = (cinfo.candies[i].str);
      const auto &comment = (cinfo.comments[i].str);
      size_t x = m_padding, y = winSize.cy + m_padding;
      size_t h = 0;
      m_pD2D->GetTextSize(label, label.length(), m_pD2D->pLabelFormat,
                          &textSize);
      h = MAX((LONG)h, textSize.cy);
      x += textSize.cx + m_padding;
      m_pD2D->GetTextSize(candie, candie.length(), m_pD2D->pTextFormat,
                          &textSize);
      h = MAX((LONG)h, textSize.cy);
      if (!comment.empty()) {
        x += textSize.cx + m_padding;
        m_pD2D->GetTextSize(comment, comment.length(), m_pD2D->pCommentFormat,
                            &textSize);
        h = MAX((LONG)h, textSize.cy);
      }
      cand_width = MAX(cand_width, x + textSize.cx);
      winSize.cy += h;
    }
    winSize.cy += m_padding;
  }
  winSize.cx = MAX(winSize.cx, (LONG)cand_width);
  m_winSize.cx = cand_width + m_padding * 2;
  m_winSize.cy = winSize.cy + m_padding;

  SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_winSize.cx, m_winSize.cy,
               SWP_NOMOVE | SWP_NOACTIVATE);
  D2D1_SIZE_U oPixelSize = {(UINT32)m_winSize.cx, (UINT32)m_winSize.cy};
}
void WeaselPanel::ResizeHorizontal() {
  SIZE textSize{0, 0};
  SIZE winSize{0, 0};
  if (!m_ctx.preedit.empty()) {
    const auto &preedit = (m_ctx.preedit.str);
    m_pD2D->GetTextSize(preedit, preedit.length(), m_pD2D->pTextFormat,
                        &textSize);
    winSize.cx = textSize.cx;
    winSize.cy = textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = (m_ctx.aux.str);
    m_pD2D->GetTextSize(aux, aux.length(), m_pD2D->pTextFormat, &textSize);
    winSize.cx = textSize.cx;
    winSize.cy = textSize.cy;
  }
  if (m_ctx.cinfo.candies.size()) {
    m_text.clear();
    winSize.cy += m_padding;
    const auto &cinfo = m_ctx.cinfo;
    for (auto i = 0; i < cinfo.candies.size(); i++) {
      bool highlighted = i == cinfo.highlighted;
      m_text += highlighted ? L"[" : L"";
      m_text += (cinfo.labels[i].str) + L". " + (cinfo.candies[i].str) + L" " +
                (cinfo.comments[i].str);
      m_text += highlighted ? L"] " : L" ";
    }
    m_pD2D->GetTextSize(m_text, m_text.length(), m_pD2D->pTextFormat,
                        &textSize);
    winSize.cx = MAX(winSize.cx, textSize.cx);
    winSize.cy += textSize.cy;
  }
  m_winSize.cx = winSize.cx + m_padding * 2;
  m_winSize.cy = winSize.cy + m_padding * 2;

  SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_winSize.cx, m_winSize.cy,
               SWP_NOMOVE | SWP_NOACTIVATE);
  D2D1_SIZE_U oPixelSize = {(UINT32)m_winSize.cx, (UINT32)m_winSize.cy};
}
void WeaselPanel::DrawHorizontal() {
  SIZE textSize{0, 0};
  SIZE winSize{0, 0};
  if (!m_ctx.preedit.empty()) {
    const auto &preedit = (m_ctx.preedit.str);
    m_pD2D->GetTextSize(preedit, preedit.length(), m_pD2D->pTextFormat,
                        &textSize);
    DrawTextAt(preedit, m_padding, m_padding, m_pD2D->pTextFormat);
    winSize.cx = textSize.cx;
    winSize.cy = textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = (m_ctx.aux.str);
    m_pD2D->GetTextSize(aux, aux.length(), m_pD2D->pTextFormat, &textSize);
    DrawTextAt(aux, m_padding, m_padding, m_pD2D->pTextFormat);
    winSize.cx = textSize.cx;
    winSize.cy = textSize.cy;
  }
  if (m_ctx.cinfo.candies.size()) {
    winSize.cy += m_padding;
    DrawTextAt(m_text, m_padding, winSize.cy, m_pD2D->pTextFormat);
  }
}
void WeaselPanel::DrawUIVertical() {
  SIZE textSize{0, 0};
  SIZE winSize{0, 0};
  if (!m_ctx.preedit.empty()) {
    const auto &preedit = (m_ctx.preedit.str);
    m_pD2D->GetTextSize(preedit, preedit.length(), m_pD2D->pTextFormat,
                        &textSize);
    m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(m_style.text_color));
    DrawTextAt(preedit, m_padding, m_padding, m_pD2D->pTextFormat);
    winSize.cx += m_padding + textSize.cx;
    winSize.cy += m_padding + textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = (m_ctx.aux.str);
    m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(m_style.text_color));
    DrawTextAt(aux, m_padding, m_padding, m_pD2D->pTextFormat);
    m_pD2D->GetTextSize(aux, aux.length(), m_pD2D->pTextFormat, &textSize);
    winSize.cx += m_padding + textSize.cx;
    winSize.cy += m_padding + textSize.cy;
  }
  size_t cand_width = 0;
  if (m_ctx.cinfo.candies.size()) {
    const auto &cinfo = m_ctx.cinfo;
    for (auto i = 0; i < cinfo.candies.size(); i++) {
      const auto &label = (cinfo.labels[i].str);
      const auto &candie = (cinfo.candies[i].str);
      const auto &comment = (cinfo.comments[i].str);
      bool hilited = (i == cinfo.highlighted);
      LONG xl, yl, xt, yt, xc, yc;
      size_t x = m_padding, y = winSize.cy + m_padding;
      size_t h = 0;
      m_pD2D->GetTextSize(label, label.length(), m_pD2D->pLabelFormat,
                          &textSize);
      xl = x;
      yl = y;
      h = MAX((LONG)h, textSize.cy);
      x += textSize.cx + m_padding;

      m_pD2D->GetTextSize(candie, candie.length(), m_pD2D->pTextFormat,
                          &textSize);
      h = MAX((LONG)h, textSize.cy);
      xt = x;
      yt = y;

      if (!comment.empty()) {
        x += textSize.cx + m_padding;
        m_pD2D->GetTextSize(comment, comment.length(), m_pD2D->pCommentFormat,
                            &textSize);
        xc = x;
        yc = y;
        h = MAX((LONG)h, textSize.cy);
      } else {
        xc = xt + textSize.cx;
        yc = y;
      }
      cand_width = MAX(cand_width, x + textSize.cx);

      RECT rc = {xl, yl, xl + (LONG)cand_width, yl + (LONG)h};
      auto color = hilited ? m_style.hilited_candidate_back_color
                           : m_style.candidate_back_color;
      auto border_color = hilited ? m_style.hilited_candidate_border_color
                                  : m_style.candidate_border_color;
      FillRoundRect(rc, m_style.round_corner, m_style.border, color,
                    border_color);

      color =
          hilited ? m_style.hilited_label_text_color : m_style.label_text_color;
      m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(color));
      DrawTextAt(label, xl, yl, m_pD2D->pLabelFormat);
      color = hilited ? m_style.hilited_candidate_text_color
                      : m_style.candidate_text_color;
      m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(color));
      DrawTextAt(candie, xt, yt, m_pD2D->pTextFormat);
      if (!comment.empty()) {
        color = hilited ? m_style.hilited_comment_text_color
                        : m_style.comment_text_color;
        m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(color));
        DrawTextAt(comment, xc, yc, m_pD2D->pCommentFormat);
      }
      winSize.cy += h;
    }
    winSize.cy += m_padding;
    m_winSize.cx = cand_width;
    m_winSize.cy = winSize.cy;
  }
}

void WeaselPanel::OnPaint() {
  if (m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL) {
    ResizeHorizontal();
  } else {
    ResizeVertical();
  }
  Render();
}

LRESULT CALLBACK WeaselPanel::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) {
  WeaselPanel *self;
  if (uMsg == WM_NCCREATE) {
    self = static_cast<WeaselPanel *>(
        reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self =
        reinterpret_cast<WeaselPanel *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }
  switch (uMsg) {
  case WM_PAINT:
    if (self) {
      self->OnPaint();
      ValidateRect(hwnd, nullptr);
    }
    break;
  case WM_DESTROY:
    return 0;
  case WM_LBUTTONUP: {
    return 0;
  }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
