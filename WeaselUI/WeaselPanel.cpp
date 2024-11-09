#include "WeaselPanel.h"
#include <DWrite.h>
#include <map>
#include <regex>
#include <string>
#include <vector>

using namespace weasel;

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define STYLEORWEIGHT (L":[^:]*[^a-f0-9:]+[^:]*")

WeaselPanel::WeaselPanel(UI &ui)
    : m_hWnd(nullptr), m_horizontal(ui.horizontal()), m_ctx(ui.ctx()),
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

void WeaselPanel::Refresh() {
  if (m_hWnd)
    InvalidateRect(m_hWnd, NULL, TRUE); // 请求重绘
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
  if (m_horizontal) {
    DrawHorizontal();
  } else {
    DrawUIVertical();
  }
  HR(m_pD2D->dc->EndDraw());
  // Make the swap chain available to the composition engine
  HR(m_pD2D->swapChain->Present(1, 0)); // sync
}

void WeaselPanel::DrawTextAt(const wstring &text, size_t x, size_t y) {
  ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
  HR(m_pD2D->m_pWriteFactory->CreateTextLayout(
      text.c_str(), text.length(), m_pD2D->pTextFormat.Get(), m_winSize.cx,
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
      m_pD2D->GetTextSize(label, label.length(), m_pD2D->pTextFormat,
                          &textSize);
      h = MAX((LONG)h, textSize.cy);
      x += textSize.cx + m_padding;
      m_pD2D->GetTextSize(candie, candie.length(), m_pD2D->pTextFormat,
                          &textSize);
      h = MAX((LONG)h, textSize.cy);
      if (!comment.empty()) {
        x += textSize.cx + m_padding;
        m_pD2D->GetTextSize(comment, comment.length(), m_pD2D->pTextFormat,
                            &textSize);
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
    DrawTextAt(preedit, m_padding, m_padding);
    winSize.cx = textSize.cx;
    winSize.cy = textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = (m_ctx.aux.str);
    m_pD2D->GetTextSize(aux, aux.length(), m_pD2D->pTextFormat, &textSize);
    DrawTextAt(aux, m_padding, m_padding);
    winSize.cx = textSize.cx;
    winSize.cy = textSize.cy;
  }
  if (m_ctx.cinfo.candies.size()) {
    winSize.cy += m_padding;
    DrawTextAt(m_text, m_padding, winSize.cy);
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
    DrawTextAt(preedit, m_padding, m_padding);
    winSize.cx += m_padding + textSize.cx;
    winSize.cy += m_padding + textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = (m_ctx.aux.str);
    m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(m_style.text_color));
    DrawTextAt(aux, m_padding, m_padding);
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
      m_pD2D->GetTextSize(label, label.length(), m_pD2D->pTextFormat,
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
        m_pD2D->GetTextSize(comment, comment.length(), m_pD2D->pTextFormat,
                            &textSize);
        xc = x;
        yc = y;
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
      DrawTextAt(label, xl, yl);
      color = hilited ? m_style.hilited_candidate_text_color
                      : m_style.candidate_text_color;
      m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(color));
      DrawTextAt(candie, xt, yt);
      if (!comment.empty()) {
        color = hilited ? m_style.hilited_comment_text_color
                        : m_style.comment_text_color;
        m_pD2D->m_pBrush->SetColor(D2d1ColorFromColorRef(color));
        DrawTextAt(comment, xc, yc);
      }
      winSize.cy += h;
    }
    winSize.cy += m_padding;
    m_winSize.cx = cand_width;
    m_winSize.cy = winSize.cy;
  }
}

