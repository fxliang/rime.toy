#include "WeaselPanel.h"
#include "HorizontalLayout.h"
#include "VHorizontalLayout.h"
#include "VerticalLayout.h"
#include "base.h"
#include <DWrite.h>
#include <dwrite_2.h>
#include <map>
#include <regex>
#include <string>
#include <utils.h>
#include <vector>
#include <wrl/client.h>

using namespace weasel;

#define DPI_SCALE(t) (int)(t * m_pD2D->m_dpiScaleLayout)
#define COLORTRANSPARENT(color) ((color & 0xff000000) == 0)
#define COLORNOTTRANSPARENT(color) ((color & 0xff000000) != 0)
#define TRANS_COLOR 0x00000000
#define HALF_ALPHA_COLOR(color)                                                \
  ((((color & 0xff000000) >> 25) & 0xff) << 24) | (color & 0x00ffffff)

const GUID CLSID_D2D1GaussianBlur = {
    0x1feb6d69,
    0x2fe6,
    0x4ac9,
    {0x8c, 0x58, 0x1d, 0x7f, 0x93, 0xe7, 0xa6, 0xa5}};

WeaselPanel::WeaselPanel(UI &ui)
    : m_hWnd(nullptr), m_ctx(ui.ctx()), m_octx(ui.octx()),
      m_status(ui.status()), m_style(ui.style()), m_ostyle(ui.ostyle()) {
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

void WeaselPanel::_ResizeWindow() {
  CSize size = m_layout->GetContentSize();
  SetWindowPos(m_hWnd, 0, 0, 0, size.cx, size.cy,
               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW);
  m_pD2D->OnResize(size.cx, size.cy);
}

void WeaselPanel::_CreateLayout() {
  if (m_layout)
    delete m_layout;

  Layout *layout;
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
    layout = new VHorizontalLayout(m_style, m_ctx, m_status, m_pD2D);
  } else {
    if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL ||
        m_style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN) {
      layout = new VerticalLayout(m_style, m_ctx, m_status, m_pD2D);
    } else if (m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL ||
               m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN) {
      layout = new HorizontalLayout(m_style, m_ctx, m_status, m_pD2D);
    }
  }
  m_layout = layout;
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
      CW_USEDEFAULT, CW_USEDEFAULT, 10, 10, parent, nullptr,
      GetModuleHandle(nullptr), this);
  if (m_hWnd) {
    m_pD2D.reset(new D2D(m_style, m_hWnd));
    m_ostyle = m_style;
  }
  UpdateWindow(m_hWnd);
  return !!m_hWnd;
}

D2D1_ROUNDED_RECT RoundedRectFromRect(const RECT &rect, uint32_t radius) {
  return D2D1::RoundedRect(
      D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                  static_cast<float>(rect.right),
                  static_cast<float>(rect.bottom)),
      radius, // radiusX
      radius  // radiusY
  );
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
  if (m_status.composing) {
    auto rc = m_layout->GetContentRect();
    HighlightRect(rc, DPI_SCALE(m_style.round_corner_ex),
                  DPI_SCALE(m_style.border), m_style.back_color,
                  m_style.shadow_color, m_style.border_color);
    if (!m_ctx.preedit.empty()) {
      auto prc = m_layout->GetPreeditRect();
      _DrawPreedit(m_ctx.preedit, prc);
    }
    if (!m_ctx.aux.empty()) {
      auto arc = m_layout->GetAuxiliaryRect();
      _DrawPreedit(m_ctx.aux, arc);
    }
    if (m_ctx.cinfo.candies.size()) {
      _DrawCandidates(true);
    }
  }
  HR(m_pD2D->dc->EndDraw());
  // Make the swap chain available to the composition engine
  HR(m_pD2D->swapChain->Present(1, 0)); // sync
}

