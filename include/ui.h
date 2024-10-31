#pragma once
#ifndef _UI_H_
#define _UI_H_
#include <data.h>
#include <string>
#include <windows.h>
class PopupWindow {
public:
  PopupWindow(HINSTANCE hInstance, const wchar_t *className,
              const wchar_t *windowTitle);
  ~PopupWindow();
  HWND CreatePopup();
  void Show();
  void Hide();
  void UpdatePos(int x, int y);
  bool IsVisible() const;
  void ShowWithTimeout(size_t millisec);
  void SetText(const std::wstring &text,
               const std::wstring &aux = L""); // 新增方法
  void Refresh();
  void Update(const Context &ctx, const Status &sta);

private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  static VOID CALLBACK OnTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent, _In_ DWORD dwTime);
  void RegisterWindowClass();
  bool IsShown() const { return shown; }
  void OnPaint();
  void OnCreate();
  void OnDestroy();
  void DrawUI(HDC hdc);

  HFONT hFont;
  static const int AUTOHIDE_TIMER = 20241031;
  static UINT_PTR timer;

  bool shown;
  HINSTANCE hInstance_;
  const wchar_t *className_;
  const wchar_t *windowTitle_;
  HWND hwnd_;
  std::wstring text_; // 用于存储文本
  std::wstring aux_;  // 用于存储文本
  Context m_ctx;
  Status m_status;
};

UINT_PTR PopupWindow::timer = 0;

void PopupWindow::Update(const Context &ctx, const Status &sta) {
  m_ctx = ctx;
  m_status = sta;
  Refresh();
}
void PopupWindow::Refresh() {
  if (!hwnd_)
    return;
  if (timer) {
    Hide();
    KillTimer(hwnd_, AUTOHIDE_TIMER);
  }
  if (hwnd_) {
    InvalidateRect(hwnd_, NULL, TRUE); // 请求重绘
  }
}
void PopupWindow::ShowWithTimeout(size_t millisec) {
  if (!hwnd_)
    return;
  ShowWindow(hwnd_, SW_SHOWNA);
  shown = true;
  SetTimer(hwnd_, AUTOHIDE_TIMER, static_cast<UINT>(millisec),
           &PopupWindow::OnTimer);
  timer = UINT_PTR(this);
}
VOID CALLBACK PopupWindow::OnTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                                   _In_ UINT_PTR idEvent, _In_ DWORD dwTime) {
  KillTimer(hwnd, idEvent);
  PopupWindow *self = (PopupWindow *)timer;
  timer = 0;
  if (self) {
    self->Hide();
    self->shown = false;
  }
}
// Constructor to initialize the instance, class name, and window title
PopupWindow::PopupWindow(HINSTANCE hInstance, const wchar_t *className,
                         const wchar_t *windowTitle)
    : hInstance_(hInstance), className_(className), windowTitle_(windowTitle),
      hwnd_(NULL), shown(false) {
  RegisterWindowClass();
  CreatePopup();
  SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}
// Destructor to clean up resources
PopupWindow::~PopupWindow() {
  Hide(); // Ensure the window is hidden before destruction
  UnregisterClass(className_, hInstance_);
}
// Register window class
void PopupWindow::RegisterWindowClass() {
  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance_;
  wc.lpszClassName = className_;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClass(&wc);
}
// Method to create the popup window
HWND PopupWindow::CreatePopup() {
  hwnd_ =
      CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE, // Extended window styles
                     className_,                       // Window class name
                     windowTitle_,                     // Window title
                     WS_POPUP | WS_BORDER,             // Window styles
                     CW_USEDEFAULT, CW_USEDEFAULT,     // Position
                     10, 10,                           // Size
                     NULL,                             // Parent window
                     NULL,                             // Menu
                     hInstance_,                       // Instance handle
                     NULL // Additional application data
      );

  return hwnd_;
}
// Show the popup window
void PopupWindow::Show() {
  if (hwnd_ && !shown) {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);
    shown = true;
  }
}
// Hide the popup window
void PopupWindow::Hide() {
  if (hwnd_ && shown) {
    ShowWindow(hwnd_, SW_HIDE);
    shown = false;
  }
}
// Update the position of the popup window
void PopupWindow::UpdatePos(int x, int y) {
  if (hwnd_) {
    RECT rc;
    GetWindowRect(hwnd_, &rc);
    auto h = rc.bottom - rc.top;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y - h, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);
  }
}
// Check if the popup window is currently visible
bool PopupWindow::IsVisible() const { return shown; }
void PopupWindow::OnCreate() {
  hFont = CreateFont(24, // 字体高度
                     0, 0, 0,
                     FW_NORMAL, // 字体粗细
                     FALSE, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                     L"Microsoft Yahei" // 字体名称
  );
}
void PopupWindow::OnDestroy() {}

