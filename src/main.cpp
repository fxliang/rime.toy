// Copyright fxliang
// Distrobuted under GPLv3 https://www.gnu.org/licenses/gpl-3.0.en.html
#include <data.h>
#include <filesystem>
#include <iostream>
#include <rime_api.h>
namespace fs = std::filesystem;

HHOOK hHook = NULL;
BYTE keyState[256] = {0};
RimeSessionId session_id = NULL;
Context old_ctx;
bool to_commit = true;
bool is_composing = false;
std::string commit_str = "";
UINT original_codepage;
bool horizontal = true, escape_ansi = false;

#ifdef IME_STUFF
HWND hwnd_previous;
HIMC hOriginalIMC = NULL;
#endif

KeyInfo ki(0);

// ----------------------------------------------------------------------------

int expand_ibus_modifier(int m) { return (m & 0xff) | ((m & 0xff00) << 16); }
// ----------------------------------------------------------------------------
inline std::wstring string_to_wstring(const std::string &str,
                                      int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != 0 && code_page != CP_UTF8)
    return L"";
  // calc len
  int len =
      MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), NULL, 0);
  if (len <= 0)
    return L"";
  std::wstring res;
  WCHAR *buffer = new WCHAR[len + 1];
  MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), buffer, len);
  buffer[len] = '\0';
  res.append(buffer);
  delete[] buffer;
  return res;
}

inline std::string wstring_to_string(const std::wstring &wstr,
                                     int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != 0 && code_page != CP_UTF8)
    return "";
  int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                                NULL, 0, NULL, NULL);
  if (len <= 0)
    return "";
  std::string res;
  char *buffer = new char[len + 1];
  WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(), buffer, len,
                      NULL, NULL);
  buffer[len] = '\0';
  res.append(buffer);
  delete[] buffer;
  return res;
}

void on_message(void *context_object, RimeSessionId session_id,
                const char *message_type, const char *message_value) {
  printf("message: [%p] [%s] %s\n", (void *)session_id, message_type,
         message_value);
  RimeApi *rime = rime_get_api();
  if (RIME_API_AVAILABLE(rime, get_state_label) &&
      !strcmp(message_type, "option")) {
    Bool state = message_value[0] != '!';
    const char *option_name = message_value + !state;
    const char *state_label =
        rime->get_state_label(session_id, option_name, state);
    if (state_label) {
      printf("updated option: %s = %d // %s\n", option_name, state,
             state_label);
    }
  }
}

void get_candidate_info(CandidateInfo &cinfo, RimeContext &ctx) {
  cinfo.candies.resize(ctx.menu.num_candidates);
  cinfo.comments.resize(ctx.menu.num_candidates);
  cinfo.labels.resize(ctx.menu.num_candidates);
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    cinfo.candies[i].str =
        string_to_wstring(ctx.menu.candidates[i].text, CP_UTF8);
    if (ctx.menu.candidates[i].comment) {
      cinfo.comments[i].str =
          string_to_wstring(ctx.menu.candidates[i].comment, CP_UTF8);
    }
    if (RIME_STRUCT_HAS_MEMBER(ctx, ctx.select_labels) && ctx.select_labels) {
      cinfo.labels[i].str = string_to_wstring(ctx.select_labels[i], CP_UTF8);
    } else if (ctx.menu.select_keys) {
      cinfo.labels[i].str = std::wstring(1, ctx.menu.select_keys[i]);
    } else {
      cinfo.labels[i].str = std::to_wstring((i + 1) % 10);
    }
  }
  cinfo.highlighted = ctx.menu.highlighted_candidate_index;
  cinfo.currentPage = ctx.menu.page_no;
  cinfo.is_last_page = ctx.menu.is_last_page;
}

inline int utf8towcslen(const char *utf8_str, int utf8_len) {
  return MultiByteToWideChar(CP_UTF8, 0, utf8_str, utf8_len, NULL, 0);
}

void get_context(Context &weasel_context, RimeContext ctx) {
  if (ctx.composition.length > 0) {
    weasel_context.preedit.str =
        string_to_wstring(ctx.composition.preedit, CP_UTF8);
    if (ctx.composition.sel_start < ctx.composition.sel_end) {
      TextAttribute attr;
      attr.type = HIGHLIGHTED;
      attr.range.start =
          utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start);
      attr.range.end =
          utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end);

      weasel_context.preedit.attributes.push_back(attr);
    }
  }
  if (ctx.menu.num_candidates) {
    CandidateInfo &cinfo(weasel_context.cinfo);
    get_candidate_info(cinfo, ctx);
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

// ----------------------------------------------------------------------------
void clear_screen() {
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  COORD coordScreen = {0, 0};
  DWORD cCharsWritten;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  DWORD dwConSize;
  // 获取控制台屏幕缓冲区信息
  if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
    return;
  }
  dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
  // 用空格填充缓冲区
  if (!FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen,
                                  &cCharsWritten)) {
    return;
  }
  // 设置缓冲区属性
  if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize,
                                  coordScreen, &cCharsWritten)) {
    return;
  }
  // 将光标移动到左上角
  SetConsoleCursorPosition(hConsole, coordScreen);
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

