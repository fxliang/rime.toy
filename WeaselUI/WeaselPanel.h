#include <WeaselUI.h>

#include "Layout.h"
#include "d2d.h"
#include <utils.h>

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
  bool GetIsReposition() { return m_istorepos; }

  HWND m_hWnd;

  static void CALLBACK OnClickTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                                    _In_ UINT_PTR idEvent, _In_ DWORD dwTime);
  static UINT_PTR ptimer;
  static const int AUTOREV_TIMER = 20241209;

private:
  void RedrawWindow() { InvalidateRect(m_hWnd, nullptr, true); }
  void _CreateLayout();
  bool _DrawPreedit(const Text &text, CRect &rc);
  bool _DrawCandidates();
  void _ResizeWindow();
  void _Reposition();
  void _TextOut(CRect &rc, const wstring &text, size_t cch, uint32_t color,
                ComPtr<IDWriteTextFormat1> &pTextFormat);
  void _HighlightRect(const RECT &rect, float radius, uint32_t border,
                      uint32_t back_color, uint32_t shadow_color,
                      uint32_t border_color, IsToRoundStruct roundInfo);
  CRect _GetInflatedCandRect(int i);
  void _CaptureRect(CRect &rect);

  void DoPaint();
  void OnDestroy();
  HRESULT OnScroll(UINT uMsg, WPARAM wParam, LPARAM lParam);
  LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam);
  LRESULT OnMouseActive(UINT uMsg, WPARAM wParam, LPARAM lParam);
  LRESULT OnLeftClickUp(UINT uMsg, WPARAM wParam, LPARAM lParam);
  LRESULT OnLeftClickDown(UINT uMsg, WPARAM wParam, LPARAM lParam);

  LRESULT MsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);

  RECT m_inputPos{0, 0, 0, 0};

  Context &m_ctx;
  Context &m_octx;
  Status &m_status;
  UIStyle &m_style;
  UIStyle &m_ostyle;

  int m_candidateCount;
  int m_hoverIndex = -1;
  bool hide_candidates;

  int m_offsetys[MAX_CANDIDATES_COUNT]; // offset y for candidates when
                                        // vertical layout over bottom
  int m_offsety_preedit = 0;
  int m_offsety_aux = 0;
  bool m_istorepos = false;
  bool m_sticky = false;
  float m_bar_scale = 1.0f;
  // ------------------------------------------------------------
  an<D2D> m_pD2D;
  an<Layout> m_layout;
  HICON m_iconAlpha;
  HICON m_iconEnabled;
  HICON m_iconFull;
  HICON m_iconHalf;
  HICON m_iconDisabled;
  wstring m_current_zhung_icon;
  wstring m_current_ascii_icon;
  wstring m_current_half_icon;
  wstring m_current_full_icon;
  UICallbackFunc &m_uiCallback;
};
} // namespace weasel
