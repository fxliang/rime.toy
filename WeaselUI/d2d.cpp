#include "d2d.h"
#include <Dwmapi.h>
#include <ShellScalingApi.h>
#include <map>
#include <wincodec.h>

namespace weasel {

#define STYLEORWEIGHT (L":[^:]*[^a-f0-9:]+[^:]*")

D2D::D2D(UIStyle &style, HWND hwnd)
    : m_style(style), m_hWnd(hwnd), m_dpiX(96.0f), m_dpiY(96.0f) {
  InitDpiInfo();
  InitDirect2D();
}

D2D::~D2D() {
  SafeReleaseAll(direct3dDevice, dxgiDevice, dxFactory, swapChain, d2Factory,
                 d2Device, dc, surface, bitmap, dcompDevice, target, visual,
                 pPreeditFormat, pLabelFormat, pTextFormat, pCommentFormat,
                 m_pWriteFactory, m_pBrush);
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
  dc->SetTextAntialiasMode((D2D1_TEXT_ANTIALIAS_MODE)m_style.antialias_mode);
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
  target.Reset();
  HR(dcompDevice->CreateTargetForHwnd(m_hWnd,
                                      true, // Top most
                                      target.ReleaseAndGetAddressOf()));
  HR(dcompDevice->CreateVisual(visual.ReleaseAndGetAddressOf()));
  HR(visual->SetContent(swapChain.Get()));
  HR(target->SetRoot(visual.Get()));
  HR(dcompDevice->Commit());
  D2D1_COLOR_F const brushColor = D2D1::ColorF(D2D1::ColorF::Black);
  HR(dc->CreateSolidColorBrush(brushColor, m_pBrush.ReleaseAndGetAddressOf()));
}

void D2D::OnResize(UINT width, UINT height) {
  // Release Direct2D resources
  dc->SetTarget(nullptr);
  bitmap.Reset();
  surface.Reset();
  // Resize the swap chain
  HR(swapChain->ResizeBuffers(2, // Buffer count
                              width, height,
                              DXGI_FORMAT_B8G8R8A8_UNORM, // New format
                              0                           // Swap chain flags
                              ));
  // Retrieve the new swap chain's back buffer
  HR(swapChain->GetBuffer(
      0, // index
      __uuidof(surface.Get()),
      reinterpret_cast<void **>(surface.ReleaseAndGetAddressOf())));
  // Create a new Direct2D bitmap pointing to the new swap chain surface
  D2D1_BITMAP_PROPERTIES1 properties = {};
  properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
  properties.bitmapOptions =
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
  HR(dc->CreateBitmapFromDxgiSurface(surface.Get(), properties,
                                     bitmap.ReleaseAndGetAddressOf()));
  // Point the device context to the new bitmap for rendering
  dc->SetTarget(bitmap.Get());
  // Update DirectComposition target and visual
  HR(visual->SetContent(swapChain.Get()));
  HR(target->SetRoot(visual.Get()));
  HR(dcompDevice->Commit());
}

std::vector<std::wstring> ws_split(const std::wstring &in,
                                   const std::wstring &delim) {
  std::wregex re{delim};
  return std::vector<std::wstring>{
      std::wsregex_token_iterator(in.begin(), in.end(), re, -1),
      std::wsregex_token_iterator()};
}

static std::wstring MatchWordsOutLowerCaseTrim1st(const std::wstring &wstr,
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
  InitFontFormats(m_style.label_font_face, m_style.label_font_point,
                  m_style.font_face, m_style.font_point,
                  m_style.comment_font_face, m_style.comment_font_point);
}

void D2D::InitFontFormats(const wstring &label_font_face,
                          const int label_font_point, const wstring &font_face,
                          const int font_point,
                          const wstring &comment_font_face,
                          const int comment_font_point) {
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
  auto func = [&](const wstring &font_face, int font_point,
                  PtTextFormat &_pTextFormat, DWRITE_WORD_WRAPPING wrap) {
    bool vertical_text = m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT;
    std::vector<std::wstring> fontFaceStrVector;

    // set main font a invalid font name, to make every font range customizable
    const std::wstring _mainFontFace = L"_InvalidFontName_";
    DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH fontStretch = DWRITE_FONT_STRETCH_NORMAL;
    // convert percentage to float
    float linespacing =
        m_dpiScaleFontPoint * ((float)m_style.linespacing / 100.0f);
    float baseline = m_dpiScaleFontPoint * ((float)m_style.baseline / 100.0f);
    if (vertical_text)
      baseline = linespacing / 2;

    ParseFontFace(font_face, fontWeight, fontStyle, fontStretch);
    // text font text format set up
    fontFaceStrVector = ws_split(font_face, L",");
    fontFaceStrVector[0] =
        std::regex_replace(fontFaceStrVector[0],
                           std::wregex(STYLEORWEIGHT, std::wregex::icase), L"");
    if (font_point <= 0) {
      _pTextFormat.Reset();
      return;
    }
    HR(m_pWriteFactory->CreateTextFormat(
        _mainFontFace.c_str(), NULL, fontWeight, fontStyle, fontStretch,
        font_point * m_dpiScaleFontPoint, L"",
        reinterpret_cast<IDWriteTextFormat **>(
            _pTextFormat.ReleaseAndGetAddressOf())));

    if (vertical_text) {
      _pTextFormat->SetFlowDirection(flow);
      _pTextFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
      _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    } else
      _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    _pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // _pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    _pTextFormat->SetWordWrapping(wrap);
    SetFontFallback(_pTextFormat, fontFaceStrVector);
    if (m_style.linespacing && m_style.baseline)
      _pTextFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,
                                   font_point * linespacing,
                                   font_point * baseline);
    decltype(fontFaceStrVector)().swap(fontFaceStrVector);
  };
  func(font_face, font_point, pPreeditFormat, wrapping_preedit);
  func(font_face, font_point, pTextFormat, wrapping);
  func(label_font_face, label_font_point, pLabelFormat, wrapping);
  func(comment_font_face, comment_font_point, pCommentFormat, wrapping);
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

void D2D::SetBrushColor(uint32_t color) {
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float b = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float r = (color & 0xFF) / 255.0f;
  auto colorf = D2D1::ColorF(r, g, b, a);
  m_pBrush->SetColor(colorf);
}

void D2D::InitDpiInfo() {
  if (IsWindowsBlueOrLaterEx()) {
    HMONITOR const mon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    UINT x = 0, y = 0;
    HR(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y));
    m_dpiX = static_cast<float>(x);
    m_dpiY = static_cast<float>(y);
    if (m_dpiY == 0)
      m_dpiX = m_dpiY = 96.0;
  } else
    m_dpiY = m_dpiX = 96.0f;
  m_dpiScaleFontPoint = m_dpiY / 72.0f;
  m_dpiScaleLayout = m_dpiY / 96.0;
}