void WeaselPanel::OnPaint() {
  if (m_horizontal) {
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

D2D::D2D(UIStyle &style, HWND hwnd)
    : m_style(style), m_hWnd(hwnd), m_dpiX(96.0f), m_dpiY(96.0f) {
  InitDpiInfo();
  InitDirect2D();
  InitDirectWriteResources();
}

void D2D::InitDirect2D() {
  HR(D3D11CreateDevice(nullptr, // Adapter
                       D3D_DRIVER_TYPE_HARDWARE,
                       nullptr, // Module
                       D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                       0, // Highest available feature level
                       D3D11_SDK_VERSION, &direct3dDevice,
                       nullptr,   // Actual feature level
                       nullptr)); // Device context
  HR(direct3dDevice.As(&dxgiDevice));
  HR(CreateDXGIFactory2(
      0, __uuidof(dxFactory.Get()),
      reinterpret_cast<void **>(dxFactory.ReleaseAndGetAddressOf())));
  DXGI_SWAP_CHAIN_DESC1 description = {};
  description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  description.BufferCount = 2;
  description.SampleDesc.Count = 1;
  description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

  RECT rect = {};
  GetClientRect(m_hWnd, &rect);
  description.Width = rect.right - rect.left;
  description.Height = rect.bottom - rect.top;
  HR(dxFactory->CreateSwapChainForComposition(
      dxgiDevice.Get(), &description,
      nullptr, // Don抰 restrict
      swapChain.ReleaseAndGetAddressOf()));
  // Create a single-threaded Direct2D factory with debugging information
  D2D1_FACTORY_OPTIONS const options = {D2D1_DEBUG_LEVEL_INFORMATION};
  HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
                       d2Factory.ReleaseAndGetAddressOf()));
  // Create the Direct2D device that links back to the Direct3D device
  HR(d2Factory->CreateDevice(dxgiDevice.Get(),
                             d2Device.ReleaseAndGetAddressOf()));
  // Create the Direct2D device context that is the actual render target
  // and exposes drawing commands
  HR(d2Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                   dc.ReleaseAndGetAddressOf()));
  dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  // Retrieve the swap chain's back buffer
  HR(swapChain->GetBuffer(
      0, // index
      __uuidof(surface.Get()),
      reinterpret_cast<void **>(surface.ReleaseAndGetAddressOf())));
  // Create a Direct2D bitmap that points to the swap chain surface
  D2D1_BITMAP_PROPERTIES1 properties = {};
  properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
  properties.bitmapOptions =
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
  HR(dc->CreateBitmapFromDxgiSurface(surface.Get(), properties,
                                     bitmap.ReleaseAndGetAddressOf()));
  // Point the device context to the bitmap for rendering
  dc->SetTarget(bitmap.Get());
  HR(DCompositionCreateDevice(
      dxgiDevice.Get(), __uuidof(dcompDevice.Get()),
      reinterpret_cast<void **>(dcompDevice.ReleaseAndGetAddressOf())));
  HR(dcompDevice->CreateTargetForHwnd(m_hWnd,
                                      true, // Top most
                                      target.ReleaseAndGetAddressOf()));
  HR(dcompDevice->CreateVisual(visual.ReleaseAndGetAddressOf()));
  HR(visual->SetContent(swapChain.Get()));
  HR(target->SetRoot(visual.Get()));
  HR(dcompDevice->Commit());
  D2D1_COLOR_F const brushColor = D2D1::ColorF(0.18f,  // red
                                               0.55f,  // green
                                               0.34f,  // blue
                                               0.75f); // alpha
  HR(dc->CreateSolidColorBrush(brushColor, brush.ReleaseAndGetAddressOf()));
}

std::vector<std::wstring> ws_split(const std::wstring &in,
                                   const std::wstring &delim) {
  std::wregex re{delim};
  return std::vector<std::wstring>{
      std::wsregex_token_iterator(in.begin(), in.end(), re, -1),
      std::wsregex_token_iterator()};
}

static std::wstring _MatchWordsOutLowerCaseTrim1st(const std::wstring &wstr,
                                                   const std::wstring &pat) {
  std::wstring mat = L"";
  std::wsmatch mc;
  std::wregex pattern(pat, std::wregex::icase);
  std::wstring::const_iterator iter = wstr.cbegin();
  std::wstring::const_iterator end = wstr.cend();
  while (regex_search(iter, end, mc, pattern)) {
    for (const auto &m : mc) {
      mat = m;
      mat = mat.substr(1);
      break;
    }
    iter = mc.suffix().first;
  }
  std::wstring res;
  std::transform(mat.begin(), mat.end(), std::back_inserter(res), ::tolower);
  return res;
}

