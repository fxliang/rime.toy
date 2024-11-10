#include "keymodule.h"
namespace weasel {

static BYTE keyState[256] = {0};
// ----------------------------------------------------------------------------
ibus::Keycode TranslateKeycode(UINT vkey, KeyInfo kinfo) {
  switch (vkey) {
  case VK_BACK:
    return ibus::BackSpace;
  case VK_TAB:
    return ibus::Tab;
  case VK_CLEAR:
    return ibus::Clear;
  case VK_RETURN: {
    if (kinfo.isExtended == 1)
      return ibus::KP_Enter;
    else
      return ibus::Return;
  }
  case VK_SHIFT: {
    if (kinfo.scanCode == 0x36)
      return ibus::Shift_R;
    else
      return ibus::Shift_L;
  }
  case VK_CONTROL: {
    if (kinfo.isExtended == 1)
      return ibus::Control_R;
    else
      return ibus::Control_L;
  }
  case VK_MENU:
    return ibus::Alt_L;
  case VK_PAUSE:
    return ibus::Pause;
  case VK_CAPITAL:
    return ibus::Caps_Lock;

  case VK_KANA:
    return ibus::Hiragana_Katakana;
    // case VK_JUNJA:	return 0;
    // case VK_FINAL:	return 0;
  case VK_KANJI:
    return ibus::Kanji;

  case VK_ESCAPE:
    return ibus::Escape;

  case VK_CONVERT:
    return ibus::Henkan;
  case VK_NONCONVERT:
    return ibus::Muhenkan;
    // case VK_ACCEPT:	return 0;
    // case VK_MODECHANGE:	return 0;

  case VK_SPACE:
    return ibus::space;
  case VK_PRIOR:
    return ibus::Prior;
  case VK_NEXT:
    return ibus::Next;
  case VK_END:
    return ibus::End;
  case VK_HOME:
    return ibus::Home;
  case VK_LEFT:
    return ibus::Left;
  case VK_UP:
    return ibus::Up;
  case VK_RIGHT:
    return ibus::Right;
  case VK_DOWN:
    return ibus::Down;
  case VK_SELECT:
    return ibus::Select;
  case VK_PRINT:
    return ibus::Print;
  case VK_EXECUTE:
    return ibus::Execute;
    // case VK_SNAPSHOT:	return 0;
  case VK_INSERT:
    return ibus::Insert;
  case VK_DELETE:
    return ibus::Delete;
  case VK_HELP:
    return ibus::Help;

  case VK_LWIN:
    return ibus::Meta_L;
  case VK_RWIN:
    return ibus::Meta_R;
    // case VK_APPS:	return 0;
    // case VK_SLEEP:	return 0;
  case VK_NUMPAD0:
    return ibus::KP_0;
  case VK_NUMPAD1:
    return ibus::KP_1;
  case VK_NUMPAD2:
    return ibus::KP_2;
  case VK_NUMPAD3:
    return ibus::KP_3;
  case VK_NUMPAD4:
    return ibus::KP_4;
  case VK_NUMPAD5:
    return ibus::KP_5;
  case VK_NUMPAD6:
    return ibus::KP_6;
  case VK_NUMPAD7:
    return ibus::KP_7;
  case VK_NUMPAD8:
    return ibus::KP_8;
  case VK_NUMPAD9:
    return ibus::KP_9;
  case VK_MULTIPLY:
    return ibus::KP_Multiply;
  case VK_ADD:
    return ibus::KP_Add;
  case VK_SEPARATOR:
    return ibus::KP_Separator;
  case VK_SUBTRACT:
    return ibus::KP_Subtract;
  case VK_DECIMAL:
    return ibus::KP_Decimal;
  case VK_DIVIDE:
    return ibus::KP_Divide;
  case VK_F1:
    return ibus::F1;
  case VK_F2:
    return ibus::F2;
  case VK_F3:
    return ibus::F3;
  case VK_F4:
    return ibus::F4;
  case VK_F5:
    return ibus::F5;
  case VK_F6:
    return ibus::F6;
  case VK_F7:
    return ibus::F7;
  case VK_F8:
    return ibus::F8;
  case VK_F9:
    return ibus::F9;
  case VK_F10:
    return ibus::F10;
  case VK_F11:
    return ibus::F11;
  case VK_F12:
    return ibus::F12;
  case VK_F13:
    return ibus::F13;
  case VK_F14:
    return ibus::F14;
  case VK_F15:
    return ibus::F15;
  case VK_F16:
    return ibus::F16;
  case VK_F17:
    return ibus::F17;
  case VK_F18:
    return ibus::F18;
  case VK_F19:
    return ibus::F19;
  case VK_F20:
    return ibus::F20;
  case VK_F21:
    return ibus::F21;
  case VK_F22:
    return ibus::F22;
  case VK_F23:
    return ibus::F23;
  case VK_F24:
    return ibus::F24;

  case VK_NUMLOCK:
    return ibus::Num_Lock;
  case VK_SCROLL:
    return ibus::Scroll_Lock;

  case VK_LSHIFT:
    return ibus::Shift_L;
  case VK_RSHIFT:
    return ibus::Shift_R;
  case VK_LCONTROL:
    return ibus::Control_L;
  case VK_RCONTROL:
    return ibus::Control_R;
  case VK_LMENU:
    return ibus::Alt_L;
  case VK_RMENU:
    return ibus::Alt_R;

  case VK_OEM_AUTO:
    return ibus::Zenkaku_Hankaku;
  case VK_OEM_ENLW:
    return ibus::Zenkaku_Hankaku;
  }
  return ibus::Null;
}

bool ConvertKeyEvent(const KBDLLHOOKSTRUCT *pKeyboard, KeyInfo &kinfo,
                     KeyEvent &result) {
  static HKL hkl = GetKeyboardLayout(0);
  const BYTE KEY_DOWN = 0x80;
  const BYTE TOGGLED = 0x01;
  result.mask = ibus::NULL_MASK;

  const auto vkey = pKeyboard->vkCode;

  if ((keyState[VK_SHIFT] & KEY_DOWN) || (keyState[VK_LSHIFT] & KEY_DOWN) ||
      (keyState[VK_RSHIFT] & KEY_DOWN))
    result.mask |= ibus::SHIFT_MASK;

  if (keyState[VK_CONTROL] & KEY_DOWN)
    result.mask |= ibus::CONTROL_MASK;

  if (keyState[VK_MENU] & KEY_DOWN)
    result.mask |= ibus::ALT_MASK;

  if ((keyState[VK_LWIN] & KEY_DOWN) || (keyState[VK_RWIN] & KEY_DOWN))
    result.mask |= ibus::SUPER_MASK;

  if (keyState[VK_CAPITAL] & TOGGLED) {
    result.mask |= ibus::LOCK_MASK;
  }

  if (kinfo.isKeyUp)
    result.mask |= ibus::RELEASE_MASK;

  if (vkey == VK_CAPITAL && !kinfo.isKeyUp) {
    result.mask ^= ibus::LOCK_MASK;
  }

  ibus::Keycode code = TranslateKeycode(vkey, kinfo);
  if (code) {
    result.keycode = code;
    return true;
  }

  const int buf_len = 8;
  static WCHAR buf[buf_len];
  static BYTE table[256];
  // 清除Ctrl、Alt鍵狀態，以令ToUnicodeEx()返回字符
  memcpy(table, keyState, sizeof(table));
  table[VK_CONTROL] = 0;
  table[VK_MENU] = 0;
  table[VK_CAPITAL] = GetKeyState(VK_CAPITAL) & 0X01;
  int ret = ToUnicodeEx(vkey, UINT(kinfo), table, buf, buf_len, 0, NULL);
  if (ret == 1) {
    result.keycode = UINT(buf[0]);
    return true;
  }

  result.keycode = 0;
  return false;
}
// ----------------------------------------------------------------------------

KeyInfo parse_key(WPARAM wParam, LPARAM lParam) {
  KeyInfo ki(0);
  KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
  BYTE tmp = keyState[pKeyboard->vkCode];
  DWORD vkCode = pKeyboard->vkCode;
  DWORD scanCode = pKeyboard->scanCode;
  DWORD flags = pKeyboard->flags;
  DWORD time = pKeyboard->time;
  ULONG_PTR dwExtraInfo = pKeyboard->dwExtraInfo;

  if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
    ki.repeatCount = 1;
  else if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
    if ((keyState[pKeyboard->vkCode] & 0x80) && ki.repeatCount < 0xffff &&
        scanCode == ki.scanCode)
      ki.repeatCount++;
    else
      ki.repeatCount = 1;
  }
  ki.scanCode = scanCode;
  ki.isExtended = (flags & LLKHF_EXTENDED) != 0;
  ki.contextCode = 0;
  // for WM_KEYDOWN or WM_SYSKEYDOWN, The value is 1 if the key is down before
  // the message is sent, or it is zero if the key is up. The value is always
  // 1 for a WM_KEYUP and WM_SYSKEYUP message.
  ki.prevKeyState =
      (((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && (tmp & 0x80)) ||
       (wParam == WM_KEYUP || wParam == WM_SYSKEYUP))
          ? 1
          : 0;
  ki.isKeyUp = (flags & LLKHF_UP) ? 1 : 0;
  return ki;
}

void update_keystates(WPARAM wParam, LPARAM lParam) {
  KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
  if (pKeyboard->vkCode != VK_CAPITAL) {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      keyState[pKeyboard->vkCode] |= 0x80; // 设置按键状态为按下
    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
      keyState[pKeyboard->vkCode] &= ~0x80; // 设置按键状态为松开
    }
    keyState[VK_SHIFT] = (keyState[VK_LSHIFT] | keyState[VK_RSHIFT]);
    keyState[VK_CONTROL] = (keyState[VK_LCONTROL] | keyState[VK_RCONTROL]);
    keyState[VK_MENU] = (keyState[VK_LMENU] | keyState[VK_RMENU]);
  } else {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      keyState[pKeyboard->vkCode] |= 0x01; // 设置按键状态为按下
    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
      keyState[pKeyboard->vkCode] &= ~0x01; // 设置按键状态为松开
    }
  }
}

void send_input_to_window(HWND hwnd, const std::wstring &text) {
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
} // namespace weasel