void D2D::SetFontFallback(PtTextFormat textFormat,
                          const std::vector<std::wstring> &fontVector) {
  ComPtr<IDWriteFontFallback> pSysFallback;
  HR(m_pWriteFactory->GetSystemFontFallback(
      pSysFallback.ReleaseAndGetAddressOf()));
  ComPtr<IDWriteFontFallback> pFontFallback = NULL;
  ComPtr<IDWriteFontFallbackBuilder> pFontFallbackBuilder = NULL;
  HR(m_pWriteFactory->CreateFontFallbackBuilder(
      pFontFallbackBuilder.ReleaseAndGetAddressOf()));
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
    const auto func = [](wstring wstr, UINT fallback) -> UINT {
      try {
        return std::stoul(wstr.c_str(), 0, 16);
      } catch (...) {
        return fallback;
      }
    };
    UINT first = func(firstWstr, 0), last = func(lastWstr, 0x10ffff);
    DWRITE_UNICODE_RANGE range = {first, last};
    const WCHAR *familys = {_fontFaceWstr.c_str()};
    HR(pFontFallbackBuilder->AddMapping(&range, 1, &familys, 1));
    decltype(fallbackFontsVector)().swap(fallbackFontsVector);
  }
  // add system defalt font fallback
  HR(pFontFallbackBuilder->AddMappings(pSysFallback.Get()));
  HR(pFontFallbackBuilder->CreateFontFallback(
      pFontFallback.ReleaseAndGetAddressOf()));
  HR(textFormat->SetFontFallback(pFontFallback.Get()));
  decltype(fallbackFontsVector)().swap(fallbackFontsVector);
}

