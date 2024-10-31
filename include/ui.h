#pragma once
#ifndef _UI_H_
#define _UI_H_
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

private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  void RegisterWindowClass();
  bool IsShown() const { return shown; }

  static VOID CALLBACK OnTimer(_In_ HWND hwnd, _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent, _In_ DWORD dwTime);
  static const int AUTOHIDE_TIMER = 20121220;
  static UINT_PTR timer;

  bool shown;
  HINSTANCE hInstance_;
  const wchar_t *className_;
  const wchar_t *windowTitle_;
  HWND hwnd_;
  static std::wstring text_; // 用于存储文本
  static std::wstring aux_;  // 用于存储文本
};

UINT_PTR PopupWindow::timer = 0;
std::wstring PopupWindow::text_ = L""; // 用于存储文本
std::wstring PopupWindow::aux_ = L"";  // 用于存储文本

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
                     500, 200,                         // Size
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
// Window procedure to handle messages
LRESULT CALLBACK PopupWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) {
  static HFONT hFont;
  switch (uMsg) {
  case WM_CREATE:
    hFont = CreateFont(24, // 字体高度
                       0, 0, 0,
                       FW_NORMAL, // 字体粗细
                       FALSE, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       L"Microsoft Yahei" // 字体名称
    );
    return 0;
  case WM_PAINT: {
    // OutputDebugString(text_.c_str());
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    SIZE textSize;
    auto text = (aux_.empty()) ? text_ : aux_;
    GetTextExtentPoint32(hdc, text.c_str(), text.length(), &textSize);
    SetWindowPos(hwnd, nullptr, 0, 0, textSize.cx + 20, textSize.cy + 20,
                 SWP_NOMOVE | SWP_NOZORDER);
    TextOut(hdc, 10, 10, text.c_str(), text.length());
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_DESTROY:
    DeleteObject(hFont); // 删除创建的字体
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
// Set the text to be displayed in the popup window
void PopupWindow::SetText(const std::wstring &text, const std::wstring &aux) {
  text_ = text;
  aux_ = aux;
  if (hwnd_) {
    InvalidateRect(hwnd_, NULL, TRUE); // 请求重绘
  }
}

#endif