void D2D::InitFontFormats() {
  DWRITE_WORD_WRAPPING wrapping =
      ((m_style.max_width == 0 &&
        m_style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT) ||
       (m_style.max_height == 0 &&
        m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT))
          ? DWRITE_WORD_WRAPPING_NO_WRAP
          : DWRITE_WORD_WRAPPING_WHOLE_WORD;
  DWRITE_WORD_WRAPPING wrapping_preedit =
      ((m_style.max_width == 0 &&
        m_style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT) ||
       (m_style.max_height == 0 &&
        m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT))
          ? DWRITE_WORD_WRAPPING_NO_WRAP
          : DWRITE_WORD_WRAPPING_CHARACTER;
  DWRITE_FLOW_DIRECTION flow = m_style.vertical_text_left_to_right
                                   ? DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT
                                   : DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT;

  HRESULT hResult = S_OK;
  bool vertical_text = m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT;
  std::vector<std::wstring> fontFaceStrVector;

  // set main font a invalid font name, to make every font range customizable
  const std::wstring _mainFontFace = L"_InvalidFontName_";
  DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
  DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
  // convert percentage to float
  float linespacing =
      m_dpiScaleFontPoint * ((float)m_style.linespacing / 100.0f);
  float baseline = m_dpiScaleFontPoint * ((float)m_style.baseline / 100.0f);
  if (vertical_text)
    baseline = linespacing / 2;
  _ParseFontFace(m_style.font_face, fontWeight, fontStyle);
  // text font text format set up
  fontFaceStrVector = ws_split(m_style.font_face, L",");
  fontFaceStrVector[0] =
      std::regex_replace(fontFaceStrVector[0],
                         std::wregex(STYLEORWEIGHT, std::wregex::icase), L"");
  HR(m_pWriteFactory->CreateTextFormat(
      _mainFontFace.c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      m_style.font_point * (m_dpiY / 72.0f), L"",
      reinterpret_cast<IDWriteTextFormat **>(
          pTextFormat.ReleaseAndGetAddressOf())));

  if (vertical_text) {
    pTextFormat->SetFlowDirection(flow);
    pTextFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  } else
    pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

  // pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  pTextFormat->SetWordWrapping(wrapping);
  _SetFontFallback(pTextFormat, fontFaceStrVector);
  if (m_style.linespacing && m_style.baseline)
    pTextFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,
                                m_style.font_point * linespacing,
                                m_style.font_point * baseline);
  decltype(fontFaceStrVector)().swap(fontFaceStrVector);
}

void D2D::InitDirectWriteResources() {
  // create dwrite objs
  HR(DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
      reinterpret_cast<IUnknown **>(m_pWriteFactory.ReleaseAndGetAddressOf())));
  InitFontFormats();
  HR(dc->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                               m_pBrush.ReleaseAndGetAddressOf()));
}

inline BOOL GetVersionEx2(LPOSVERSIONINFOW lpVersionInformation) {
  HMODULE hNtDll = GetModuleHandleW(L"NTDLL"); // 获取ntdll.dll的句柄
  typedef NTSTATUS(NTAPI * tRtlGetVersion)(
      PRTL_OSVERSIONINFOW povi); // RtlGetVersion的原型
  tRtlGetVersion pRtlGetVersion = NULL;
  if (hNtDll) {
    pRtlGetVersion = (tRtlGetVersion)GetProcAddress(
        hNtDll, "RtlGetVersion"); // 获取RtlGetVersion地址
  }
  if (pRtlGetVersion) {
    return pRtlGetVersion((PRTL_OSVERSIONINFOW)lpVersionInformation) >=
           0; // 调用RtlGetVersion
  }
  return FALSE;
}

static inline BOOL IsWinVersionGreaterThan(DWORD dwMajorVersion,
                                           DWORD dwMinorVersion) {
  OSVERSIONINFOEXW ovi = {sizeof ovi};
  GetVersionEx2((LPOSVERSIONINFOW)&ovi);
  if ((ovi.dwMajorVersion == dwMajorVersion &&
       ovi.dwMinorVersion >= dwMinorVersion) ||
      ovi.dwMajorVersion > dwMajorVersion)
    return true;
  else
    return false;
}

// Use WinBlue for Windows 8.1
#define IsWindowsBlueOrLaterEx() IsWinVersionGreaterThan(6, 3)

void D2D::InitDpiInfo() {
  if (!IsWindowsBlueOrLaterEx())
    return;
  HMONITOR const monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
  unsigned x = 0;
  unsigned y = 0;
  HR(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &x, &y));
  m_dpiX = static_cast<float>(x);
  m_dpiY = static_cast<float>(y);
  if (m_dpiY) {
    m_dpiX = m_dpiY = 96.0;
    m_dpiScaleFontPoint = m_dpiY / 72.0f;
  }
}

