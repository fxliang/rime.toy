// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include "config.h"
#include <WeaselUI.h>
#include <data.h>
#include <filesystem>
#include <iostream>
#include <resource.h>
#include <rime_api.h>
#include <sstream>
#include <trayicon.h>

using namespace std;
using namespace weasel;
namespace fs = filesystem;

HHOOK hHook = NULL;
BYTE keyState[256] = {0};
RimeSessionId session_id = NULL;
Status old_sta;
Status sta;
bool committed = true;
wstring commit_str = L"";
bool horizontal = false, escape_ansi = false;
bool hook_enabled = true;
RECT rect;

KeyInfo ki(0);
UI *ui;
TrayIcon *trayIcon;
HICON ascii_icon;
HICON ime_icon;

// ----------------------------------------------------------------------------
int expand_ibus_modifier(int m) { return (m & 0xff) | ((m & 0xff00) << 16); }
// ----------------------------------------------------------------------------

void on_message(void *context_object, RimeSessionId session_id,
                const char *message_type, const char *message_value) {
  // printf("message: [%p] [%s] %s\n", (void *)session_id, message_type,
  //        message_value);
  RimeApi *rime = rime_get_api();
  if (RIME_API_AVAILABLE(rime, get_state_label) &&
      !strcmp(message_type, "option")) {
    Bool state = message_value[0] != '!';
    const char *option_name = message_value + !state;
    const char *state_label =
        rime->get_state_label(session_id, option_name, state);
    if (string(option_name) == "ascii_mode") {
      if (trayIcon) {
        trayIcon->SetIcon(state ? ascii_icon : ime_icon);
      }
    }
    if (state_label) {
      HWND hwnd = GetForegroundWindow();
      if (hwnd) {
        GetWindowRect(hwnd, &rect);
      }
      auto msg = u8tow(sta.schema_name + " " + string(state_label));
      if (ui) {
        ui->SetText(msg);
        ui->Refresh();
        POINT pt;
        if (GetCursorPos(&pt)) {
          ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
        } else {
          pt.x = rect.left + (rect.right - rect.left) / 2 - 150;
          pt.y = rect.bottom - (rect.bottom - rect.top) / 2 - 100;
          ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
        }
        ui->ShowWithTimeout(500);
      }
    }
  }
}

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

bool ConvertKeyEvent(UINT vkey, KeyInfo kinfo, const LPBYTE keyState,
                     KeyEvent &result) {
  static HKL hkl = GetKeyboardLayout(0);
  const BYTE KEY_DOWN = 0x80;
  const BYTE TOGGLED = 0x01;
  result.mask = ibus::NULL_MASK;

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

void get_candidate_info(CandidateInfo &cinfo, RimeContext &ctx) {
  cinfo.candies.resize(ctx.menu.num_candidates);
  cinfo.comments.resize(ctx.menu.num_candidates);
  cinfo.labels.resize(ctx.menu.num_candidates);
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    cinfo.candies[i].str = u8tow(ctx.menu.candidates[i].text);
    if (ctx.menu.candidates[i].comment) {
      cinfo.comments[i].str = u8tow(ctx.menu.candidates[i].comment);
    }
    if (RIME_STRUCT_HAS_MEMBER(ctx, ctx.select_labels) && ctx.select_labels) {
      cinfo.labels[i].str = u8tow(ctx.select_labels[i]);
    } else if (ctx.menu.select_keys) {
      cinfo.labels[i].str = wstring(1, ctx.menu.select_keys[i]);
    } else {
      cinfo.labels[i].str = to_wstring((i + 1) % 10);
    }
  }
  cinfo.highlighted = ctx.menu.highlighted_candidate_index;
  cinfo.currentPage = ctx.menu.page_no;
  cinfo.is_last_page = ctx.menu.is_last_page;
}

void get_context(Context &context, RimeSessionId id) {
  RimeApi *rime = rime_get_api();
  assert(rime);
  RIME_STRUCT(RimeContext, ctx);
  if (rime->get_context(id, &ctx)) {
    if (ctx.composition.length > 0) {
      context.preedit.str = u8tow(ctx.composition.preedit);
      if (ctx.composition.sel_start < ctx.composition.sel_end) {
        TextAttribute attr;
        attr.type = HIGHLIGHTED;
        attr.range.start = ctx.composition.sel_start;
        attr.range.end = ctx.composition.sel_end;
        attr.range.cursor = ctx.composition.cursor_pos;
        context.preedit.attributes.push_back(attr);
      }
    }
    if (ctx.menu.num_candidates) {
      CandidateInfo &cinfo(context.cinfo);
      get_candidate_info(cinfo, ctx);
    }
    rime->free_context(&ctx);
  }
}

