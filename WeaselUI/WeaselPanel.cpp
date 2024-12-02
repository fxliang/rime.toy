#include "WeaselPanel.h"
#include "FullScreenLayout.h"
#include "HorizontalLayout.h"
#include "VHorizontalLayout.h"
#include "VerticalLayout.h"
#include <filesystem>
#include <resource.h>
#include <wrl/client.h>

namespace fs = std::filesystem;

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

void LoadIconIfNeed(wstring &oicofile, const wstring &icofile, HICON &hIcon,
                    UINT id) {
  if (oicofile == icofile)
    return;
  oicofile = icofile;
  const int STATUS_ICON_SIZE = GetSystemMetrics(SM_CXICON);
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (!icofile.empty() && fs::exists(fs::path(icofile)))
    hIcon =
        (HICON)LoadImage(hInstance, icofile.c_str(), IMAGE_ICON,
                         STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_LOADFROMFILE);
  else
    hIcon =
        (HICON)LoadImage(hInstance, MAKEINTRESOURCE(id), IMAGE_ICON,
                         STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
}

WeaselPanel::WeaselPanel(UI &ui)
    : m_hWnd(nullptr), m_ctx(ui.ctx()), m_octx(ui.octx()), m_layout(nullptr),
      m_pD2D(nullptr), m_status(ui.status()), m_style(ui.style()),
      m_uiCallback(ui.uiCallback()), m_ostyle(ui.ostyle()), m_candidateCount(0),
      hide_candidates(true) {
  auto hInstance = GetModuleHandle(nullptr);
  m_iconAlpha = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EN));
  m_iconEnabled = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ZH));
  m_iconFull = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FULL_SHAPE));
  m_iconHalf = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HALF_SHAPE));
  m_iconDisabled = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RELOAD));
}

BOOL WeaselPanel::IsWindow() const { return ::IsWindow(m_hWnd); }

void WeaselPanel::ShowWindow(int nCmdShow) { ::ShowWindow(m_hWnd, nCmdShow); }

void WeaselPanel::DestroyWindow() {
  ::DestroyWindow(m_hWnd);
  m_hWnd = nullptr;
}

void WeaselPanel::MoveTo(RECT rc) {
  if (m_hWnd) {
    m_inputPos = rc;
    RECT rcWorkArea;
    memset(&rcWorkArea, 0, sizeof(rcWorkArea));
#ifdef TOY_FEATURE
    HWND topWin = GetForegroundWindow();
    CRect clientRect = rc;
    if (topWin)
      GetWindowRect(topWin, &clientRect);
    HMONITOR hMonitor = MonitorFromRect(&clientRect, MONITOR_DEFAULTTONEAREST);
#else
    HMONITOR hMonitor = MonitorFromRect(&m_inputPos, MONITOR_DEFAULTTONEAREST);
#endif
    if (hMonitor) {
      MONITORINFO info;
      info.cbSize = sizeof(MONITORINFO);
      if (GetMonitorInfo(hMonitor, &info)) {
        rcWorkArea = info.rcWork;
      }
    }
    CRect rcWindow;
    GetWindowRect(m_hWnd, &rcWindow);
    rcWorkArea.right -= rcWindow.Width();
    rcWorkArea.bottom -= rcWindow.Height();
    int x = m_inputPos.left;
    int y = m_inputPos.bottom;
#ifdef TOY_FEATURE
    if (!IS_FULLSCREENLAYOUT(m_style)) {
      POINT pt{x, y};
      if (!clientRect.PtInRect(pt)) {
        y = clientRect.bottom - rcWindow.Height();
        x = clientRect.left + clientRect.Width() / 2;
        auto xp = (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT &&
                   !m_style.vertical_text_left_to_right)
                      ? (rcWindow.Width() / 2)
                      : (-rcWindow.Width() / 2);
        x += xp;
      }
    }
#endif
    if (x > rcWorkArea.right)
      x = rcWorkArea.right;
    if (x < rcWorkArea.left)
      x = rcWorkArea.right;
    bool m_istorepos_buffer = m_istorepos;
    m_istorepos = false;
    if (y < rcWorkArea.bottom)
      m_sticky = false;
    if (y > rcWorkArea.bottom || m_sticky) {
      m_sticky = true;
      m_istorepos = (m_style.vertical_auto_reverse &&
                     m_style.layout_type == UIStyle::LAYOUT_VERTICAL);
      y = rcWorkArea.bottom;
    }
    if (y < rcWorkArea.top)
      y = rcWorkArea.top;
    if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT &&
        !m_style.vertical_text_left_to_right)
      x -= rcWindow.Width();
    m_inputPos.bottom = y;

    SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    if (m_istorepos_buffer != m_istorepos)
      InvalidateRect(m_hWnd, NULL, TRUE); // 请求重绘
  }
}