void D2D::_SetFontFallback(ComPtr<IDWriteTextFormat1> textFormat,
                           const std::vector<std::wstring> &fontVector) {
  ComPtr<IDWriteFontFallback> pSysFallback;
  HR(m_pWriteFactory->GetSystemFontFallback(pSysFallback.GetAddressOf()));
  ComPtr<IDWriteFontFallback> pFontFallback = NULL;
  ComPtr<IDWriteFontFallbackBuilder> pFontFallbackBuilder = NULL;
  HR(m_pWriteFactory->CreateFontFallbackBuilder(
      pFontFallbackBuilder.GetAddressOf()));
  std::vector<std::wstring> fallbackFontsVector;
  for (UINT32 i = 0; i < fontVector.size(); i++) {
    fallbackFontsVector = ws_split(fontVector[i], L":");
    std::wstring _fontFaceWstr, firstWstr, lastWstr;
    if (fallbackFontsVector.size() == 3) {
      _fontFaceWstr = fallbackFontsVector[0];
      firstWstr = fallbackFontsVector[1];
      lastWstr = fallbackFontsVector[2];
      if (lastWstr.empty())
        lastWstr = L"10ffff";
      if (firstWstr.empty())
        firstWstr = L"0";
    } else if (fallbackFontsVector.size() == 2) // fontName : codepoint
    {
      _fontFaceWstr = fallbackFontsVector[0];
      firstWstr = fallbackFontsVector[1];
      if (firstWstr.empty())
        firstWstr = L"0";
      lastWstr = L"10ffff";
    } else if (fallbackFontsVector.size() ==
               1) // if only font defined, use all range
    {
      _fontFaceWstr = fallbackFontsVector[0];
      firstWstr = L"0";
      lastWstr = L"10ffff";
    }
    UINT first = 0, last = 0x10ffff;
    try {
      first = std::stoi(firstWstr.c_str(), 0, 16);
    } catch (...) {
      first = 0;
    }
    try {
      last = std::stoi(lastWstr.c_str(), 0, 16);
    } catch (...) {
      last = 0x10ffff;
    }
    DWRITE_UNICODE_RANGE range = {first, last};
    const WCHAR *familys = {_fontFaceWstr.c_str()};
    HR(pFontFallbackBuilder->AddMapping(&range, 1, &familys, 1));
    decltype(fallbackFontsVector)().swap(fallbackFontsVector);
  }
  // add system defalt font fallback
  HR(pFontFallbackBuilder->AddMappings(pSysFallback.Get()));
  HR(pFontFallbackBuilder->CreateFontFallback(pFontFallback.GetAddressOf()));
  HR(textFormat->SetFontFallback(pFontFallback.Get()));
  decltype(fallbackFontsVector)().swap(fallbackFontsVector);
}

void D2D::_ParseFontFace(const std::wstring &fontFaceStr,
                         DWRITE_FONT_WEIGHT &fontWeight,
                         DWRITE_FONT_STYLE &fontStyle) {
  const std::wstring patWeight(
      L"(:thin|:extra_light|:ultra_light|:light|:semi_light|:medium|:demi_bold|"
      L":semi_bold|:bold|:extra_bold|:ultra_bold|:black|:heavy|:extra_black|:"
      L"ultra_black)");
  const std::map<std::wstring, DWRITE_FONT_WEIGHT> _mapWeight = {
      {L"thin", DWRITE_FONT_WEIGHT_THIN},
      {L"extra_light", DWRITE_FONT_WEIGHT_EXTRA_LIGHT},
      {L"ultra_light", DWRITE_FONT_WEIGHT_ULTRA_LIGHT},
      {L"light", DWRITE_FONT_WEIGHT_LIGHT},
      {L"semi_light", DWRITE_FONT_WEIGHT_SEMI_LIGHT},
      {L"medium", DWRITE_FONT_WEIGHT_MEDIUM},
      {L"demi_bold", DWRITE_FONT_WEIGHT_DEMI_BOLD},
      {L"semi_bold", DWRITE_FONT_WEIGHT_SEMI_BOLD},
      {L"bold", DWRITE_FONT_WEIGHT_BOLD},
      {L"extra_bold", DWRITE_FONT_WEIGHT_EXTRA_BOLD},
      {L"ultra_bold", DWRITE_FONT_WEIGHT_ULTRA_BOLD},
      {L"black", DWRITE_FONT_WEIGHT_BLACK},
      {L"heavy", DWRITE_FONT_WEIGHT_HEAVY},
      {L"extra_black", DWRITE_FONT_WEIGHT_EXTRA_BLACK},
      {L"normal", DWRITE_FONT_WEIGHT_NORMAL},
      {L"ultra_black", DWRITE_FONT_WEIGHT_ULTRA_BLACK}};
  std::wstring weight = _MatchWordsOutLowerCaseTrim1st(fontFaceStr, patWeight);
  auto it = _mapWeight.find(weight);
  fontWeight =
      (it != _mapWeight.end()) ? it->second : DWRITE_FONT_WEIGHT_NORMAL;

  const std::wstring patStyle(L"(:italic|:oblique|:normal)");
  const std::map<std::wstring, DWRITE_FONT_STYLE> _mapStyle = {
      {L"italic", DWRITE_FONT_STYLE_ITALIC},
      {L"oblique", DWRITE_FONT_STYLE_OBLIQUE},
      {L"normal", DWRITE_FONT_STYLE_NORMAL},
  };
  std::wstring style = _MatchWordsOutLowerCaseTrim1st(fontFaceStr, patStyle);
  auto it2 = _mapStyle.find(style);
  fontStyle = (it2 != _mapStyle.end()) ? it2->second : DWRITE_FONT_STYLE_NORMAL;
}