void D2D::ParseFontFace(const std::wstring &fontFaceStr,
                        DWRITE_FONT_WEIGHT &fontWeight,
                        DWRITE_FONT_STYLE &fontStyle,
                        DWRITE_FONT_STRETCH &fontStretch) {
  const wstring patWeight(
      L"(:thin|:extra_light|:ultra_light|:light|:semi_light|:medium|:demi_bold|"
      L":semi_bold|:bold|:extra_bold|:ultra_bold|:black|:heavy|:extra_black|:"
      L"ultra_black)");
  const std::map<wstring, DWRITE_FONT_WEIGHT> _mapWeight = {
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
  const wstring weight = MatchWordsOutLowerCaseTrim1st(fontFaceStr, patWeight);
  const auto it = _mapWeight.find(weight);
  fontWeight =
      (it != _mapWeight.end()) ? it->second : DWRITE_FONT_WEIGHT_NORMAL;

  const wstring patStyle(L"(:italic|:oblique|:normal)");
  const std::map<wstring, DWRITE_FONT_STYLE> _mapStyle = {
      {L"italic", DWRITE_FONT_STYLE_ITALIC},
      {L"oblique", DWRITE_FONT_STYLE_OBLIQUE},
      {L"normal", DWRITE_FONT_STYLE_NORMAL},
  };
  const wstring style = MatchWordsOutLowerCaseTrim1st(fontFaceStr, patStyle);
  const auto it2 = _mapStyle.find(style);
  fontStyle = (it2 != _mapStyle.end()) ? it2->second : DWRITE_FONT_STYLE_NORMAL;

  const wstring patStretch(L"(:undefined|:ultra_condensed|:extra_condensed|"
                           L":condensed|:semi_condensed|"
                           L":normal_stretch|:medium_stretch|:semi_expanded|"
                           L":expanded|:extra_expanded|:ultra_expanded)");
  const std::map<wstring, DWRITE_FONT_STRETCH> _mapStretch = {
      {L"undefined", DWRITE_FONT_STRETCH_UNDEFINED},
      {L"ultra_condensed", DWRITE_FONT_STRETCH_ULTRA_CONDENSED},
      {L"extra_condensed", DWRITE_FONT_STRETCH_EXTRA_CONDENSED},
      {L"condensed", DWRITE_FONT_STRETCH_CONDENSED},
      {L"semi_condensed", DWRITE_FONT_STRETCH_SEMI_CONDENSED},
      {L"normal_stretch", DWRITE_FONT_STRETCH_NORMAL},
      {L"medium_stretch", DWRITE_FONT_STRETCH_MEDIUM},
      {L"semi_expanded", DWRITE_FONT_STRETCH_SEMI_EXPANDED},
      {L"expanded", DWRITE_FONT_STRETCH_EXPANDED},
      {L"extra_expanded", DWRITE_FONT_STRETCH_EXTRA_EXPANDED},
      {L"ultra_expanded", DWRITE_FONT_STRETCH_ULTRA_EXPANDED}};
  const wstring stretch =
      MatchWordsOutLowerCaseTrim1st(fontFaceStr, patStretch);
  const auto it3 = _mapStretch.find(stretch);
  fontStretch =
      (it3 != _mapStretch.end()) ? it3->second : DWRITE_FONT_STRETCH_NORMAL;
  if (DWRITE_FONT_STRETCH_UNDEFINED)
    fontStretch = DWRITE_FONT_STRETCH_NORMAL;
}

void D2D::GetTextSize(const wstring &text, size_t nCount,
                      PtTextFormat &pTextFormat, LPSIZE lpSize) {

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

// Helper function to convert IWICBitmap to a format compatible with Direct2D
static HRESULT
ConvertWicBitmapToSupportedFormat(IWICBitmap *pWicBitmap,
                                  IWICImagingFactory *pWicFactory,
                                  IWICFormatConverter **ppConvertedBitmap) {
  if (!pWicBitmap || !pWicFactory || !ppConvertedBitmap)
    return E_POINTER;

  // Create a format converter to convert to a Direct2D supported format
  ComPtr<IWICFormatConverter> pWicFormatConverter;
  HRESULT hr = pWicFactory->CreateFormatConverter(
      pWicFormatConverter.ReleaseAndGetAddressOf());
  if (FAILED(hr))
    return hr;
  // Convert the bitmap to a format supported by Direct2D (e.g.,
  // DXGI_FORMAT_B8G8R8A8_UNORM)
  hr = pWicFormatConverter->Initialize(
      pWicBitmap,                    // Source bitmap
      GUID_WICPixelFormat32bppPBGRA, // Supported pixel format for Direct2D
                                     // (BGRA, 32bpp)
      WICBitmapDitherTypeNone,       // No dithering
      nullptr,                       // No palette
      0.0f,                          // No alpha threshold
      WICBitmapPaletteTypeCustom);   // Custom palette (none in this case)

  if (FAILED(hr))
    return hr;
  *ppConvertedBitmap = pWicFormatConverter.Detach();
  return S_OK;
}

HRESULT D2D::GetBmpFromIcon(HICON hIcon, ComPtr<ID2D1Bitmap1> &pBitmap) {
  if (!hIcon)
    return S_FALSE; // Failed to load icon
  // Get icon info and HBITMAP
  ICONINFO iconInfo;
  if (!GetIconInfo(hIcon, &iconInfo))
    return S_FALSE; // Failed to get icon info
  HBITMAP hBitmap = iconInfo.hbmColor;
  if (!hBitmap)
    return S_FALSE; // Failed to get bitmap from icon
  // Create a WIC factory
  ComPtr<IWICImagingFactory> pWicFactory;
  HR(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                      IID_PPV_ARGS(pWicFactory.ReleaseAndGetAddressOf())));
  // Create WIC Bitmap from HBITMAP
  ComPtr<IWICBitmap> pWicBitmap;
  HR(pWicFactory->CreateBitmapFromHBITMAP(hBitmap, nullptr, WICBitmapUseAlpha,
                                          pWicBitmap.ReleaseAndGetAddressOf()));
  // Convert the bitmap to a Direct2D compatible format
  ComPtr<IWICFormatConverter> pConvertedBitmap;
  HR(ConvertWicBitmapToSupportedFormat(
      pWicBitmap.Get(), pWicFactory.Get(),
      pConvertedBitmap.ReleaseAndGetAddressOf()));
  HR(dc->CreateBitmapFromWicBitmap(pConvertedBitmap.Get(), nullptr,
                                   pBitmap.ReleaseAndGetAddressOf()));
  return S_OK;
}

HRESULT D2D::GetIconFromFile(const wstring &iconPath,
                             ComPtr<ID2D1Bitmap1> &pD2DBitmap) {
  // Step 1: Create a WIC factory
  ComPtr<IWICImagingFactory> pWicFactory;
  HRESULT hr =
      CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(pWicFactory.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    DEBUG << "Failed to create WIC imaging factory, HRESULT: " << std::hex
          << hr;
    return hr;
  }

  // Step 2: Load the image from file into a WICBitmapDecoder
  ComPtr<IWICBitmapDecoder> pDecoder;
  hr = pWicFactory->CreateDecoderFromFilename(
      iconPath.c_str(), nullptr, GENERIC_READ,
      WICDecodeOptions::WICDecodeMetadataCacheOnLoad,
      pDecoder.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    DEBUG << "Failed to load image from file, HRESULT: " << std::hex << hr;
    return hr;
  }

  // Step 3: Get the first frame of the image
  ComPtr<IWICBitmapFrameDecode> pFrame;
  hr = pDecoder->GetFrame(0, pFrame.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    DEBUG << "Failed to get frame from decoder, HRESULT: " << std::hex << hr;
    return hr;
  }

  // Step 4: Convert the frame to a supported format using IWICFormatConverter
  ComPtr<IWICFormatConverter> pConvertedBitmap;
  hr = pWicFactory->CreateFormatConverter(
      pConvertedBitmap.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    DEBUG << "Failed to create IWICFormatConverter, HRESULT: " << std::hex
          << hr;
    return hr;
  }

  // Initialize the format converter
  hr = pConvertedBitmap->Initialize(
      pFrame.Get(), // The source bitmap (IWICBitmapFrameDecode)
      GUID_WICPixelFormat32bppPBGRA, // Target format (Direct2D supported
                                     // format)
      WICBitmapDitherTypeNone,       // Dithering type
      nullptr,                       // Palette (nullptr to use default)
      0.0f,                      // Alpha threshold (0.0 for no transparency)
      WICBitmapPaletteTypeCustom // Palette type (Custom)
  );
  if (FAILED(hr)) {
    DEBUG << "Failed to initialize IWICFormatConverter, HRESULT: " << std::hex
          << hr;
    return hr;
  }

  // Step 5: Create a Direct2D bitmap from the converted WIC bitmap
  hr = dc->CreateBitmapFromWicBitmap(pConvertedBitmap.Get(), nullptr,
                                     pD2DBitmap.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    DEBUG << "Failed to create Direct2D bitmap from WIC bitmap, HRESULT: "
          << std::hex << hr;
    return hr;
  }

  return S_OK;
}

HRESULT
D2D::CreateRoundedRectanglePath(const RECT &rc, float radius,
                                const IsToRoundStruct &roundInfo,
                                ComPtr<ID2D1PathGeometry> &pPathGeometry) {
#define PT2F(x, y) D2D1::Point2F(x, y)
  // 创建路径几何对象
  HRESULT hr =
      d2Factory->CreatePathGeometry(pPathGeometry.ReleaseAndGetAddressOf());
  FAILEDACTION(hr, return hr);
  // 打开路径几何
  ComPtr<ID2D1GeometrySink> pSink = nullptr;
  hr = pPathGeometry->Open(pSink.ReleaseAndGetAddressOf());
  FAILEDACTION(hr, return hr);
  pSink->SetFillMode(D2D1_FILL_MODE_WINDING);
  // if radius is 0, return a rectangle path
  if (radius <= 0.0f) {
    pSink->BeginFigure(D2D1::Point2F((float)rc.left, (float)rc.top),
                       D2D1_FIGURE_BEGIN_FILLED);
    pSink->AddLine(D2D1::Point2F((float)rc.right, (float)rc.top));
    pSink->AddLine(D2D1::Point2F((float)rc.right, (float)rc.bottom));
    pSink->AddLine(D2D1::Point2F((float)rc.left, (float)rc.bottom));
    pSink->AddLine(D2D1::Point2F((float)rc.left, (float)rc.top));
  } else {
    D2D1_RECT_F rectf{(float)rc.left, (float)rc.top, (float)rc.right,
                      (float)rc.bottom};
    radius = MIN(radius, (rc.right - rc.left) / 2.0f);
    radius = MIN(radius, (rc.bottom - rc.top) / 2.0f);
    const float good_radius = 0.382f * radius;
    // 从左上角开始
    if (roundInfo.IsTopLeftNeedToRound) {
      pSink->BeginFigure(PT2F(rectf.left + radius, rectf.top),
                         D2D1_FIGURE_BEGIN_FILLED);
    } else
      pSink->BeginFigure(PT2F(rectf.left, rectf.top), D2D1_FIGURE_BEGIN_FILLED);
    // 顶边
    pSink->AddLine(PT2F(rectf.right - radius, rectf.top));
    // 右上角圆角
    if (roundInfo.IsTopRightNeedToRound) {
      D2D1_BEZIER_SEGMENT seg{PT2F(rectf.right - good_radius, rectf.top),
                              PT2F(rectf.right, rectf.top + good_radius),
                              PT2F(rectf.right, rectf.top + radius)};
      pSink->AddBezier(seg);
    } else
      pSink->AddLine(PT2F(rectf.right, rectf.top));
    // 右边
    pSink->AddLine(PT2F(rectf.right, rectf.bottom - radius));
    // 右下角圆角
    if (roundInfo.IsBottomRightNeedToRound) {
      D2D1_BEZIER_SEGMENT seg{PT2F(rectf.right, rectf.bottom - good_radius),
                              PT2F(rectf.right - good_radius, rectf.bottom),
                              PT2F(rectf.right - radius, rectf.bottom)};
      pSink->AddBezier(seg);
    } else
      pSink->AddLine(PT2F(rectf.right, rectf.bottom));
    // 底边
    pSink->AddLine(PT2F(rectf.left + radius, rectf.bottom));
    // 左下角圆角
    if (roundInfo.IsBottomLeftNeedToRound) {
      D2D1_BEZIER_SEGMENT seg{PT2F(rectf.left + good_radius, rectf.bottom),
                              PT2F(rectf.left, rectf.bottom - good_radius),
                              PT2F(rectf.left, rectf.bottom - radius)};
      pSink->AddBezier(seg);
    } else
      pSink->AddLine(PT2F(rectf.left, rectf.bottom));
    // 左边
    pSink->AddLine(PT2F(rectf.left, rectf.top + radius));
    // 左上角圆角
    if (roundInfo.IsTopLeftNeedToRound) {
      D2D1_BEZIER_SEGMENT seg{PT2F(rectf.left, rectf.top + good_radius),
                              PT2F(rectf.left + good_radius, rectf.top),
                              PT2F(rectf.left + radius, rectf.top)};
      pSink->AddBezier(seg);
    } else {
      pSink->AddLine(PT2F(rectf.left, rectf.top));
    }
  }
  // 闭合路径
  pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
  hr = pSink->Close();
#undef PT2F
  return hr;
}

HRESULT D2D::FillGeometry(const CRect &rect, uint32_t color, uint32_t radius,
                          IsToRoundStruct roundInfo, bool to_blur) {
  SetBrushColor(color);
  ComPtr<ID2D1PathGeometry> pGeometry;
  if (to_blur) {
    CRect rc = rect;
    rc.OffsetRect(m_dpiScaleLayout * m_style.shadow_offset_x,
                  m_dpiScaleLayout * m_style.shadow_offset_y);
    CreateRoundedRectanglePath(rc, radius, roundInfo, pGeometry);
    ComPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
    HR(dc->CreateCompatibleRenderTarget(&bitmapRenderTarget));
    bitmapRenderTarget->BeginDraw();
    bitmapRenderTarget->FillGeometry(pGeometry.Get(), m_pBrush.Get());
    bitmapRenderTarget->EndDraw();
    // Get the bitmap from the bitmap render target
    ComPtr<ID2D1Bitmap> bitmap;
    HR(bitmapRenderTarget->GetBitmap(&bitmap));
    //// Create a Gaussian blur effect
    ComPtr<ID2D1Effect> blurEffect;
    HR(dc->CreateEffect(CLSID_D2D1GaussianBlur, &blurEffect));
    blurEffect->SetInput(0, bitmap.Get());
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION,
                         (float)m_style.shadow_radius);
    // Draw the blurred rounded rectangle onto the main render target
    dc->DrawImage(blurEffect.Get());
  } else {
    HR(CreateRoundedRectanglePath(rect, radius, roundInfo, pGeometry));
    dc->FillGeometry(pGeometry.Get(), m_pBrush.Get());
  }
  return S_OK;
}

