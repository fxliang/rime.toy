// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "RimeWithToy.h"
#include "keymodule.h"
#include "trayicon.h"
#include <ShellScalingApi.h>
#include <WeaselIPCData.h>
#include <WeaselUI.h>
#include <iostream>
#include <resource.h>
#include <sstream>

using namespace std;
using namespace weasel;

HHOOK hHook = NULL;
Status status;
bool committed = true;
wstring commit_str = L"";
bool horizontal = false, escape_ansi = false;
HICON ascii_icon;
HICON ime_icon;

std::shared_ptr<UI> ui;
std::shared_ptr<TrayIcon> trayIcon;
std::shared_ptr<RimeWithToy> toy;

// ----------------------------------------------------------------------------

void send_input_to_window(HWND hwnd, const wstring &text) {
  for (const auto &ch : text) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = ch;
    input.ki.dwFlags = KEYEVENTF_UNICODE;
    input.ki.time = 0;
    input.ki.dwExtraInfo = GetMessageExtraInfo();
    INPUT inputRelease = input;
    inputRelease.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
    SendInput(1, &inputRelease, sizeof(INPUT));
  }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  const int TOGGLE_KEY = 0x31; // key '1'
  static KeyEvent prevKeyEvent;
  static BOOL prevfEaten = FALSE;
  static int keyCountToSimulate = 0;
  static HWND hwnd_previous = nullptr;

  HWND hwnd = GetForegroundWindow();
  RECT rect;
  if (hwnd)
    GetWindowRect(hwnd, &rect);
  if (hwnd != hwnd_previous) {
    hwnd_previous = hwnd;
    ui->ctx().clear();
    ui->Hide();
  }
  if (nCode == HC_ACTION) {
    // update keyState table
    update_keystates(wParam, lParam);

    KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
    // get KBDLLHOOKSTRUCT info, generate keyinfo
    KeyInfo ki = parse_key(wParam, lParam);
    KeyEvent ke;
    if (ConvertKeyEvent(pKeyboard, ki, ke)) {
      bool eat = false;
      if (!keyCountToSimulate)
        eat = toy->ProcessKeyEvent(ke, commit_str);
      // make capslock bindable for rime
      if (ke.keycode == ibus::Caps_Lock) {
        if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
            (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
          if (GetKeyState(VK_CAPITAL) & 0x01) {
            if (committed || (!eat && status.composing)) {
              keyCountToSimulate = 2;
              INPUT inputs[2];
              inputs[0].type = INPUT_KEYBOARD;
              inputs[0].ki = {VK_CAPITAL, 0, 0, 0, 0};
              inputs[1].type = INPUT_KEYBOARD;
              inputs[1].ki = {VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0, 0};
              ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs,
                          sizeof(INPUT));
            }
          }
          eat = TRUE;
        }
        if (keyCountToSimulate)
          keyCountToSimulate--;
      }
      // make capslock bindable for rime ends
      prevfEaten = eat;
      prevKeyEvent = ke;

      toy->UpdateUI();
      status = ui->status();

      // update position
      POINT pt;
      if (GetCursorPos(&pt)) {
        ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
      } else {
        pt.x = rect.left + (rect.right - rect.left) / 2 - 150;
        pt.y = rect.bottom - (rect.bottom - rect.top) / 2 - 100;
        ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
      }
      if (!commit_str.empty()) {
        send_input_to_window(hwnd, commit_str);
        commit_str.clear();
        if (!status.composing)
          ui->Hide();
        committed = true;
      } else
        committed = false;
      if ((ke.keycode == ibus::Caps_Lock && keyCountToSimulate) ||
          status.ascii_mode) {
        goto skip;
      }
      if (eat)
        return 1;
    }
  }
skip:
  return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void release_hook() { UnhookWindowsHookEx(hHook); }

void set_hook() {
  hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (hHook == NULL) {
    cerr << "Failed to install hook!" << endl;
    exit(1);
  }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  trayIcon->ProcessMessage(hwnd, msg, wParam, lParam);
  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

void RefeshTrayIcon(const Status &status) {
  if (status.ascii_mode)
    trayIcon->SetIcon(ascii_icon);
  else
    trayIcon->SetIcon(ime_icon);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
  wstring cmdLine(lpCmdLine), arg;
  wistringstream wiss(cmdLine);
  while (wiss >> arg) {
    if (arg == L"/h")
      horizontal = true;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  ui.reset(new UI());
  toy.reset(new RimeWithToy(ui.get()));
  toy->SetTrayIconCallBack(RefeshTrayIcon);
  ui->SetHorizontal(horizontal);
  ui->Create(nullptr);
  set_hook();
  // --------------------------------------------------------------------------
  ime_icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN));
  ascii_icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_ASCII));
  //  注册窗口类
  WNDCLASS wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"Rime.Toy.App";
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClass(&wc);

  // --------------------------------------------------------------------------
  // 创建一个隐藏窗口
  HWND hwnd = CreateWindow(L"Rime.Toy.App", L"Rime.Toy.App",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 0,
                           0, NULL, NULL, hInstance, NULL);
  // 创建托盘图标
  trayIcon.reset(new TrayIcon(hInstance, hwnd, L"rime.toy"));
  trayIcon->SetDeployFunc([&]() { toy->Initialize(); });
  trayIcon->SetSwichAsciiFunc([&]() { toy->SwitchAsciiMode(); });
  trayIcon->SetIcon(wc.hIcon); // 使用默认图标
  trayIcon->SetTooltip(L"rime.toy\n左键点击切换ASCII, 右键菜单可退出^_^");
  trayIcon->Show();
  // 不显示窗口
  ShowWindow(hwnd, SW_HIDE);
  // --------------------------------------------------------------------------
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  toy->Finalize();
  release_hook();
  return 0;
}