void WeaselPanel::_ResizeWindow() {
  CSize size = m_layout->GetContentSize();
  SetWindowPos(m_hWnd, 0, 0, 0, size.cx, size.cy,
               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW);
  m_pD2D->OnResize(size.cx, size.cy);
}

void WeaselPanel::_CreateLayout() {
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
  if (IS_FULLSCREENLAYOUT(m_style)) {
    layout = new FullScreenLayout(m_style, m_ctx, m_status, m_inputPos, layout,
                                  m_pD2D);
  }
  m_layout.reset(layout);
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
    bool should_show_icon =
        (m_status.ascii_mode || !m_status.composing || !m_ctx.aux.empty());
    m_candidateCount = (BYTE)m_ctx.cinfo.candies.size();
    // check if to hide candidates window
    // show tips status, two kind of situation: 1) only aux strings, don't care
    // icon status; 2)only icon(ascii mode switching)
    bool show_tips =
        (!m_ctx.aux.empty() && m_ctx.cinfo.empty() && m_ctx.preedit.empty()) ||
        (m_ctx.empty() && should_show_icon);
    // show schema menu status: schema_id == L".default"
    bool show_schema_menu = m_status.schema_id == L".default";
    bool margin_negative =
        (DPI_SCALE(m_style.margin_x) < 0 || DPI_SCALE(m_style.margin_y) < 0);
    // when to hide_cadidates?
    // 1. margin_negative, and not in show tips mode( ascii switching /
    // half-full switching / simp-trad switching / error tips), and not in
    // schema menu
    // 2. inline preedit without candidates
    bool inline_no_candidates =
        (m_style.inline_preedit && m_candidateCount == 0) && !show_tips;
    hide_candidates = inline_no_candidates ||
                      (margin_negative && !show_tips && !show_schema_menu);
    auto hr = m_pD2D->direct3dDevice->GetDeviceRemovedReason();
    FAILEDACTION(hr, DEBUG << StrzHr(hr), m_pD2D->InitDirect2D());
    _CreateLayout();
    m_layout->DoLayout();
    _ResizeWindow();
    InvalidateRect(m_hWnd, NULL, TRUE); // 请求重绘
  }
}

BOOL WeaselPanel::Create(HWND parent) {
  if (m_hWnd)
    return !!m_hWnd;
  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"WeaselPanel";
  RegisterClass(&wc);
  m_hWnd = CreateWindowEx(
      WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST,
      L"WeaselPanel", L"WeaselPanel", WS_POPUP | WS_VISIBLE, CW_USEDEFAULT,
      CW_USEDEFAULT, 10, 10, parent, nullptr, GetModuleHandle(nullptr), this);
  if (m_hWnd) {
    if (!m_pD2D)
      m_pD2D.reset(new D2D(m_style, m_hWnd));
    else {
      m_pD2D->m_hWnd = m_hWnd;
      m_pD2D->InitDpiInfo();
      m_pD2D->InitDirect2D();
      m_pD2D->InitDirectWriteResources();
    }
    m_ostyle = m_style;
  }
  return !!m_hWnd;
}

