#pragma once
#ifndef D2D_H
#define D2D_H

#include <WeaselIPCData.h>
#include <d2d1.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <d3d11_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dwrite_2.h>
#include <dxgi1_3.h>
#include <utils.h>

namespace weasel {

struct D2D {

  D2D(UIStyle &style, HWND hwnd);
  void InitDirect2D();
  void InitDirectWriteResources();
  void InitDpiInfo();
  void InitFontFormats();
  void GetTextSize(const wstring &text, size_t nCount,
                   ComPtr<IDWriteTextFormat1> pTextFormat, LPSIZE lpSize);
  void _SetFontFallback(ComPtr<IDWriteTextFormat1> textFormat,
                        const std::vector<std::wstring> &fontVector);
  void _ParseFontFace(const std::wstring &fontFaceStr,
                      DWRITE_FONT_WEIGHT &fontWeight,
                      DWRITE_FONT_STYLE &fontStyle);
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
  ComPtr<IDWriteTextFormat1> pTextFormat;
  ComPtr<IDWriteTextFormat1> pLabelFormat;
  ComPtr<IDWriteTextFormat1> pCommentFormat;
  ComPtr<IDWriteFactory2> m_pWriteFactory;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
  UIStyle &m_style;
  HWND m_hWnd;
  float m_dpiX;
  float m_dpiY;
  float m_dpiScaleFontPoint;
};
} // namespace weasel
#endif
