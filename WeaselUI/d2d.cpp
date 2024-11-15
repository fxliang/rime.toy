#include "d2d.h"
#include <Dwmapi.h>
#include <ShellScalingApi.h>
#include <map>

namespace weasel {

#define STYLEORWEIGHT (L":[^:]*[^a-f0-9:]+[^:]*")

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
  auto func = [&](const wstring &font_face, const int font_point,
                  ComPtr<IDWriteTextFormat1> &_pTextFormat) {
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

    _ParseFontFace(font_face, fontWeight, fontStyle);
    // text font text format set up
    fontFaceStrVector = ws_split(font_face, L",");
    fontFaceStrVector[0] =
        std::regex_replace(fontFaceStrVector[0],
                           std::wregex(STYLEORWEIGHT, std::wregex::icase), L"");
    HR(m_pWriteFactory->CreateTextFormat(
        _mainFontFace.c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        font_point * (m_dpiY / 72.0f), L"",
        reinterpret_cast<IDWriteTextFormat **>(
            _pTextFormat.ReleaseAndGetAddressOf())));

    if (vertical_text) {
      _pTextFormat->SetFlowDirection(flow);
      _pTextFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
      _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    } else
      _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    // _pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    _pTextFormat->SetWordWrapping(wrapping);
    _SetFontFallback(_pTextFormat, fontFaceStrVector);
    if (m_style.linespacing && m_style.baseline)
      _pTextFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,
                                   font_point * linespacing,
                                   font_point * baseline);
    decltype(fontFaceStrVector)().swap(fontFaceStrVector);
  };
  func(m_style.font_face, m_style.font_point, pTextFormat);
  func(m_style.label_font_face, m_style.label_font_point, pLabelFormat);
  func(m_style.comment_font_face, m_style.comment_font_point, pCommentFormat);
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
  if (m_dpiY == 0) {
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
} // namespace weasel
