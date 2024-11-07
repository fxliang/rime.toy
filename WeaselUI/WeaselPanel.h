#include <WeaselUI.h>

#include <d2d1.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <d3d11_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dwrite_2.h>
#include <dxgi1_3.h>

namespace weasel {
class WeaselPanel {
public:
  WeaselPanel(UI &ui);
  ~WeaselPanel() {}
  void MoveTo(RECT rc);
  void Refresh();

  BOOL IsWindow() const;
  void ShowWindow(int nCmdShow);
  void DestroyWindow();
  BOOL Create(HWND parent);

  HWND m_hWnd;

private:
  void GetTextSize(const wstring &text, LPSIZE lpSize);
  void FillRoundRect(const RECT &rect, float radius, uint32_t color);
  D2D1::ColorF D2d1ColorFromColorRef(uint32_t color);
  void Render();
  void DrawTextAt(const wstring &text, size_t x, size_t y);
  void ResizeVertical();
  void ResizeHorizontal();
  void DrawHorizontal();
  void DrawUIVertical();
  void InitDirect2D();
  void OnPaint();
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);

  wstring m_text;
  SIZE m_winSize{0, 0};
  RECT m_inputPos{0, 0, 0, 0};
  float m_dpiScale = 96.0f / 72.0f;

  Context &m_ctx;
  Status &m_status;
  UIStyle &m_style;
  UIStyle &m_ostyle;
  UINT m_padding = 10;
  BOOL &m_horizontal;
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
  ComPtr<IDWriteTextFormat1> pTextFormat;
  ComPtr<IDWriteFactory2> m_pWriteFactory;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
};
} // namespace weasel
