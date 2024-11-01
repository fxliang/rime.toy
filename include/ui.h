#pragma once
#ifndef _UI_H_
#define _UI_H_
#include <d2d1.h>
#include <data.h>
#include <dwrite.h>
#include <math.h>
#include <shellscalingapi.h>
#include <string>
#include <windows.h>
#include <wrl.h>
namespace {
using namespace Microsoft::WRL;
using wstring = std::wstring;
using string = std::string;
class PopupWindow {
public:
  PopupWindow()
      : m_hWnd(nullptr), m_pFactory(nullptr), m_pRenderTarget(nullptr),
        m_pBrush(nullptr), m_pWriteFactory(nullptr), pTextFormat(nullptr) {
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
    m_hWnd = CreateWindowEx(WS_EX_NOACTIVATE, L"PopupWindowClass",
                            L"PopupWindowPanel", WS_POPUP | WS_VISIBLE,
                            CW_USEDEFAULT, CW_USEDEFAULT, 10, 10, nullptr,
                            nullptr, GetModuleHandle(nullptr), this);
    InitializeDirect2D();
    UpdateWindow(m_hWnd);
  }
  void InitializeDirect2D() {
    HRESULT hr;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 m_pFactory.GetAddressOf())))
      return;
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    auto w = (UINT32)(rc.right - rc.left);
    auto h = (UINT32)(rc.bottom - rc.top);
    D2D1_SIZE_U oPixelSize = {w, h};
    D2D1_RENDER_TARGET_PROPERTIES oRenderTargetProperties =
        D2D1::RenderTargetProperties();
    D2D1_HWND_RENDER_TARGET_PROPERTIES oHwndRenderTargetProperties =
        D2D1::HwndRenderTargetProperties(m_hWnd, oPixelSize);
    if (FAILED(m_pFactory->CreateHwndRenderTarget(
            oRenderTargetProperties, oHwndRenderTargetProperties,
            m_pRenderTarget.GetAddressOf())))
      return;
    m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    if (FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown **>(m_pWriteFactory.GetAddressOf()))))
      return;
    if (FAILED(m_pWriteFactory->CreateTextFormat(
            L"Microsoft Yahei", NULL, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            m_fontPoint * m_dpiScale, L"", pTextFormat.GetAddressOf())))
      return;
    if (FAILED(m_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black), m_pBrush.GetAddressOf())))
      return;
  }
  void GetTextSize(const wstring &text, LPSIZE lpSize) {
    lpSize->cx = 0;
    lpSize->cy = 0;
    if (!pTextFormat)
      return;
    ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
    HRESULT hr = m_pWriteFactory->CreateTextLayout(
        text.c_str(), text.length(), pTextFormat.Get(), 1000.0f, 1000.0f,
        pTextLayout.GetAddressOf());
    if (FAILED(hr))
      return;

    DWRITE_TEXT_METRICS textMetrics;
    hr = pTextLayout->GetMetrics(&textMetrics);
    if (FAILED(hr))
      return;
    D2D1_SIZE_F sz =
        D2D1::SizeF(ceil(textMetrics.widthIncludingTrailingWhitespace),
                    ceil(textMetrics.height));
    lpSize->cx = (int)sz.width;
    lpSize->cy = (int)sz.height;

    DWRITE_OVERHANG_METRICS overhangMetrics;
    hr = pTextLayout->GetOverhangMetrics(&overhangMetrics);
    if (SUCCEEDED(hr)) {
      if (overhangMetrics.left > 0)
        lpSize->cx += (LONG)(overhangMetrics.left);
      if (overhangMetrics.right > 0)
        lpSize->cx += (LONG)(overhangMetrics.right);
      if (overhangMetrics.top > 0)
        lpSize->cy += (LONG)(overhangMetrics.top);
      if (overhangMetrics.bottom > 0)
        lpSize->cy += (LONG)(overhangMetrics.bottom);
    }
  }
  void Resize() {
    GetTextSize(m_text, &m_winSize);
    m_winSize.cx += m_padding * 2;
    m_winSize.cy += m_padding * 2;
    SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_winSize.cx, m_winSize.cy,
                 SWP_NOMOVE | SWP_NOACTIVATE);
    D2D1_SIZE_U oPixelSize = {(UINT32)m_winSize.cx, (UINT32)m_winSize.cy};
    m_pRenderTarget->Resize(&oPixelSize);
  }

  void Render() {
    m_pRenderTarget->BeginDraw();
    if (m_horizontal) {
      ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
      if (FAILED(m_pWriteFactory->CreateTextLayout(
              m_text.c_str(), m_text.length(), pTextFormat.Get(), m_winSize.cx,
              m_winSize.cy, pTextLayout.GetAddressOf())))
        return;
      m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
      m_pRenderTarget->DrawTextLayout({(float)m_padding, (float)m_padding},
                                      pTextLayout.Get(), m_pBrush.Get(),
                                      D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    } else {

      m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
      DrawUIVertical();
    }
    m_pRenderTarget->EndDraw();
  }
  void DrawTextAt(const wstring &text, size_t x, size_t y) {
    ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
    if (FAILED(m_pWriteFactory->CreateTextLayout(
            text.c_str(), text.length(), pTextFormat.Get(), m_winSize.cx,
            m_winSize.cy, pTextLayout.GetAddressOf())))
      return;
    m_pRenderTarget->DrawTextLayout({(float)x, (float)y}, pTextLayout.Get(),
                                    m_pBrush.Get(),
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
    m_winSize.cy = winSize.cy + m_padding * 2;

    SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_winSize.cx, m_winSize.cy,
                 SWP_NOMOVE | SWP_NOACTIVATE);
    D2D1_SIZE_U oPixelSize = {(UINT32)m_winSize.cx, (UINT32)m_winSize.cy};
    m_pRenderTarget->Resize(&oPixelSize);
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
          m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
        else
          m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
        size_t x = m_padding, y = winSize.cy + m_padding;
        size_t h = 0;
        GetTextSize(label, &textSize);
        DrawTextAt(label, x, y);
        h = MAX(h, textSize.cy);
        x += textSize.cx + m_padding;

        GetTextSize(candie, &textSize);
        h = MAX(h, textSize.cy);
        DrawTextAt(candie, x, y);

        if (!command.empty()) {
          x += textSize.cx + m_padding;
          GetTextSize(command, &textSize);
          DrawTextAt(command, x, y);
        }
        cand_width = MAX(cand_width, x + textSize.cx);
        winSize.cy += h;
      }
      winSize.cy += m_padding;
      m_winSize.cx = cand_width;
      m_winSize.cy = winSize.cy;
    }
  }

  void OnPaint() {
    if (m_horizontal) {
      m_text = u8tow(m_ctx.preedit.str) + L" ";
      for (auto i = 0; i < m_ctx.cinfo.candies.size(); i++) {
        bool highlighted = i == m_ctx.cinfo.highlighted;
        m_text += highlighted ? L"[" : L"";
        m_text += u8tow(m_ctx.cinfo.labels[i].str) + L". " +
                  u8tow(m_ctx.cinfo.candies[i].str) + L" " +
                  u8tow(m_ctx.cinfo.comments[i].str);
        m_text += highlighted ? L"] " : L" ";
      }
      Resize();
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

  ComPtr<ID2D1Factory> m_pFactory;
  ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
  ComPtr<IDWriteTextFormat> pTextFormat;
  ComPtr<IDWriteFactory> m_pWriteFactory;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
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
};
UINT_PTR PopupWindow::timer = 0;
} // namespace
#endif