void get_status(Status &status, RimeSessionId id) {
  RimeApi *rime = rime_get_api();
  assert(rime);
  RIME_STRUCT(RimeStatus, status_);
  if (rime->get_status(id, &status_)) {
    status.ascii_mode = !!status_.is_ascii_mode;
    status.composing = !!status_.is_composing;
    status.disabled = !!status_.is_disabled;
    status.full_shape = !!status_.is_full_shape;
    status.schema_id = status_.schema_id;
    status.schema_name = status_.schema_name;
    rime->free_status(&status_);
  }
}

void get_commit() {
  RimeApi *rime = rime_get_api();
  assert(rime);
  RIME_STRUCT(RimeCommit, commit);
  if (rime->get_commit(session_id, &commit)) {
    commit_str = u8tow(commit.text);
    rime->free_commit(&commit);
  } else {
    commit_str.clear();
  }
}

void update_ui() {
  Context ctx;
  get_commit();
  get_context(ctx, session_id);
  get_status(sta, session_id);
  if (sta != old_sta)
    old_sta = sta;
  static RimeApi *rime = rime_get_api();
  const char *alabel = rime->get_state_label(session_id, "ascii_mode", 1);
  const char *nlabel = rime->get_state_label(session_id, "ascii_mode", 0);
  if (ctx.empty()) {
    ui->Hide();
    return;
  } else {
    std::wstring text_ = (ctx.preedit.str) + L" ";
    for (auto i = 0; i < ctx.cinfo.candies.size(); i++) {
      bool highlighted = i == ctx.cinfo.highlighted;
      text_ += highlighted ? L"[" : L"";
      text_ += (ctx.cinfo.labels[i].str) + L". " + (ctx.cinfo.candies[i].str) +
               L" " + (ctx.cinfo.comments[i].str);
      text_ += highlighted ? L"] " : L" ";
    }
    if (ui) {
      ctx.aux.clear();
      ui->Update(ctx, sta);
      ui->Refresh();
      POINT pt;
      if (GetCursorPos(&pt)) {
        ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
      } else {
        pt.x = rect.left + (rect.right - rect.left) / 2 - 150;
        pt.y = rect.bottom - (rect.bottom - rect.top) / 2 - 100;
        ui->UpdateInputPosition({pt.x, 0, 0, pt.y});
      }
      ui->Show();
    }
  }
}

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

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  const int TOGGLE_KEY = 0x31; // key '1'
  static KeyEvent prevKeyEvent;
  static BOOL prevfEaten = FALSE;
  static int keyCountToSimulate = 0;
  static HWND hwnd_previous = nullptr;

  HWND hwnd = GetForegroundWindow();
  if (hwnd) {
    GetWindowRect(hwnd, &rect);
  }
  if (hwnd != hwnd_previous) {
    hwnd_previous = hwnd;
    ui->SetText(L"");
    ui->Hide();
  }
  if (nCode == HC_ACTION) {
    KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
    if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) &&
        pKeyboard->vkCode == TOGGLE_KEY && (keyState[VK_LCONTROL] & 0x80)) {
      hook_enabled = !hook_enabled;
      RimeApi *rime_api = rime_get_api();
      rime_api->clear_composition(session_id);
      if (ui) {
        ui->SetText(L"");
        ui->Hide();
      }
      update_ui();
      return 1;
    }
    update_keystates(wParam, lParam);
    // if not enabled, ignore it;
    if (!hook_enabled)
      return CallNextHookEx(hHook, nCode, wParam, lParam);

    // get KBDLLHOOKSTRUCT info, generate keyinfo
    ki = parse_key(wParam, lParam);
    KeyEvent ke;
    if (ConvertKeyEvent(pKeyboard->vkCode, ki, keyState, ke)) {
      RimeApi *rime_api = rime_get_api();
      assert(rime_api);
      bool eat = false;
      if (!keyCountToSimulate)
        eat = rime_api->process_key(session_id, ke.keycode,
                                    expand_ibus_modifier(ke.mask));
      if (ke.keycode == ibus::Caps_Lock) {
        if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
            (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
          if (GetKeyState(VK_CAPITAL) & 0x01) {
            if (committed || (!eat && old_sta.composing)) {
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
      prevfEaten = eat;
      prevKeyEvent = ke;

      update_ui();
      if (!commit_str.empty()) {
        send_input_to_window(hwnd, commit_str);
        if (!sta.composing)
          ui->Refresh();
        committed = true;
      } else
        committed = false;
      if ((ke.keycode == ibus::Caps_Lock && keyCountToSimulate) ||
          old_sta.ascii_mode) {
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

void clean_up() {
  RimeApi *rime_api = rime_get_api();
  assert(rime_api);
  rime_api->destroy_session(session_id);
  rime_api->finalize();
  release_hook();
}

void set_hook() {
  hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (hHook == NULL) {
    cerr << "Failed to install hook!" << endl;
    clean_up();
    exit(1);
  }
}

fs::path data_path(string subdir) {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append(subdir);
}

fs::path get_log_path() {
  WCHAR _path[MAX_PATH] = {0};
  ExpandEnvironmentStringsW(L"%TEMP%\\rime.toy", _path, _countof(_path));
  fs::path path = fs::path(_path);
  if (!fs::exists(path)) {
    fs::create_directories(path);
  }
  return path;
}

void setup_rime() {
  RIME_STRUCT(RimeTraits, traits);
  auto shared_path = data_path("shared");
  auto usr_path = data_path("usr");
  auto log_path = get_log_path();
  if (!fs::exists(shared_path))
    fs::create_directory(shared_path);
  if (!fs::exists(usr_path))
    fs::create_directory(usr_path);
  if (!fs::exists(log_path))
    fs::create_directory(log_path);
  static auto shared_dir = shared_path.u8string();
  static auto usr_dir = usr_path.u8string();
  static auto log_dir = log_path.u8string();
  traits.shared_data_dir = reinterpret_cast<const char *>(shared_dir.c_str());
  traits.user_data_dir = reinterpret_cast<const char *>(usr_dir.c_str());
  traits.log_dir = reinterpret_cast<const char *>(log_dir.c_str());
  traits.prebuilt_data_dir = traits.shared_data_dir;
  traits.distribution_name = "rime.toy";
  traits.distribution_code_name = "rime.toy";
  traits.distribution_version = "0.0.2.0";
  traits.app_name = "rime.toy";
  RimeApi *rime_api = rime_get_api();
  assert(rime_api);
  rime_api->setup(&traits);
  rime_api->set_notification_handler(&on_message, nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  trayIcon->ProcessMessage(hwnd, msg, wParam, lParam);
  switch (msg) {
  case WM_CLOSE:
    DestroyWindow(hwnd);
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
  wstring cmdLine(lpCmdLine), arg;
  wistringstream wiss(cmdLine);
  while (wiss >> arg) {
    if (arg == L"/h")
      horizontal = true;
  }
  setup_rime();
  RimeApi *rime_api = rime_get_api();
  assert(rime_api);
  rime_api->initialize(NULL);
  if (rime_api->start_maintenance(true))
    rime_api->join_maintenance_thread();
  rime_api->deploy_config_file("weasel.yaml", "config_version");
  session_id = rime_api->create_session();
  assert(session_id);
  set_hook();
  // --------------------------------------------------------------------------
  ui = new UI();
  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    _UpdateUIStyle(&config, ui, true);
    rime_api->config_close(&config);
  } else {
    OutputDebugString(L"open weasel config failed");
  }
  ui->SetHorizontal(horizontal);
  ui->Create(nullptr);
  rime_api->set_option(session_id, "soft_cursor",
                       Bool(!ui->style().inline_preedit));
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

  // 创建一个隐藏窗口
  HWND hwnd = CreateWindow(L"Rime.Toy.App", L"Rime.Toy.App",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 0,
                           0, NULL, NULL, hInstance, NULL);
  // 创建托盘图标
  trayIcon = new TrayIcon(hInstance, hwnd, L"rime.toy");
  trayIcon->SetIcon(wc.hIcon); // 使用默认图标
  trayIcon->SetTooltip(L"rime.toy\n右键菜单可退出^_^");
  trayIcon->Show();
  // 不显示窗口
  ShowWindow(hwnd, SW_HIDE);
  // --------------------------------------------------------------------------
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  clean_up();
  delete trayIcon;
  return 0;
}
