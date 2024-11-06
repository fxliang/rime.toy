#pragma once
#ifndef _UI_H_
#define _UI_H_
#include <d2d1.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <d3d11_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dwrite_2.h>
#include <dxgi1_3.h>

#include <data.h>
#include <math.h>
#include <shellscalingapi.h>
#include <string>
#include <windows.h>
#include <wrl.h>

#include <format>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace Microsoft::WRL;
using wstring = std::wstring;
using string = std::string;

struct ComException {
  HRESULT result;
  ComException(HRESULT const value) : result(value) {}
};
void HR(HRESULT const result) {
  if (S_OK != result) {
    throw ComException(result);
  }
}

class PopupWindow {
public:
  PopupWindow()
      : m_hWnd(nullptr), m_pBrush(nullptr), m_pWriteFactory(nullptr),
        pTextFormat(nullptr) {
    Create();
  }
  ~PopupWindow() {}
  void UpdatePos(int x, int y) {
    if (m_hWnd) {
      RECT rc;
      GetWindowRect(m_hWnd, &rc);
      auto h = rc.bottom - rc.top;
      SetWindowPos(m_hWnd, HWND_TOPMOST, x, y - h, 0, 0,
                   SWP_NOSIZE | SWP_NOACTIVATE);
    }
  }
  void UpdatePosition(RECT rc) {
    if (m_hWnd) {
      m_inputPos = rc;
      SetWindowPos(m_hWnd, HWND_TOPMOST, m_inputPos.left, m_inputPos.bottom, 0,
                   0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
  }
  void Show(bool shown = true) {
    if (shown)
      ShowWindow(m_hWnd, SW_SHOW);
    else
      ShowWindow(m_hWnd, SW_HIDE);
  }
  void Hide() {
    if (m_hWnd)
      ShowWindow(m_hWnd, SW_HIDE);
  }
  void SetText(const wstring &aux) {
    m_aux = aux;
    m_ctx.clear();
    m_ctx.aux.str = wtou8(aux);
    if (m_hWnd)
      InvalidateRect(m_hWnd, NULL, TRUE); // 请求重绘
  }
  void Update(const Context &ctx, const Status &sta) {
    m_ctx = ctx;
    m_status = sta;
    Refresh();
  }
  void Refresh() {
    if (!m_hWnd)
      return;
    if (timer) {
      Show(false);
      KillTimer(m_hWnd, AUTOHIDE_TIMER);
    }
    if (m_hWnd)
      InvalidateRect(m_hWnd, NULL, TRUE); // 请求重绘
  }
  void ShowWithTimeout(size_t millisec) {
    if (!m_hWnd)
      return;
    ShowWindow(m_hWnd, SW_SHOWNA);
    SetTimer(m_hWnd, AUTOHIDE_TIMER, static_cast<UINT>(millisec),
             &PopupWindow::OnTimer);
    timer = UINT_PTR(this);
  }
  void SetHorizontal(bool horizontal) { m_horizontal = horizontal; }

private:
  void Create() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"PopupWindowClass";
    RegisterClass(&wc);
    m_hWnd = CreateWindowEx(WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
                            L"PopupWindowClass", L"PopupWindowPanel",
                            WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                            1000, 1000, nullptr, nullptr,
                            GetModuleHandle(nullptr), this);
    InitDirect2D();
    UpdateWindow(m_hWnd);
  }
  void GetTextSize(const wstring &text, LPSIZE lpSize) {
    lpSize->cx = 0;
    lpSize->cy = 0;
    if (!pTextFormat)
      return;
    ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
    HR(m_pWriteFactory->CreateTextLayout(text.c_str(), text.length(),
                                         pTextFormat.Get(), 1000.0f, 1000.0f,
                                         pTextLayout.GetAddressOf()));

    DWRITE_TEXT_METRICS textMetrics;
    HR(pTextLayout->GetMetrics(&textMetrics));
    D2D1_SIZE_F sz =
        D2D1::SizeF(ceil(textMetrics.widthIncludingTrailingWhitespace),
                    ceil(textMetrics.height));
    lpSize->cx = (int)sz.width;
    lpSize->cy = (int)sz.height;

    DWRITE_OVERHANG_METRICS overhangMetrics;
    HR(pTextLayout->GetOverhangMetrics(&overhangMetrics));
    if (overhangMetrics.left > 0)
      lpSize->cx += (LONG)(overhangMetrics.left);
    if (overhangMetrics.right > 0)
      lpSize->cx += (LONG)(overhangMetrics.right);
    if (overhangMetrics.top > 0)
      lpSize->cy += (LONG)(overhangMetrics.top);
    if (overhangMetrics.bottom > 0)
      lpSize->cy += (LONG)(overhangMetrics.bottom);
  }
  void FillRoundRect(const RECT &rect, float radius, uint32_t color) {
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
    HR(d2Factory->CreateRoundedRectangleGeometry(roundedRect,
                                                 &roundedRectGeometry));
    ComPtr<ID2D1SolidColorBrush> brush;
    HR(dc->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), &brush));
    dc->FillGeometry(roundedRectGeometry.Get(), brush.Get());
  }
  D2D1::ColorF D2d1ColorFromColorRef(uint32_t color) {
    float a = ((color >> 24) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float r = (color & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, a);
  }
  void Render() {
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF({0.0f, 0.0f, 0.0f, 0.0f}));
    FillRoundRect({0, 0, m_winSize.cx, m_winSize.cy}, 10.0f, 0xafffffff);
    if (m_horizontal) {
      DrawHorizontal();
    } else {
      DrawUIVertical();
    }
    HR(dc->EndDraw());
    // Make the swap chain available to the composition engine
    HR(swapChain->Present(1, 0)); // sync
  }

  void DrawTextAt(const wstring &text, size_t x, size_t y) {
    ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
    HR(m_pWriteFactory->CreateTextLayout(
        text.c_str(), text.length(), pTextFormat.Get(), m_winSize.cx,
        m_winSize.cy, pTextLayout.GetAddressOf()));
    dc->DrawTextLayout({(float)x, (float)y}, pTextLayout.Get(), m_pBrush.Get(),
                       D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
  }
  void ResizeVertical() {
    SIZE textSize{0, 0};
    SIZE winSize{0, 0};
    if (!m_ctx.preedit.empty()) {
      const auto &preedit = u8tow(m_ctx.preedit.str);
      GetTextSize(preedit, &textSize);
      winSize.cx += m_padding + textSize.cx;
      winSize.cy += m_padding + textSize.cy;
    } else if (!m_ctx.aux.empty()) {
      const auto &aux = u8tow(m_ctx.aux.str);
      GetTextSize(aux, &textSize);
      winSize.cx += m_padding + textSize.cx;
      winSize.cy += m_padding + textSize.cy;
    }
    size_t cand_width = MAX(winSize.cx, 0);
    if (m_ctx.cinfo.candies.size()) {
      const auto &cinfo = m_ctx.cinfo;
      for (auto i = 0; i < cinfo.candies.size(); i++) {
        const auto &label = u8tow(cinfo.labels[i].str);
        const auto &candie = u8tow(cinfo.candies[i].str);
        const auto &command = u8tow(cinfo.comments[i].str);
        size_t x = m_padding, y = winSize.cy + m_padding;
        size_t h = 0;
        GetTextSize(label, &textSize);
        h = MAX(h, textSize.cy);
        x += textSize.cx + m_padding;
        GetTextSize(candie, &textSize);
        h = MAX(h, textSize.cy);
        if (!command.empty()) {
          x += textSize.cx + m_padding;
          GetTextSize(command, &textSize);
        }
        cand_width = MAX(cand_width, x + textSize.cx);
        winSize.cy += h;
      }
      winSize.cy += m_padding;
    }
    winSize.cx = MAX(winSize.cx, cand_width);
    m_winSize.cx = cand_width + m_padding * 2;
    m_winSize.cy = winSize.cy + m_padding;

    SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_winSize.cx, m_winSize.cy,
                 SWP_NOMOVE | SWP_NOACTIVATE);
    D2D1_SIZE_U oPixelSize = {(UINT32)m_winSize.cx, (UINT32)m_winSize.cy};
    OutputDebugString(std::to_wstring(m_winSize.cx).c_str());
    OutputDebugString(std::to_wstring(m_winSize.cy).c_str());
    // m_pRenderTarget->Resize(&oPixelSize);
  }
  void ResizeHorizontal() {
    SIZE textSize{0, 0};
    SIZE winSize{0, 0};
    if (!m_ctx.preedit.empty()) {
      const auto &preedit = u8tow(m_ctx.preedit.str);
      GetTextSize(preedit, &textSize);
      winSize.cx = textSize.cx;
      winSize.cy = textSize.cy;
    } else if (!m_ctx.aux.empty()) {
      const auto &aux = u8tow(m_ctx.aux.str);
      GetTextSize(aux, &textSize);
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
        m_text += u8tow(cinfo.labels[i].str) + L". " +
                  u8tow(cinfo.candies[i].str) + L" " +
                  u8tow(cinfo.comments[i].str);
        m_text += highlighted ? L"] " : L" ";
      }
      GetTextSize(m_text, &textSize);
      winSize.cx = MAX(winSize.cx, textSize.cx);
      winSize.cy += textSize.cy;
    }
    m_winSize.cx = winSize.cx + m_padding * 2;
    m_winSize.cy = winSize.cy + m_padding * 2;

    SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_winSize.cx, m_winSize.cy,
                 SWP_NOMOVE | SWP_NOACTIVATE);
    D2D1_SIZE_U oPixelSize = {(UINT32)m_winSize.cx, (UINT32)m_winSize.cy};
    // m_pRenderTarget->Resize(&oPixelSize);
  }
  void DrawHorizontal() {
    SIZE textSize{0, 0};
    SIZE winSize{0, 0};
    if (!m_ctx.preedit.empty()) {
      const auto &preedit = u8tow(m_ctx.preedit.str);
      GetTextSize(preedit, &textSize);
      DrawTextAt(preedit, m_padding, m_padding);
      winSize.cx = textSize.cx;
      winSize.cy = textSize.cy;
    } else if (!m_ctx.aux.empty()) {
      const auto &aux = u8tow(m_ctx.aux.str);
      GetTextSize(aux, &textSize);
      DrawTextAt(aux, m_padding, m_padding);
      winSize.cx = textSize.cx;
      winSize.cy = textSize.cy;
    }
    if (m_ctx.cinfo.candies.size()) {
      winSize.cy += m_padding;
      DrawTextAt(m_text, m_padding, winSize.cy);
    }
  }
  void DrawUIVertical() {
    SIZE textSize{0, 0};
    SIZE winSize{0, 0};
    if (!m_ctx.preedit.empty()) {
      const auto &preedit = u8tow(m_ctx.preedit.str);
      GetTextSize(preedit, &textSize);
      DrawTextAt(preedit, m_padding, m_padding);
      winSize.cx += m_padding + textSize.cx;
      winSize.cy += m_padding + textSize.cy;
    } else if (!m_ctx.aux.empty()) {
      const auto &aux = u8tow(m_ctx.aux.str);
      DrawTextAt(aux, m_padding, m_padding);
      GetTextSize(aux, &textSize);
      winSize.cx += m_padding + textSize.cx;
      winSize.cy += m_padding + textSize.cy;
    }
    size_t cand_width = 0;
    if (m_ctx.cinfo.candies.size()) {
      const auto &cinfo = m_ctx.cinfo;
      for (auto i = 0; i < cinfo.candies.size(); i++) {
        const auto &label = u8tow(cinfo.labels[i].str);
        const auto &candie = u8tow(cinfo.candies[i].str);
        const auto &command = u8tow(cinfo.comments[i].str);
        if (i == cinfo.highlighted)
          m_pBrush->SetColor(D2d1ColorFromColorRef(0xff9e5a00));
        else
          m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
        LONG xl, yl, xt, yt, xc, yc;
        size_t x = m_padding, y = winSize.cy + m_padding;
        size_t h = 0;
        GetTextSize(label, &textSize);
        xl = x;
        yl = y;
        // DrawTextAt(label, x, y);
        h = MAX(h, textSize.cy);
        x += textSize.cx + m_padding;

        GetTextSize(candie, &textSize);
        h = MAX(h, textSize.cy);
        xt = x;
        yt = y;
        // DrawTextAt(candie, x, y);

        if (!command.empty()) {
          x += textSize.cx + m_padding;
          GetTextSize(command, &textSize);
          xc = x;
          yc = y;
          // DrawTextAt(command, x, y);
        } else {
          xc = xt + textSize.cx;
          yc = y;
        }
        cand_width = MAX(cand_width, x + textSize.cx);

        RECT rc = {xl, yl, xl + (LONG)cand_width, yl + (LONG)h};
        if (i == cinfo.highlighted)
          FillRoundRect(rc, m_padding, 0xdff0f0e0);
        DrawTextAt(label, xl, yl);
        DrawTextAt(candie, xt, yt);
        if (!command.empty())
          DrawTextAt(command, xc, yc);
        winSize.cy += h;
      }
      winSize.cy += m_padding;
      m_winSize.cx = cand_width;
      m_winSize.cy = winSize.cy;
    }
  }
  void InitDirect2D() {
    HR(D3D11CreateDevice(nullptr, // Adapter
                         D3D_DRIVER_TYPE_HARDWARE,
                         nullptr, // Module
                         D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                         0, // Highest available feature level
                         D3D11_SDK_VERSION, &direct3dDevice,
                         nullptr,   // Actual feature level
                         nullptr)); // Device context
    HR(direct3dDevice.As(&dxgiDevice));
    HR(CreateDXGIFactory2(0, __uuidof(dxFactory.Get()),
                          reinterpret_cast<void **>(dxFactory.GetAddressOf())));
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
    HR(dxFactory->CreateSwapChainForComposition(dxgiDevice.Get(), &description,
                                                nullptr, // Don抰 restrict
                                                swapChain.GetAddressOf()));
    // Create a single-threaded Direct2D factory with debugging information
    D2D1_FACTORY_OPTIONS const options = {D2D1_DEBUG_LEVEL_INFORMATION};
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
                         d2Factory.GetAddressOf()));
    // Create the Direct2D device that links back to the Direct3D device
    HR(d2Factory->CreateDevice(dxgiDevice.Get(), d2Device.GetAddressOf()));
    // Create the Direct2D device context that is the actual render target
    // and exposes drawing commands
    HR(d2Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                     dc.GetAddressOf()));
    // Retrieve the swap chain's back buffer
    HR(swapChain->GetBuffer(0, // index
                            __uuidof(surface.Get()),
                            reinterpret_cast<void **>(surface.GetAddressOf())));
    // Create a Direct2D bitmap that points to the swap chain surface
    D2D1_BITMAP_PROPERTIES1 properties = {};
    properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    properties.bitmapOptions =
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    HR(dc->CreateBitmapFromDxgiSurface(surface.Get(), properties,
                                       bitmap.GetAddressOf()));
    // Point the device context to the bitmap for rendering
    dc->SetTarget(bitmap.Get());
    HR(DCompositionCreateDevice(
        dxgiDevice.Get(), __uuidof(dcompDevice.Get()),
        reinterpret_cast<void **>(dcompDevice.GetAddressOf())));
    HR(dcompDevice->CreateTargetForHwnd(m_hWnd,
                                        true, // Top most
                                        target.GetAddressOf()));
    HR(dcompDevice->CreateVisual(visual.GetAddressOf()));
    HR(visual->SetContent(swapChain.Get()));
    HR(target->SetRoot(visual.Get()));
    HR(dcompDevice->Commit());
    D2D1_COLOR_F const brushColor = D2D1::ColorF(0.18f,  // red
                                                 0.55f,  // green
                                                 0.34f,  // blue
                                                 0.75f); // alpha
    HR(dc->CreateSolidColorBrush(brushColor, brush.GetAddressOf()));
    // create dwrite objs
    HR(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown **>(m_pWriteFactory.GetAddressOf())));
    HR(m_pWriteFactory->CreateTextFormat(
        L"Microsoft Yahei", NULL, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        m_fontPoint * m_dpiScale, L"",
        reinterpret_cast<IDWriteTextFormat **>(pTextFormat.GetAddressOf())));
    ComPtr<IDWriteFontFallback> pSysFallback;
    ComPtr<IDWriteFontFallback> pFontFallback = NULL;
    ComPtr<IDWriteFontFallbackBuilder> pFontFallbackBuilder = NULL;
    HR(m_pWriteFactory->GetSystemFontFallback(pSysFallback.GetAddressOf()));
    HR(m_pWriteFactory->CreateFontFallbackBuilder(
        pFontFallbackBuilder.GetAddressOf()));
    HR(pFontFallbackBuilder->AddMappings(pSysFallback.Get()));
    HR(pFontFallbackBuilder->CreateFontFallback(pFontFallback.GetAddressOf()));
    HR(pTextFormat->SetFontFallback(pFontFallback.Get()));
    HR(dc->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                 m_pBrush.GetAddressOf()));
  }
  void OnPaint() {
    if (m_horizontal) {
      ResizeHorizontal();
    } else {
      ResizeVertical();
    }
    Render();
  }
  void UpdateDpi() {
    HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    UINT dpix = 0, dpiy = 0;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy)))
      m_dpiScale = dpiy / 72.0f;
  }
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam) {
    PopupWindow *self;
    if (uMsg == WM_NCCREATE) {
      self = static_cast<PopupWindow *>(
          reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<PopupWindow *>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
      PostQuitMessage(0);
      return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  static VOID CALLBACK OnTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent, _In_ DWORD dwTime) {
    KillTimer(hwnd, idEvent);
    PopupWindow *self = (PopupWindow *)timer;
    timer = 0;
    if (self) {
      self->Hide();
    }
  }

  static const int AUTOHIDE_TIMER = 20241031;
  static UINT_PTR timer;

  HWND m_hWnd;
  SIZE m_winSize{0, 0};
  RECT m_inputPos{0, 0};
  wstring m_text = L"Hello, Direct2D with Emoji 😀😁!\n换行显示中文🍅🍇";
  wstring m_aux;
  float m_dpiScale = 96.0f / 72.0f;
  size_t m_fontPoint = 20;
  Context m_ctx;
  Status m_status;
  UINT m_padding = 10;
  bool m_horizontal = true;
  // ------------------------------------------------------------
  ComPtr<ID3D11Device> direct3dDevice;
  ComPtr<IDXGIDevice> dxgiDevice;
  ComPtr<IDXGIFactory2> dxFactory;
  ComPtr<IDXGISwapChain1> swapChain;
  ComPtr<ID2D1Factory2> d2Factory;
  ComPtr<ID2D1Device1> d2Device;
  ComPtr<ID2D1DeviceContext> dc;
  ComPtr<IDXGISurface2> surface;
  ComPtr<ID2D1Bitmap1> bitmap;
  ComPtr<IDCompositionDevice> dcompDevice;
  ComPtr<IDCompositionTarget> target;
  ComPtr<IDCompositionVisual> visual;
  ComPtr<ID2D1SolidColorBrush> brush;
  // ------------------------------------------------------------
  // ComPtr<ID2D1Factory> m_pFactory;
  // ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
  ComPtr<IDWriteTextFormat1> pTextFormat;
  ComPtr<IDWriteFactory2> m_pWriteFactory;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
};
UINT_PTR PopupWindow::timer = 0;
} // namespace
#endif