void D2D::GetTextSize(const wstring &text, size_t nCount,
                      ComPtr<IDWriteTextFormat1> pTextFormat, LPSIZE lpSize) {

  D2D1_SIZE_F sz;

  if (!pTextFormat) {
    lpSize->cx = 0;
    lpSize->cy = 0;
    return;
  }
  ComPtr<IDWriteTextLayout> pTextLayout;
  bool vertical_text_layout =
      (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT);
  if (vertical_text_layout)
    HR(m_pWriteFactory->CreateTextLayout(
        text.c_str(), (int)nCount, pTextFormat.Get(), 0.0f,
        (float)m_style.max_height, pTextLayout.ReleaseAndGetAddressOf()));
  else
    HR(m_pWriteFactory->CreateTextLayout(
        text.c_str(), (int)nCount, pTextFormat.Get(), (float)m_style.max_width,
        0, pTextLayout.ReleaseAndGetAddressOf()));
  if (vertical_text_layout) {
    DWRITE_FLOW_DIRECTION flow = m_style.vertical_text_left_to_right
                                     ? DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT
                                     : DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT;
    HR(pTextLayout->SetReadingDirection(
        DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    HR(pTextLayout->SetFlowDirection(flow));
  }

  DWRITE_TEXT_METRICS textMetrics;
  HR(pTextLayout->GetMetrics(&textMetrics));
  sz = D2D1::SizeF(ceil(textMetrics.widthIncludingTrailingWhitespace),
                   ceil(textMetrics.height));
  lpSize->cx = (int)sz.width;
  lpSize->cy = (int)sz.height;

  if (vertical_text_layout) {
    auto max_height =
        m_style.max_height == 0 ? textMetrics.height : m_style.max_height;
    HR(m_pWriteFactory->CreateTextLayout(
        text.c_str(), (int)nCount, pTextFormat.Get(),
        textMetrics.widthIncludingTrailingWhitespace, max_height,
        pTextLayout.ReleaseAndGetAddressOf()));
  } else {
    auto max_width = m_style.max_width == 0
                         ? textMetrics.widthIncludingTrailingWhitespace
                         : m_style.max_width;
    HR(m_pWriteFactory->CreateTextLayout(
        text.c_str(), (int)nCount, pTextFormat.Get(), max_width,
        textMetrics.height, pTextLayout.ReleaseAndGetAddressOf()));
  }

  if (vertical_text_layout) {
    HR(pTextLayout->SetReadingDirection(
        DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    HR(pTextLayout->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));
  }
  DWRITE_OVERHANG_METRICS overhangMetrics;
  HR(pTextLayout->GetOverhangMetrics(&overhangMetrics));
  {
    if (overhangMetrics.left > 0)
      lpSize->cx += (LONG)(overhangMetrics.left + 1);
    if (overhangMetrics.right > 0)
      lpSize->cx += (LONG)(overhangMetrics.right + 1);
    if (overhangMetrics.top > 0)
      lpSize->cy += (LONG)(overhangMetrics.top + 1);
    if (overhangMetrics.bottom > 0)
      lpSize->cy += (LONG)(overhangMetrics.bottom + 1);
  }
}