void PopupWindow::DrawUI(HDC hdc) {
  SIZE textSize{0, 0};
  SIZE winSize{0, 0};
  if (!m_ctx.preedit.empty()) {
    const auto &preedit = u8tow(m_ctx.preedit.str);
    GetTextExtentPoint32(hdc, preedit.c_str(), preedit.length(), &textSize);
    TextOut(hdc, 10, 10, preedit.c_str(), preedit.length());
    winSize.cx += 10 + textSize.cx;
    winSize.cy += 10 + textSize.cy;
  } else if (!m_ctx.aux.empty()) {
    const auto &aux = u8tow(m_ctx.aux.str);
    GetTextExtentPoint32(hdc, aux.c_str(), aux.length(), &textSize);
    TextOut(hdc, 10, 10, aux.c_str(), aux.length());
    winSize.cx += 10 + textSize.cx;
    winSize.cy += 10 + textSize.cy;
  }

  size_t cand_width = 0;
  if (m_ctx.cinfo.candies.size()) {
    const auto &cinfo = m_ctx.cinfo;
    for (auto i = 0; i < cinfo.candies.size(); i++) {
      const auto &label = u8tow(cinfo.labels[i].str);
      const auto &candie = u8tow(cinfo.candies[i].str);
      const auto &command = u8tow(cinfo.comments[i].str);
      if (i == cinfo.highlighted)
        SetTextColor(hdc, RGB(255, 0, 0));
      else
        SetTextColor(hdc, RGB(0, 0, 0));
      size_t x = 10, y = winSize.cy + 10;
      size_t h = 0;
      GetTextExtentPoint32(hdc, label.c_str(), label.length(), &textSize);
      TextOut(hdc, x, y, label.c_str(), label.length());
      h = MAX(h, textSize.cy);
      x += textSize.cx + 10;

      GetTextExtentPoint32(hdc, candie.c_str(), candie.length(), &textSize);
      h = MAX(h, textSize.cy);
      TextOut(hdc, x, y, candie.c_str(), candie.length());

      if (!command.empty()) {
        x += textSize.cx + 10;
        GetTextExtentPoint32(hdc, command.c_str(), command.length(), &textSize);
        TextOut(hdc, x, y, command.c_str(), command.length());
      }
      cand_width = MAX(cand_width, x + textSize.cx);
      winSize.cy += h;
    }
    winSize.cy += 10;
  }

  winSize.cx = MAX(winSize.cx, cand_width);
  OutputDebugStringA(
      (std::to_string(winSize.cx) + " " + std::to_string(winSize.cy)).c_str());
  SetWindowPos(hwnd_, nullptr, 0, 0, winSize.cx + 20, winSize.cy + 10,
               SWP_NOMOVE | SWP_NOZORDER);
}

void PopupWindow::OnPaint() {
  PAINTSTRUCT ps;
  hFont = CreateFont(24, // 字体高度
                     0, 0, 0,
                     FW_NORMAL, // 字体粗细
                     FALSE, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                     L"Microsoft Yahei" // 字体名称
  );
  HDC hdc = BeginPaint(hwnd_, &ps);
  RECT rc;
  GetClientRect(hwnd_, &rc);
  FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
  HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
#if 0
  SIZE textSize;
  auto text = (aux_.empty()) ? text_ : aux_;
  GetTextExtentPoint32(hdc, text.c_str(), text.length(), &textSize);
  SetWindowPos(hwnd_, nullptr, 0, 0, textSize.cx + 20, textSize.cy + 20,
               SWP_NOMOVE | SWP_NOZORDER);
  TextOut(hdc, 10, 10, text.c_str(), text.length());
#endif
  DrawUI(hdc);
  EndPaint(hwnd_, &ps);
  DeleteObject(hFont); // 删除创建的字体
}
// Window procedure to handle messages
LRESULT CALLBACK PopupWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) {
  PopupWindow *pThis =
      reinterpret_cast<PopupWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (pThis) {
    switch (uMsg) {
    case WM_CREATE:
      pThis->OnCreate();
      return 0;
    case WM_PAINT: {
      pThis->OnPaint();
      return 0;
    }
    case WM_DESTROY:
      pThis->OnDestroy();
      return 0;
    }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
// Set the text to be displayed in the popup window
void PopupWindow::SetText(const std::wstring &text, const std::wstring &aux) {
  text_ = text;
  aux_ = aux;
  m_ctx.clear();
  m_ctx.aux.str = wtou8(aux);
  if (hwnd_) {
    InvalidateRect(hwnd_, NULL, TRUE); // 请求重绘
  }
}

#endif