void print_status(RimeStatus *status) {
  printf("schema: %s / %s\n", status->schema_id, status->schema_name);
  printf("status: ");
  if (status->is_disabled)
    printf("disabled ");
  if (status->is_composing)
    printf("composing ");
  if (status->is_ascii_mode)
    printf("ascii ");
  if (status->is_full_shape)
    printf("full_shape ");
  if (status->is_simplified)
    printf("simplified ");
  printf("\n");
}

void print_composition(RimeComposition *composition) {
  const char *preedit = composition->preedit;
  if (!preedit)
    return;
  size_t len = strlen(preedit);
  size_t start = composition->sel_start;
  size_t end = composition->sel_end;
  size_t cursor = composition->cursor_pos;
  for (size_t i = 0; i <= len; ++i) {
    if (start < end) {
      if (i == start) {
        if (escape_ansi)
          printf("\x1b[7m");
        else
          putchar('[');
      } else if (i == end) {
        if (escape_ansi)
          printf("\x1b[0m");
        else
          putchar(']');
      }
    }
    if (i == cursor)
      putchar('|');
    if (i < len)
      putchar(preedit[i]);
  }
  printf("\n");
}

void print_menu(RimeMenu *menu) {
  if (menu->num_candidates == 0)
    return;
  // printf("\tpage: %d%c (of size %d)\n", menu->page_no + 1,
  //        menu->is_last_page ? '$' : ' ', menu->page_size);
  for (int i = 0; i < menu->num_candidates; ++i) {
    bool highlighted = i == menu->highlighted_candidate_index;
    const char *sep0 = escape_ansi ? "\x1b[7m" : "[";
    const char *sep1 = escape_ansi ? "\x1b[0m" : "]";
    printf("%s%d.%s%s%s%s", highlighted ? sep0 : " ", i + 1,
           menu->candidates[i].text,
           menu->candidates[i].comment ? menu->candidates[i].comment : "",
           horizontal ? "" : "\n", highlighted ? sep1 : " ");
  }
}

void print_context(RimeContext *context) {
  if (context->composition.length > 0 || context->menu.num_candidates > 0) {
    print_composition(&context->composition);
  } else {
    // printf("(not composing)\n");
  }
  print_menu(&context->menu);
}

void print(RimeSessionId session_id) {
  RimeApi *rime = rime_get_api();

  RIME_STRUCT(RimeCommit, commit);
  RIME_STRUCT(RimeStatus, status);
  RIME_STRUCT(RimeContext, context);

  if (rime->get_commit(session_id, &commit)) {
    // printf("commit: %s\n", commit.text);
    commit_str = std::string(commit.text);
    to_commit = true;
    rime->free_commit(&commit);
  } else {
    to_commit = false;
    commit_str.clear();
  }

  auto is_composing_buff = is_composing;
  if (rime->get_status(session_id, &status)) {
    // print_status(&status);
    is_composing = status.is_composing;
    rime->free_status(&status);
  } else
    is_composing = false;

  if (is_composing_buff != is_composing || to_commit)
    clear_screen();

  if (rime->get_context(session_id, &context)) {
    Context ctx;
    get_context(ctx, context);
    if (ctx != old_ctx) {
      old_ctx = ctx;
      clear_screen();
      print_context(&context);
    }
    rime->free_context(&context);
  }
}

#ifdef IME_STUFF /* not ok yet */
// disable ime, a possible solution
void disable_ime(HWND hWnd) {
  hOriginalIMC = ImmGetContext(hWnd);
  if (hOriginalIMC) {
    ImmAssociateContext(hWnd, NULL);
  }
}

// recover
void restore_ime(HWND hWnd) {
  if (hOriginalIMC)
    ImmAssociateContext(hWnd, hOriginalIMC);
}
#endif

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  HWND hwnd = GetForegroundWindow();
#ifdef IME_STUFF /* not ok yet */
  disable_ime(hwnd);
  if (hwnd != hwnd_previous) {
    if (!hwnd_previous)
      restore_ime(hwnd_previous);
    hwnd_previous = hwnd;
  }