static uint32_t revert_color(uint32_t &color) {
  // Revert color to D2D1 format
  uint32_t a = (color >> 24) & 0xFF;
  uint32_t b = (color >> 16) & 0xFF;
  uint32_t g = (color >> 8) & 0xFF;
  uint32_t r = color & 0xFF;
  uint32_t color_ret =
      ((255 - r) << 16) | ((255 - g) << 8) | ((255 - b) << 0) | (a << 24);
  return color_ret;
}

HRESULT D2D::DrawTextLayout(ComPtr<IDWriteTextLayout> pTextLayout, float x,
                            float y, uint32_t color, bool shadow) {
  if (shadow) {
    SetBrushColor(revert_color(color));
    ComPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
    HR(dc->CreateCompatibleRenderTarget(&bitmapRenderTarget));
    bitmapRenderTarget->BeginDraw();
    bitmapRenderTarget->DrawTextLayout(
        {x, y}, pTextLayout.Get(), m_pBrush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    bitmapRenderTarget->EndDraw();
    ComPtr<ID2D1Bitmap> bitmap;
    HR(bitmapRenderTarget->GetBitmap(&bitmap));
    //// Create a Gaussian blur effect
    ComPtr<ID2D1Effect> blurEffect;
    HR(dc->CreateEffect(CLSID_D2D1GaussianBlur, &blurEffect));
    blurEffect->SetInput(0, bitmap.Get());
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 4.0f);
    // Draw the blurred rounded rectangle onto the main render target
    dc->DrawImage(blurEffect.Get());
    SetBrushColor(color);
  }
  dc->DrawTextLayout({x, y}, pTextLayout.Get(), m_pBrush.Get(),
                     D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
  return S_OK;
}
} // namespace weasel
