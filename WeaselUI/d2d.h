#pragma once
#ifndef D2D_H
#define D2D_H

#include <BaseTypes.h>
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

typedef ComPtr<IDWriteTextFormat1> PtTextFormat;
struct IsToRoundStruct {
  bool IsTopLeftNeedToRound;
  bool IsBottomLeftNeedToRound;
  bool IsTopRightNeedToRound;
  bool IsBottomRightNeedToRound;
  bool Hemispherical;
  IsToRoundStruct()
      : IsTopLeftNeedToRound(true), IsTopRightNeedToRound(true),
        IsBottomLeftNeedToRound(true), IsBottomRightNeedToRound(true),
        Hemispherical(false) {}
};

struct D2D {

  D2D(UIStyle &style, HWND hwnd);
  ~D2D();
  void InitDirect2D();
  void InitDirectWriteResources();
  void InitDpiInfo();
  void InitFontFormats();
  void InitFontFormats(const wstring &label_font_face,
                       const int label_font_point, const wstring &font_face,
                       const int font_point, const wstring &comment_font_face,
                       const int comment_font_point);
  void OnResize(UINT width, UINT height);
  void SetBrushColor(uint32_t color);
  void GetTextSize(const wstring &text, size_t nCount,
                   PtTextFormat &pTextFormat, LPSIZE lpSize);
  void _SetFontFallback(PtTextFormat textFormat,
                        const std::vector<std::wstring> &fontVector);
  void _ParseFontFace(const std::wstring &fontFaceStr,
                      DWRITE_FONT_WEIGHT &fontWeight,
                      DWRITE_FONT_STYLE &fontStyle);
  HRESULT GetBmpFromIcon(HICON hIcon, ComPtr<ID2D1Bitmap1> &pBitmap);
  HRESULT GetIconFromFile(const wstring &iconPath,
                          ComPtr<ID2D1Bitmap1> &pD2DBitmap);

  HRESULT CreateRoundedRectanglePath(const RECT &rc, float radius,
                                     const IsToRoundStruct &roundInfo,
                                     ComPtr<ID2D1PathGeometry> &pPathGeometry);
  HRESULT FillGeometry(const CRect &rect, uint32_t color, uint32_t radius,
                       IsToRoundStruct roundInfo, bool to_blur = false);
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
  PtTextFormat pPreeditFormat;
  PtTextFormat pTextFormat;
  PtTextFormat pLabelFormat;
  PtTextFormat pCommentFormat;
  ComPtr<IDWriteFactory2> m_pWriteFactory;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
  UIStyle &m_style;
  HWND m_hWnd;
  float m_dpiX;
  float m_dpiY;
  float m_dpiScaleFontPoint;
  float m_dpiScaleLayout;
};
} // namespace weasel
#endif