bool WeaselPanel::_DrawPreedit(const Text &text, CRect &rc) {
  bool drawn = false;
  wstring const &t = text.str;
  ComPtr<IDWriteTextFormat1> &pTextFormat = m_pD2D->pPreeditFormat;

  if (!t.empty()) {
    TextRange range = m_layout->GetPreeditRange();
    if (range.start < range.end) {
      auto before_str = t.substr(0, range.start);
      auto hilited_str = t.substr(range.start, range.end - range.start);
      auto after_str = t.substr(range.end);
      auto beforeSz = m_layout->GetBeforeSize();
      auto hilitedSz = m_layout->GetHiliteSize();
      auto afterSz = m_layout->GetAfterSize();
      int x = rc.left, y = rc.top;
      if (range.start > 0) {
        CRect rc_before;
        if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
          rc_before = CRect(rc.left, y, rc.right, y + beforeSz.cy);
        else
          rc_before = CRect(x, rc.top, rc.left + beforeSz.cx, rc.bottom);
        _TextOut(rc_before, before_str, before_str.length(), m_style.text_color,
                 pTextFormat);
        if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
          y += beforeSz.cy + DPI_SCALE(m_style.hilite_spacing);
        else
          x += beforeSz.cx + DPI_SCALE(m_style.hilite_spacing);
      }
      {
        // zzz[yyy]
        CRect rc_hi;
        if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
          rc_hi = CRect(rc.left, y, rc.right, y + hilitedSz.cy);
        else
          rc_hi = CRect(x, rc.top, x + hilitedSz.cx, rc.bottom);
        CRect rc_hib = rc_hi;
        rc_hib.Inflate(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
        HighlightRect(rc_hib, DPI_SCALE(m_style.round_corner),
                      DPI_SCALE(m_style.border), m_style.hilited_back_color,
                      m_style.hilited_shadow_color, 0);
        _TextOut(rc_hi, hilited_str, hilited_str.length(),
                 m_style.hilited_text_color, pTextFormat);
        if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
          y += rc_hi.Height() + DPI_SCALE(m_style.hilite_spacing);
        else
          x += rc_hi.Width() + DPI_SCALE(m_style.hilite_spacing);
      }
      if (range.end < static_cast<int>(t.length())) {
        // zzz[yyy]xxx
        CRect rc_after;
        if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
          rc_after = CRect(rc.left, y, rc.right, y + afterSz.cy);
        else
          rc_after = CRect(x, rc.top, x + afterSz.cx, rc.bottom);
        _TextOut(rc_after, after_str, after_str.length(), m_style.text_color,
                 pTextFormat);
      }
    } else {
      CRect rcText(rc.left, rc.top, rc.right, rc.bottom);
      _TextOut(rcText, t.c_str(), t.length(), m_style.text_color, pTextFormat);
    }
    if (m_candidateCount && !m_style.inline_preedit &&
        COLORNOTTRANSPARENT(m_style.prevpage_color) &&
        COLORNOTTRANSPARENT(m_style.nextpage_color)) {
      const std::wstring pre = L"<";
      const std::wstring next = L">";
      CRect prc = m_layout->GetPrepageRect();
      // clickable color / disabled color
      int color =
          m_ctx.cinfo.currentPage ? m_style.prevpage_color : m_style.text_color;
      _TextOut(prc, pre.c_str(), pre.length(), color, pTextFormat);

      CRect nrc = m_layout->GetNextpageRect();
      // clickable color / disabled color
      color = m_ctx.cinfo.is_last_page ? m_style.text_color
                                       : m_style.nextpage_color;
      _TextOut(nrc, next.c_str(), next.length(), color, pTextFormat);
    }
    drawn = true;
  }
  return drawn;
}

bool WeaselPanel::_DrawCandidates(bool back = false) {
  bool drawn = false;
  const vector<Text> &candidates(m_ctx.cinfo.candies);
  const vector<Text> &comments(m_ctx.cinfo.comments);
  const vector<Text> &labels(m_ctx.cinfo.labels);
  ComPtr<IDWriteTextFormat1> &txtFormat = m_pD2D->pTextFormat;
  ComPtr<IDWriteTextFormat1> &labeltxtFormat = m_pD2D->pTextFormat;
  ComPtr<IDWriteTextFormat1> &commenttxtFormat = m_pD2D->pTextFormat;

  if (back) {
    if (COLORNOTTRANSPARENT(m_style.candidate_shadow_color)) {
      for (auto i = 0; i < m_candidateCount; i++) {
        if (i == m_ctx.cinfo.highlighted || i == m_hoverIndex)
          continue;
        auto rect = m_layout->GetCandidateRect(i);
        rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                         DPI_SCALE(m_style.hilite_padding_y));
        HighlightRect(rect, m_style.round_corner, 0, 0,
                      m_style.candidate_shadow_color, 0);
        drawn = true;
      }
    }
    // if (COLORNOTTRANSPARENT(m_style.candidate_back_color) ||
    // COLORNOTTRANSPARENT(m_style.candidate_border_color)) {
    int label_text_color = m_style.label_text_color;
    int candidate_text_color = m_style.candidate_text_color;
    int comment_text_color = m_style.comment_text_color;
    for (auto i = 0; i < candidates.size(); i++) {
      if (i == m_ctx.cinfo.highlighted || i == m_hoverIndex)
        continue;

      auto rect = m_layout->GetCandidateRect(i);
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
      HighlightRect(rect, DPI_SCALE(m_style.round_corner), 0,
                    m_style.candidate_back_color, 0,
                    m_style.candidate_border_color);

      auto &label = labels.at(i).str;
      if (!label.empty()) {
        rect = m_layout->GetCandidateLabelRect(i);
        _TextOut(rect, label, label.length(), label_text_color, labeltxtFormat);
      }
      auto &text = candidates.at(i).str;
      if (!text.empty()) {
        rect = m_layout->GetCandidateTextRect(i);
        _TextOut(rect, text, text.length(), candidate_text_color, txtFormat);
      }
      auto &comment = comments.at(i).str;
      if (!comment.empty() && COLORNOTTRANSPARENT(m_style.comment_text_color)) {
        rect = m_layout->GetCandidateCommentRect(i);
        _TextOut(rect, comment, comment.length(), comment_text_color,
                 commenttxtFormat);
      }

      drawn = true;
    }
    //}

    // draw highlighted background and shadow
    {
      int label_text_color = m_style.hilited_label_text_color;
      int candidate_text_color = m_style.hilited_candidate_text_color;
      int comment_text_color = m_style.hilited_comment_text_color;
      auto rect = m_layout->GetHighlightRect();
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
      HighlightRect(rect, DPI_SCALE(m_style.round_corner),
                    DPI_SCALE(m_style.border),
                    m_style.hilited_candidate_back_color,
                    m_style.hilited_candidate_shadow_color,
                    m_style.hilited_candidate_border_color);
      auto i = m_ctx.cinfo.highlighted;
      auto &label = labels.at(i).str;
      if (!label.empty()) {
        rect = m_layout->GetCandidateLabelRect(i);
        _TextOut(rect, label, label.length(), label_text_color, labeltxtFormat);
      }
      auto &text = candidates.at(i).str;
      if (!text.empty()) {
        rect = m_layout->GetCandidateTextRect(i);
        _TextOut(rect, text, text.length(), candidate_text_color, txtFormat);
      }
      auto &comment = comments.at(i).str;
      if (!comment.empty() && COLORNOTTRANSPARENT(m_style.comment_text_color)) {
        rect = m_layout->GetCandidateCommentRect(i);
        _TextOut(rect, comment, comment.length(), comment_text_color,
                 commenttxtFormat);
      }
      if (m_style.mark_text.empty() &&
          COLORNOTTRANSPARENT(m_style.hilited_mark_color)) {
        int height =
            MIN(rect.Height() - DPI_SCALE(m_style.hilite_padding_y) * 2,
                rect.Height() - DPI_SCALE(m_style.round_corner) * 2);
        int width = MIN(rect.Width() - DPI_SCALE(m_style.hilite_padding_x) * 2,
                        rect.Width() - DPI_SCALE(m_style.round_corner) * 2);
        width = MIN(width, static_cast<int>(rect.Width() * 0.618));
        height = MIN(height, static_cast<int>(rect.Height() * 0.618));
      }
      drawn = true;
    }
  }
  return drawn;
}