#endif
  if (nCode == HC_ACTION) {
    KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
    BYTE tmp = keyState[pKeyboard->vkCode];
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
#ifdef PRINT_KEY_UPDATE
    for (int i = 0; i < 256; ++i) {
      if (i == pKeyboard->vkCode && tmp != keyState[pKeyboard->vkCode]) {
        if (keyState[i] & 0x80) {
          std::cout << "Key " << std::hex << i << " is pressed." << std::endl;
        } else {
          std::cout << "Key " << std::hex << i << " is released." << std::endl;
        }
      }
    }
#endif
    // get KBDLLHOOKSTRUCT info, generate keyinfo
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
    KeyEvent ke;
    if (ConvertKeyEvent(pKeyboard->vkCode, ki, keyState, ke)) {
      // std::cout << std::hex << "ke.keycode = " <<  ke.keycode << ", ke.mask =
      // " << ke.mask << std::endl;
#if 1
      // todo: capslock to be handled
      RimeApi *rime_api = rime_get_api();
      assert(rime_api);
      auto eat = rime_api->process_key(session_id, ke.keycode,
                                       expand_ibus_modifier(ke.mask));
      print(session_id);
      if (eat) {
        if (to_commit)
          send_input_to_window(hwnd, string_to_wstring(commit_str, CP_UTF8));
        return 1;
      }
#endif
    }
  }
  return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void release_hook() { UnhookWindowsHookEx(hHook); }

void clean_up() {
  RimeApi *rime_api = rime_get_api();
  assert(rime_api);
  rime_api->destroy_session(session_id);
  rime_api->finalize();
  release_hook();
  SetConsoleOutputCP(original_codepage);
}

void set_hook() {
  hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (hHook == NULL) {
    std::cerr << "Failed to install hook!" << std::endl;
    clean_up();
    exit(1);
  }
}

fs::path data_path(std::string subdir) {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append(subdir);
}

inline fs::path get_log_path() {
  WCHAR _path[MAX_PATH] = {0};
  // default location
  ExpandEnvironmentStringsW(L"%TEMP%\\rime.toy", _path, _countof(_path));
  fs::path path = fs::path(_path);
  if (!fs::exists(path)) {
    fs::create_directories(path);
  }
  return path;
}

void setup_rime() {
  RIME_STRUCT(RimeTraits, weasel_traits);
  auto shared_path = data_path("shared");
  auto usr_path = data_path("usr");
  auto log_path = get_log_path();
  std::cout << "shared data dir: " << shared_path.u8string() << std::endl
            << "user data dir: " << usr_path.u8string() << std::endl
            << "log dir: " << log_path.u8string() << std::endl;
  if (!fs::exists(shared_path))
    fs::create_directory(shared_path);
  if (!fs::exists(usr_path))
    fs::create_directory(usr_path);
  if (!fs::exists(log_path))
    fs::create_directory(log_path);
  static auto shared_dir = shared_path.u8string();
  static auto usr_dir = usr_path.u8string();
  static auto log_dir = log_path.u8string();
  weasel_traits.shared_data_dir = shared_dir.c_str();
  weasel_traits.user_data_dir = usr_dir.c_str();
  weasel_traits.log_dir = log_dir.c_str();
  weasel_traits.prebuilt_data_dir = weasel_traits.shared_data_dir;
  weasel_traits.distribution_name = "rime.toy";
  weasel_traits.distribution_code_name = "rime.toy";
  weasel_traits.distribution_version = "0.0.1.0";
  weasel_traits.app_name = "rime.toy";
  RimeApi *rime_api = rime_get_api();
  assert(rime_api);
  rime_api->setup(&weasel_traits);
  rime_api->set_notification_handler(&on_message, nullptr);
}

BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT) {
    std::cout << "Control+c detected, cleaning up..." << std::endl;
    clean_up();
    exit(0);
  }
  return FALSE;
}

int main(int argc, char *argv[]) {
  for (auto i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "/v")
      horizontal = false;
    if (arg == "/e")
      escape_ansi = true;
  }

  original_codepage = GetConsoleOutputCP();
  SetConsoleOutputCP(CP_UTF8);
  setup_rime();
  fprintf(stderr, "initializing...\n");
  RimeApi *rime_api = rime_get_api();
  assert(rime_api);
  rime_api->initialize(NULL);
  if (rime_api->start_maintenance(true))
    rime_api->join_maintenance_thread();
  fprintf(stderr, "ready.\n");
  session_id = rime_api->create_session();
  assert(session_id);
  set_hook();
  if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
    std::cerr << "Error: Could not set control handler." << std::endl;
    clean_up();
    return 1;
  }
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  std::cout << "cleaning up..." << std::endl;
  clean_up();
  return 0;
}