void WeaselPanel::DoPaint() {
  if (!m_pD2D)
    return;
  auto hr = m_pD2D->direct3dDevice->GetDeviceRemovedReason();
  FAILEDACTION(hr, DEBUG << StrzHr(hr), m_pD2D->InitDirect2D());
  m_pD2D->dc->BeginDraw();
  m_pD2D->dc->Clear(D2D1::ColorF({0.0f, 0.0f, 0.0f, 0.0f}));
  if (!hide_candidates) {
    auto rc = m_layout->GetContentRect();
    IsToRoundStruct roundInfo;
    _HighlightRect(rc, DPI_SCALE(m_style.round_corner_ex),
                   DPI_SCALE(m_style.border), m_style.back_color,
                   m_style.shadow_color, m_style.border_color, roundInfo);
    auto prc = m_layout->GetPreeditRect();
    auto arc = m_layout->GetAuxiliaryRect();
    // if vertical auto reverse triggered
    if (m_istorepos) {
      CRect *rects = new CRect[m_candidateCount];
      int *btmys = new int[m_candidateCount];
      for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
        rects[i] = m_layout->GetCandidateRect(i);
        btmys[i] = rects[i].bottom;
      }
      if (m_candidateCount) {
        if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty())
          m_offsety_preedit = rects[m_candidateCount - 1].bottom - prc.bottom;
        if (!m_ctx.aux.str.empty())
          m_offsety_aux = rects[m_candidateCount - 1].bottom - arc.bottom;
      } else {
        m_offsety_preedit = 0;
        m_offsety_aux = 0;
      }
      int base_gap = 0;
      if (!m_ctx.aux.str.empty())
        base_gap = arc.Height() + m_style.spacing;
      else if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty())
        base_gap = prc.Height() + m_style.spacing;

      for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
        if (i == 0)
          m_offsetys[i] =
              btmys[m_candidateCount - i - 1] - base_gap - rects[i].bottom;
        else
          m_offsetys[i] = (rects[i - 1].top + m_offsetys[i - 1] -
                           DPI_SCALE(m_style.candidate_spacing)) -
                          rects[i].bottom;
      }
      delete[] rects;
      delete[] btmys;
    }
    if (!m_ctx.preedit.empty()) {
      if (m_istorepos)
        prc.OffsetRect(0, m_offsety_preedit);
      _DrawPreedit(m_ctx.preedit, prc);
    }
    if (!m_ctx.aux.empty()) {
      if (m_istorepos)
        arc.OffsetRect(0, m_offsety_aux);
      _DrawPreedit(m_ctx.aux, arc);
    }
    if (m_candidateCount) {
      _DrawCandidates(true);
    }
    if (m_layout->ShouldDisplayStatusIcon()) {
      ComPtr<ID2D1Bitmap1> pBitmap;
      LoadIconIfNeed(m_current_ascii_icon, m_style.current_ascii_icon,
                     m_iconAlpha, IDI_EN);
      LoadIconIfNeed(m_current_zhung_icon, m_style.current_zhung_icon,
                     m_iconEnabled, IDI_ZH);
      LoadIconIfNeed(m_current_full_icon, m_style.current_full_icon, m_iconFull,
                     IDI_FULL_SHAPE);
      LoadIconIfNeed(m_current_half_icon, m_style.current_half_icon, m_iconHalf,
                     IDI_HALF_SHAPE);

      HICON &ico =
          m_status.disabled ? m_iconDisabled
          : m_status.ascii_mode
              ? m_iconAlpha
              : (m_status.type == SCHEMA
                     ? m_iconEnabled
                     : (m_status.full_shape ? m_iconFull : m_iconHalf));
      m_pD2D->GetBmpFromIcon(ico, pBitmap);
      // Draw the bitmap
      auto iconRect = m_layout->GetStatusIconRect();
      D2D1_RECT_F iconRectf = D2D1::RectF(iconRect.left, iconRect.top,
                                          iconRect.right, iconRect.bottom);
      m_pD2D->dc->DrawBitmap(pBitmap.Get(), iconRectf);
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
        IsToRoundStruct roundInfo = m_layout->GetTextRoundInfo();
        _HighlightRect(rc_hib, DPI_SCALE(m_style.round_corner),
                       DPI_SCALE(m_style.border), m_style.hilited_back_color,
                       m_style.hilited_shadow_color, 0, roundInfo);
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
  ComPtr<IDWriteTextFormat1> &labeltxtFormat = m_pD2D->pLabelFormat;
  ComPtr<IDWriteTextFormat1> &commenttxtFormat = m_pD2D->pCommentFormat;

  if (back) {
    if (COLORNOTTRANSPARENT(m_style.candidate_shadow_color)) {
      for (auto i = 0; i < m_candidateCount; i++) {
        if (i == m_ctx.cinfo.highlighted || i == m_hoverIndex)
          continue;
        auto rect = m_layout->GetCandidateRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
        rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                         DPI_SCALE(m_style.hilite_padding_y));
        IsToRoundStruct roundInfo = m_layout->GetRoundInfo(i);
        _HighlightRect(rect, m_style.round_corner, 0, 0,
                       m_style.candidate_shadow_color, 0, roundInfo);
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
      if (m_istorepos)
        rect.OffsetRect(0, m_offsetys[i]);
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));

      IsToRoundStruct roundInfo = m_layout->GetRoundInfo(i);
      _HighlightRect(rect, DPI_SCALE(m_style.round_corner), 0,
                     m_style.candidate_back_color, 0,
                     m_style.candidate_border_color, roundInfo);

      auto &label = labels.at(i).str;
      if (!label.empty()) {
        rect = m_layout->GetCandidateLabelRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
        _TextOut(rect, label, label.length(), label_text_color, labeltxtFormat);
      }
      auto &text = candidates.at(i).str;
      if (!text.empty()) {
        rect = m_layout->GetCandidateTextRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
        _TextOut(rect, text, text.length(), candidate_text_color, txtFormat);
      }
      auto &comment = comments.at(i).str;
      if (!comment.empty() && COLORNOTTRANSPARENT(m_style.comment_text_color)) {
        rect = m_layout->GetCandidateCommentRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
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
      if (m_istorepos)
        rect.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
      IsToRoundStruct roundInfo =
          m_layout->GetRoundInfo(m_ctx.cinfo.highlighted);
      _HighlightRect(rect, DPI_SCALE(m_style.round_corner),
                     DPI_SCALE(m_style.border),
                     m_style.hilited_candidate_back_color,
                     m_style.hilited_candidate_shadow_color,
                     m_style.hilited_candidate_border_color, roundInfo);
      // draw highlight mark
      if (COLORNOTTRANSPARENT(m_style.hilited_mark_color)) {
        if (!m_style.mark_text.empty()) {
          CRect rc = m_layout->GetHighlightRect();
          if (m_istorepos)
            rc.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
          rc.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                         DPI_SCALE(m_style.hilite_padding_y));
          int vgap = m_layout->mark_height
                         ? (rc.Height() - m_layout->mark_height) / 2
                         : 0;
          int hgap = m_layout->mark_width
                         ? (rc.Width() - m_layout->mark_width) / 2
                         : 0;
          CRect hlRc;
          if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
            hlRc = CRect(rc.left + hgap,
                         rc.top + DPI_SCALE(m_style.hilite_padding_y),
                         rc.left + hgap + m_layout->mark_width,
                         rc.top + DPI_SCALE(m_style.hilite_padding_y) +
                             m_layout->mark_height);
          else
            hlRc = CRect(rc.left + DPI_SCALE(m_style.hilite_padding_x),
                         rc.top + vgap,
                         rc.left + DPI_SCALE(m_style.hilite_padding_x) +
                             m_layout->mark_width,
                         rc.bottom - vgap);
          _TextOut(hlRc, m_style.mark_text.c_str(), m_style.mark_text.length(),
                   m_style.hilited_mark_color, txtFormat);
        } else {
          int height =
              MIN(rect.Height() - DPI_SCALE(m_style.hilite_padding_y) * 2,
                  rect.Height() - DPI_SCALE(m_style.round_corner) * 2);
          int width =
              MIN(rect.Width() - DPI_SCALE(m_style.hilite_padding_x) * 2,
                  rect.Width() - DPI_SCALE(m_style.round_corner) * 2);
          width = MIN(width, static_cast<int>(rect.Width() * 0.618));
          height = MIN(height, static_cast<int>(rect.Height() * 0.618));
          // if (bar_scale_ != 1.0f) {
          //   width = static_cast<int>(width * bar_scale_);
          //   height = static_cast<int>(height * bar_scale_);
          // }
          CRect mkrc;
          int mark_radius;
          if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
            int x = rect.left + (rect.Width() - width) / 2;
            mkrc =
                CRect(x, rect.top, x + width, rect.top + m_layout->mark_height);
            mark_radius = mkrc.Height() / 2;
          } else {
            int y = rect.top + (rect.Height() - height) / 2;
            mkrc = CRect(rect.left, y, rect.left + m_layout->mark_width,
                         y + height);
            mark_radius = mkrc.Width() / 2;
          }
          IsToRoundStruct roundInfo;
          _HighlightRect(mkrc, mark_radius, 0, m_style.hilited_mark_color, 0, 0,
                         roundInfo);
        }
      }
      auto i = m_ctx.cinfo.highlighted;
      auto &label = labels.at(i).str;
      if (!label.empty()) {
        rect = m_layout->GetCandidateLabelRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
        _TextOut(rect, label, label.length(), label_text_color, labeltxtFormat);
      }
      auto &text = candidates.at(i).str;
      if (!text.empty()) {
        rect = m_layout->GetCandidateTextRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
        _TextOut(rect, text, text.length(), candidate_text_color, txtFormat);
      }
      auto &comment = comments.at(i).str;
      if (!comment.empty() && COLORNOTTRANSPARENT(m_style.comment_text_color)) {
        rect = m_layout->GetCandidateCommentRect(i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
        _TextOut(rect, comment, comment.length(), comment_text_color,
                 commenttxtFormat);
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
    m_pD2D->SetBrushColor(color);
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

void WeaselPanel::_HighlightRect(const RECT &rect, float radius,
                                 uint32_t border, uint32_t back_color,
                                 uint32_t shadow_color, uint32_t border_color,
                                 IsToRoundStruct roundInfo) {
  if (roundInfo.Hemispherical)
    radius = m_style.round_corner_ex;
  // draw shadow
  if ((shadow_color & 0xff000000) && m_style.shadow_radius) {
    CRect rc = rect;
    rc.OffsetRect(DPI_SCALE(m_style.shadow_offset_x),
                  DPI_SCALE(m_style.shadow_offset_y));
    // Define a rounded rectangle
    ComPtr<ID2D1PathGeometry> pGeometry;
    HR(m_pD2D->CreateRoundedRectanglePath(rc, radius, roundInfo, pGeometry));
    // Create a compatible render target
    ComPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
    HR(m_pD2D->dc->CreateCompatibleRenderTarget(&bitmapRenderTarget));

    // Draw the rounded rectangle onto the bitmap render target
    m_pD2D->SetBrushColor(shadow_color);
    bitmapRenderTarget->BeginDraw();
    bitmapRenderTarget->FillGeometry(pGeometry.Get(), m_pD2D->m_pBrush.Get());
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
    ComPtr<ID2D1PathGeometry> pGeometry;
    HR(m_pD2D->CreateRoundedRectanglePath(rect, radius, roundInfo, pGeometry));
    m_pD2D->SetBrushColor(back_color);
    m_pD2D->dc->FillGeometry(pGeometry.Get(), m_pD2D->m_pBrush.Get());
  }
  // draw border
  if ((border_color & 0xff000000) && border) {
    float hb = -(float)border / 2;
    CRect rc = rect;
    rc.InflateRect(hb, hb);
    ComPtr<ID2D1PathGeometry> pGeometry;
    HR(m_pD2D->CreateRoundedRectanglePath(rc, radius + hb, roundInfo,
                                          pGeometry));
    m_pD2D->SetBrushColor(border_color);
    m_pD2D->dc->DrawGeometry(pGeometry.Get(), m_pD2D->m_pBrush.Get(), border);
  }
}

void WeaselPanel::OnDestroy() {
  m_layout.reset();
  m_sticky = false;
}

HRESULT WeaselPanel::OnScroll(WPARAM wParam) {
  int delta = GET_WHEEL_DELTA_WPARAM(wParam);
  if (m_uiCallback && delta != 0) {
    bool nextpage = delta < 0;
    m_uiCallback(NULL, NULL, NULL, &nextpage);
  }
  return 0;
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
      self->DoPaint();
    }
    break;
  case WM_DESTROY:
    if (self) {
      self->OnDestroy();
    }
    break;
  // not ready yet
  // case WM_MOUSEWHEEL:
  //  if (self)
  //    return self->OnScroll(wParam);
  //  break;
  case WM_LBUTTONUP: {
    return 0;
  }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