void WeaselPanel::_TextOut(CRect &rc, const wstring &text, size_t cch,
                           uint32_t color,
                           ComPtr<IDWriteTextFormat1> &pTextFormat) {
  if (!pTextFormat.Get())
    return;
  if (m_pD2D->m_pBrush == nullptr) {
  } else {
    m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(color));
  }

  ComPtr<IDWriteTextLayout> pTextLayout;
  m_pD2D->m_pWriteFactory->CreateTextLayout(
      text.c_str(), cch, pTextFormat.Get(), rc.Width(), rc.Height(),
      reinterpret_cast<IDWriteTextLayout **>(pTextLayout.GetAddressOf()));
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
  }
  float offsetx = (float)rc.left;
  float offsety = (float)rc.top;

  DWRITE_OVERHANG_METRICS omt;
  pTextLayout->GetOverhangMetrics(&omt);
  if (m_style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT && omt.left > 0)
    offsetx += omt.left;
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT && omt.top > 0)
    offsety += omt.top;

  m_pD2D->dc->DrawTextLayout({offsetx, offsety}, pTextLayout.Get(),
                             m_pD2D->m_pBrush.Get(),
                             D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

void WeaselPanel::HighlightRect(const RECT &rect, float radius, uint32_t border,
                                uint32_t back_color, uint32_t shadow_color,
                                uint32_t border_color) {
  // draw shadow
  if ((shadow_color & 0xff000000) && m_style.shadow_radius) {
    CRect rc = rect;
    rc.OffsetRect(DPI_SCALE(m_style.shadow_offset_x),
                  DPI_SCALE(m_style.shadow_offset_y));
    // Define a rounded rectangle
    ComPtr<ID2D1RoundedRectangleGeometry> roundedRectGeometry;
    D2D1_ROUNDED_RECT roundedRect = RoundedRectFromRect(rc, radius);
    HR(m_pD2D->d2Factory->CreateRoundedRectangleGeometry(roundedRect,
                                                         &roundedRectGeometry));
    // Create a compatible render target
    ComPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
    HR(m_pD2D->dc->CreateCompatibleRenderTarget(&bitmapRenderTarget));

    // Draw the rounded rectangle onto the bitmap render target
    m_pD2D->brush->SetColor(D2d1ColorFromColorRef(shadow_color));
    bitmapRenderTarget->BeginDraw();
    bitmapRenderTarget->FillGeometry(roundedRectGeometry.Get(),
                                     m_pD2D->brush.Get());
    bitmapRenderTarget->EndDraw();
    // Get the bitmap from the bitmap render target
    ComPtr<ID2D1Bitmap> bitmap;
    HR(bitmapRenderTarget->GetBitmap(&bitmap));
    //// Create a Gaussian blur effect
    ComPtr<ID2D1Effect> blurEffect;
    HR(m_pD2D->dc->CreateEffect(CLSID_D2D1GaussianBlur, &blurEffect));
    blurEffect->SetInput(0, bitmap.Get());
    blurEffect->SetValue(
        D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION,
        (float)m_style.shadow_radius); // Adjust blur radius as needed
    // Draw the blurred rounded rectangle onto the main render target
    m_pD2D->dc->DrawImage(blurEffect.Get());
  }
  // draw back color
  if (back_color & 0xff000000) {
    // Define a rounded rectangle
    ComPtr<ID2D1RoundedRectangleGeometry> roundedRectGeometry;
    D2D1_ROUNDED_RECT roundedRect = RoundedRectFromRect(rect, radius);
    HR(m_pD2D->d2Factory->CreateRoundedRectangleGeometry(roundedRect,
                                                         &roundedRectGeometry));
    m_pD2D->brush->SetColor(D2d1ColorFromColorRef(back_color));
    m_pD2D->dc->FillGeometry(roundedRectGeometry.Get(), m_pD2D->brush.Get());
  }
  // draw border
  if ((border_color & 0xff000000) && border) {
    float hb = -(float)border / 2;
    CRect rc = rect;
    rc.InflateRect(hb, hb);
    D2D1_ROUNDED_RECT borderRect = RoundedRectFromRect(rc, radius + hb);
    m_pD2D->brush->SetColor(D2d1ColorFromColorRef(border_color));
    m_pD2D->dc->DrawRoundedRectangle(&borderRect, m_pD2D->brush.Get(), border);
  }
}

void WeaselPanel::OnPaint() {
  // ignore if d2d resources not ready
  if (!m_pD2D)
    return;
  _CreateLayout();
  m_layout->DoLayout();
  _ResizeWindow();
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
