#pragma once

#include <assert.h>
#include <cstdint>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <windows.h>
namespace weasel {
enum TextAttributeType { NONE = 0, HIGHLIGHTED, LAST_TYPE };

struct TextRange {
  TextRange() : start(0), end(0), cursor(-1) {}
  TextRange(int _start, int _end, int _cursor)
      : start(_start), end(_end), cursor(_cursor) {}
  bool operator==(const TextRange &tr) {
    return (start == tr.start && end == tr.end && cursor == tr.cursor);
  }
  bool operator!=(const TextRange &tr) {
    return (start != tr.start || end != tr.end || cursor != tr.cursor);
  }
  int start;
  int end;
  int cursor;
};

struct TextAttribute {
  TextAttribute() : type(NONE) {}
  TextAttribute(int _start, int _end, TextAttributeType _type)
      : range(_start, _end, -1), type(_type) {}
  bool operator==(const TextAttribute &ta) {
    return (range == ta.range && type == ta.type);
  }
  bool operator!=(const TextAttribute &ta) {
    return (range != ta.range || type != ta.type);
  }
  TextRange range;
  TextAttributeType type;
};

struct Text {
  Text() : str("") {}
  Text(std::string const &_str) : str(_str) {}
  void clear() {
    str.clear();
    attributes.clear();
  }
  bool empty() const { return str.empty(); }
  bool operator==(const Text &txt) {
    if (str != txt.str || (attributes.size() != txt.attributes.size()))
      return false;
    for (size_t i = 0; i < attributes.size(); i++) {
      if ((attributes[i] != txt.attributes[i]))
        return false;
    }
    return true;
  }
  bool operator!=(const Text &txt) {
    if (str != txt.str || (attributes.size() != txt.attributes.size()))
      return true;
    for (size_t i = 0; i < attributes.size(); i++) {
      if ((attributes[i] != txt.attributes[i]))
        return true;
    }
    return false;
  }
  std::string str;
  std::vector<TextAttribute> attributes;
};

struct CandidateInfo {
  CandidateInfo() {
    currentPage = 0;
    totalPages = 0;
    highlighted = 0;
    is_last_page = false;
  }
  void clear() {
    currentPage = 0;
    totalPages = 0;
    highlighted = 0;
    is_last_page = false;
    candies.clear();
    labels.clear();
  }
  bool empty() const { return candies.empty(); }
  bool operator==(const CandidateInfo &ci) {
    if (currentPage != ci.currentPage || totalPages != ci.totalPages ||
        highlighted != ci.highlighted || is_last_page != ci.is_last_page ||
        notequal(candies, ci.candies) || notequal(comments, ci.comments) ||
        notequal(labels, ci.labels))
      return false;
    return true;
  }
  bool operator!=(const CandidateInfo &ci) {
    if (currentPage != ci.currentPage || totalPages != ci.totalPages ||
        highlighted != ci.highlighted || is_last_page != ci.is_last_page ||
        notequal(candies, ci.candies) || notequal(comments, ci.comments) ||
        notequal(labels, ci.labels))
      return true;
    return false;
  }
  bool notequal(std::vector<Text> txtSrc, std::vector<Text> txtDst) {
    if (txtSrc.size() != txtDst.size())
      return true;
    for (size_t i = 0; i < txtSrc.size(); i++) {
      if (txtSrc[i] != txtDst[i])
        return true;
    }
    return false;
  }
  int currentPage;
  bool is_last_page;
  int totalPages;
  int highlighted;
  std::vector<Text> candies;
  std::vector<Text> comments;
  std::vector<Text> labels;
};

struct Context {
  Context() {}
  void clear() {
    preedit.clear();
    aux.clear();
    cinfo.clear();
  }
  bool empty() const { return preedit.empty() && aux.empty() && cinfo.empty(); }
  bool operator==(const Context &ctx) {
    if (preedit == ctx.preedit && aux == ctx.aux && cinfo == ctx.cinfo)
      return true;
    return false;
  }
  bool operator!=(const Context &ctx) { return !(operator==(ctx)); }

  bool operator!() {
    if (preedit.str.empty() && aux.str.empty() && cinfo.candies.empty() &&
        cinfo.labels.empty() && cinfo.comments.empty())
      return true;
    else
      return false;
  }
  Text preedit;
  Text aux;
  CandidateInfo cinfo;
};

struct Status {
  Status()
      : ascii_mode(false), composing(false), disabled(false),
        full_shape(false) {}
  void reset() {
    schema_name.clear();
    schema_id.clear();
    ascii_mode = false;
    composing = false;
    disabled = false;
    full_shape = false;
  }
  bool operator==(const Status status) {
    return (status.schema_name == schema_name &&
            status.schema_id == schema_id && status.ascii_mode == ascii_mode &&
            status.composing == composing && status.disabled == disabled &&
            status.full_shape == full_shape);
  }
  bool operator!=(const Status status) {
    return (status.schema_name != schema_name ||
            status.schema_id != schema_id || status.ascii_mode != ascii_mode ||
            status.composing != composing || status.disabled != disabled ||
            status.full_shape != full_shape);
  }
  // 輸入方案
  std::string schema_name;
  // 輸入方案 id
  std::string schema_id;
  // 轉換開關
  bool ascii_mode;
  // 寫作狀態
  bool composing;
  // 維護模式（暫停輸入功能）
  bool disabled;
  // 全角状态
  bool full_shape;
};

namespace ibus {
// keycodes
enum Keycode {
  VoidSymbol = 0xFFFFFF,
  space = 0x020,
  grave = 0x060,
  BackSpace = 0xFF08,
  Tab = 0xFF09,
  Linefeed = 0xFF0A,
  Clear = 0xFF0B,
  Return = 0xFF0D,
  Pause = 0xFF13,
  Scroll_Lock = 0xFF14,
  Sys_Req = 0xFF15,
  Escape = 0xFF1B,
  Delete = 0xFFFF,
  Multi_key = 0xFF20,
  Codeinput = 0xFF37,
  SingleCandidate = 0xFF3C,
  MultipleCandidate = 0xFF3D,
  PreviousCandidate = 0xFF3E,
  Kanji = 0xFF21,
  Muhenkan = 0xFF22,
  Henkan_Mode = 0xFF23,
  Henkan = 0xFF23,
  Romaji = 0xFF24,
  Hiragana = 0xFF25,
  Katakana = 0xFF26,
  Hiragana_Katakana = 0xFF27,
  Zenkaku = 0xFF28,
  Hankaku = 0xFF29,
  Zenkaku_Hankaku = 0xFF2A,
  Touroku = 0xFF2B,
  Massyo = 0xFF2C,
  Kana_Lock = 0xFF2D,
  Kana_Shift = 0xFF2E,
  Eisu_Shift = 0xFF2F,
  Eisu_toggle = 0xFF30,
  Kanji_Bangou = 0xFF37,
  Zen_Koho = 0xFF3D,
  Mae_Koho = 0xFF3E,
  Home = 0xFF50,
  Left = 0xFF51,
  Up = 0xFF52,
  Right = 0xFF53,
  Down = 0xFF54,
  Prior = 0xFF55,
  Page_Up = 0xFF55,
  Next = 0xFF56,
  Page_Down = 0xFF56,
  End = 0xFF57,
  Begin = 0xFF58,
  Select = 0xFF60,
  Print = 0xFF61,
  Execute = 0xFF62,
  Insert = 0xFF63,
  Undo = 0xFF65,
  Redo = 0xFF66,
  Menu = 0xFF67,
  Find = 0xFF68,
  Cancel = 0xFF69,
  Help = 0xFF6A,
  Break = 0xFF6B,
  Mode_switch = 0xFF7E,
  script_switch = 0xFF7E,
  Num_Lock = 0xFF7F,
  KP_Space = 0xFF80,
  KP_Tab = 0xFF89,
  KP_Enter = 0xFF8D,
  KP_F1 = 0xFF91,
  KP_F2 = 0xFF92,
  KP_F3 = 0xFF93,
  KP_F4 = 0xFF94,
  KP_Home = 0xFF95,
  KP_Left = 0xFF96,
  KP_Up = 0xFF97,
  KP_Right = 0xFF98,
  KP_Down = 0xFF99,
  KP_Prior = 0xFF9A,
  KP_Page_Up = 0xFF9A,
  KP_Next = 0xFF9B,
  KP_Page_Down = 0xFF9B,
  KP_End = 0xFF9C,
  KP_Begin = 0xFF9D,
  KP_Insert = 0xFF9E,
  KP_Delete = 0xFF9F,
  KP_Equal = 0xFFBD,
  KP_Multiply = 0xFFAA,
  KP_Add = 0xFFAB,
  KP_Separator = 0xFFAC,
  KP_Subtract = 0xFFAD,
  KP_Decimal = 0xFFAE,
  KP_Divide = 0xFFAF,
  KP_0 = 0xFFB0,
  KP_1 = 0xFFB1,
  KP_2 = 0xFFB2,
  KP_3 = 0xFFB3,
  KP_4 = 0xFFB4,
  KP_5 = 0xFFB5,
  KP_6 = 0xFFB6,
  KP_7 = 0xFFB7,
  KP_8 = 0xFFB8,
  KP_9 = 0xFFB9,
  F1 = 0xFFBE,
  F2 = 0xFFBF,
  F3 = 0xFFC0,
  F4 = 0xFFC1,
  F5 = 0xFFC2,
  F6 = 0xFFC3,
  F7 = 0xFFC4,
  F8 = 0xFFC5,
  F9 = 0xFFC6,
  F10 = 0xFFC7,
  F11 = 0xFFC8,
  L1 = 0xFFC8,
  F12 = 0xFFC9,
  L2 = 0xFFC9,
  F13 = 0xFFCA,
  L3 = 0xFFCA,
  F14 = 0xFFCB,
  L4 = 0xFFCB,
  F15 = 0xFFCC,
  L5 = 0xFFCC,
  F16 = 0xFFCD,
  L6 = 0xFFCD,
  F17 = 0xFFCE,
  L7 = 0xFFCE,
  F18 = 0xFFCF,
  L8 = 0xFFCF,
  F19 = 0xFFD0,
  L9 = 0xFFD0,
  F20 = 0xFFD1,
  L10 = 0xFFD1,
  F21 = 0xFFD2,
  R1 = 0xFFD2,
  F22 = 0xFFD3,
  R2 = 0xFFD3,
  F23 = 0xFFD4,
  R3 = 0xFFD4,
  F24 = 0xFFD5,
  R4 = 0xFFD5,
  F25 = 0xFFD6,
  R5 = 0xFFD6,
  F26 = 0xFFD7,
  R6 = 0xFFD7,
  F27 = 0xFFD8,
  R7 = 0xFFD8,
  F28 = 0xFFD9,
  R8 = 0xFFD9,
  F29 = 0xFFDA,
  R9 = 0xFFDA,
  F30 = 0xFFDB,
  R10 = 0xFFDB,
  F31 = 0xFFDC,
  R11 = 0xFFDC,
  F32 = 0xFFDD,
  R12 = 0xFFDD,
  F33 = 0xFFDE,
  R13 = 0xFFDE,
  F34 = 0xFFDF,
  R14 = 0xFFDF,
  F35 = 0xFFE0,
  R15 = 0xFFE0,
  Shift_L = 0xFFE1,
  Shift_R = 0xFFE2,
  Control_L = 0xFFE3,
  Control_R = 0xFFE4,
  Caps_Lock = 0xFFE5,
  Shift_Lock = 0xFFE6,
  Meta_L = 0xFFE7,
  Meta_R = 0xFFE8,
  Alt_L = 0xFFE9,
  Alt_R = 0xFFEA,
  Super_L = 0xFFEB,
  Super_R = 0xFFEC,
  Hyper_L = 0xFFED,
  Hyper_R = 0xFFEE,
  Null = 0
};
// modifiers, modified to fit a UINT16
enum Modifier {
  NULL_MASK = 0,

  SHIFT_MASK = 1 << 0,
  LOCK_MASK = 1 << 1,
  CONTROL_MASK = 1 << 2,
  ALT_MASK = 1 << 3,
  MOD1_MASK = 1 << 3,
  MOD2_MASK = 1 << 4,
  MOD3_MASK = 1 << 5,
  MOD4_MASK = 1 << 6,
  MOD5_MASK = 1 << 7,

  HANDLED_MASK = 1 << 8, // 24
  IGNORED_MASK = 1 << 9, // 25
  FORWARD_MASK = 1 << 9, // 25

  SUPER_MASK = 1 << 10, // 26
  HYPER_MASK = 1 << 11, // 27
  META_MASK = 1 << 12,  // 28

  RELEASE_MASK = 1 << 14, // 30

  MODIFIER_MASK = 0x2fff
};

} // namespace ibus

struct KeyEvent {
  UINT keycode : 16;
  UINT mask : 16;
  KeyEvent() : keycode(0), mask(0) {}
  KeyEvent(UINT _keycode, UINT _mask) : keycode(_keycode), mask(_mask) {}
  KeyEvent(UINT x) { *reinterpret_cast<UINT *>(this) = x; }
  operator UINT32 const() const {
    return *reinterpret_cast<UINT32 const *>(this);
  }
};

struct KeyInfo {
  UINT repeatCount : 16;
  UINT scanCode : 8;
  UINT isExtended : 1;
  UINT reserved : 4;
  UINT contextCode : 1;
  UINT prevKeyState : 1;
  UINT isKeyUp : 1;
  KeyInfo(LPARAM lparam) { *this = *reinterpret_cast<KeyInfo *>(&lparam); }
  operator UINT32() { return *reinterpret_cast<UINT32 *>(this); }
};

enum IconType { SCHEMA, FULL_SHAPE };
// 用於向前端告知設置信息
struct Config {
  Config() : inline_preedit(false) {}
  void reset() { inline_preedit = false; }
  bool inline_preedit;
};

struct UIStyle {
  enum AntiAliasMode {
    DEFAULT = 0,
    CLEARTYPE = 1,
    GRAYSCALE = 2,
    ALIASED = 3,
    FORCE_DWORD = 0xffffffff
  };

  enum PreeditType { COMPOSITION, PREVIEW, PREVIEW_ALL };
  enum HoverType { NONE, SEMI_HILITE, HILITE };
  enum LayoutType {
    LAYOUT_VERTICAL = 0,
    LAYOUT_HORIZONTAL,
    LAYOUT_VERTICAL_TEXT,
    LAYOUT_VERTICAL_FULLSCREEN,
    LAYOUT_HORIZONTAL_FULLSCREEN,
    LAYOUT_TYPE_LAST
  };

  enum LayoutAlignType { ALIGN_BOTTOM = 0, ALIGN_CENTER, ALIGN_TOP };

  // font face and font point settings
  std::wstring font_face;
  std::wstring label_font_face;
  std::wstring comment_font_face;
  int font_point;
  int label_font_point;
  int comment_font_point;
  int candidate_abbreviate_length;

  bool inline_preedit;
  bool display_tray_icon;
  bool ascii_tip_follow_cursor;
  bool paging_on_scroll;
  bool enhanced_position;
  bool click_to_capture;
  HoverType hover_type;
  AntiAliasMode antialias_mode;
  PreeditType preedit_type;
  // custom icon settings
  std::wstring current_zhung_icon;
  std::wstring current_ascii_icon;
  std::wstring current_half_icon;
  std::wstring current_full_icon;
  // label format and mark_text
  std::wstring label_text_format;
  std::wstring mark_text;
  // layout relative parameters
  LayoutType layout_type;
  LayoutAlignType align_type;
  bool vertical_text_left_to_right;
  bool vertical_text_with_wrap;
  // layout, with key name like style/layout/...
  int min_width;
  int max_width;
  int min_height;
  int max_height;
  int border;
  int margin_x;
  int margin_y;
  int spacing;
  int candidate_spacing;
  int hilite_spacing;
  int hilite_padding_x;
  int hilite_padding_y;
  int round_corner;
  int round_corner_ex;
  int shadow_radius;
  int shadow_offset_x;
  int shadow_offset_y;
  bool vertical_auto_reverse;
  // color scheme
  int text_color;
  int candidate_text_color;
  int candidate_back_color;
  int candidate_shadow_color;
  int candidate_border_color;
  int label_text_color;
  int comment_text_color;
  int back_color;
  int shadow_color;
  int border_color;
  int hilited_text_color;
  int hilited_back_color;
  int hilited_shadow_color;
  int hilited_candidate_text_color;
  int hilited_candidate_back_color;
  int hilited_candidate_shadow_color;
  int hilited_candidate_border_color;
  int hilited_label_text_color;
  int hilited_comment_text_color;
  int hilited_mark_color;
  int prevpage_color;
  int nextpage_color;
  // per client
  int client_caps;
  int baseline;
  int linespacing;

  UIStyle()
      : font_face(), label_font_face(), comment_font_face(), font_point(0),
        label_font_point(0), comment_font_point(0),
        candidate_abbreviate_length(0), inline_preedit(false),
        display_tray_icon(false), ascii_tip_follow_cursor(false),
        paging_on_scroll(false), enhanced_position(false),
        click_to_capture(false), hover_type(NONE), antialias_mode(DEFAULT),
        preedit_type(COMPOSITION), current_zhung_icon(), current_ascii_icon(),
        current_half_icon(), current_full_icon(), label_text_format(L"%s."),
        mark_text(), layout_type(LAYOUT_VERTICAL), align_type(ALIGN_BOTTOM),
        vertical_text_left_to_right(false), vertical_text_with_wrap(false),
        min_width(0), max_width(0), min_height(0), max_height(0), border(0),
        margin_x(0), margin_y(0), spacing(0), candidate_spacing(0),
        hilite_spacing(0), hilite_padding_x(0), hilite_padding_y(0),
        round_corner(0), round_corner_ex(0), shadow_radius(0),
        shadow_offset_x(0), shadow_offset_y(0), vertical_auto_reverse(false),
        text_color(0), candidate_text_color(0), candidate_back_color(0),
        candidate_shadow_color(0), candidate_border_color(0),
        label_text_color(0), comment_text_color(0), back_color(0),
        shadow_color(0), border_color(0), hilited_text_color(0),
        hilited_back_color(0), hilited_shadow_color(0),
        hilited_candidate_text_color(0), hilited_candidate_back_color(0),
        hilited_candidate_shadow_color(0), hilited_candidate_border_color(0),
        hilited_label_text_color(0), hilited_comment_text_color(0),
        hilited_mark_color(0), prevpage_color(0), nextpage_color(0),
        baseline(0), linespacing(0), client_caps(0) {}
  bool operator!=(const UIStyle &st) {
    return (
        align_type != st.align_type || antialias_mode != st.antialias_mode ||
        preedit_type != st.preedit_type || layout_type != st.layout_type ||
        vertical_text_left_to_right != st.vertical_text_left_to_right ||
        vertical_text_with_wrap != st.vertical_text_with_wrap ||
        paging_on_scroll != st.paging_on_scroll || font_face != st.font_face ||
        label_font_face != st.label_font_face ||
        comment_font_face != st.comment_font_face ||
        hover_type != st.hover_type || font_point != st.font_point ||
        label_font_point != st.label_font_point ||
        comment_font_point != st.comment_font_point ||
        candidate_abbreviate_length != st.candidate_abbreviate_length ||
        inline_preedit != st.inline_preedit || mark_text != st.mark_text ||
        display_tray_icon != st.display_tray_icon ||
        ascii_tip_follow_cursor != st.ascii_tip_follow_cursor ||
        current_zhung_icon != st.current_zhung_icon ||
        current_ascii_icon != st.current_ascii_icon ||
        current_half_icon != st.current_half_icon ||
        current_full_icon != st.current_full_icon ||
        enhanced_position != st.enhanced_position ||
        click_to_capture != st.click_to_capture ||
        label_text_format != st.label_text_format ||
        min_width != st.min_width || max_width != st.max_width ||
        min_height != st.min_height || max_height != st.max_height ||
        border != st.border || margin_x != st.margin_x ||
        margin_y != st.margin_y || spacing != st.spacing ||
        candidate_spacing != st.candidate_spacing ||
        hilite_spacing != st.hilite_spacing ||
        hilite_padding_x != st.hilite_padding_x ||
        hilite_padding_y != st.hilite_padding_y ||
        round_corner != st.round_corner ||
        round_corner_ex != st.round_corner_ex ||
        shadow_radius != st.shadow_radius ||
        shadow_offset_x != st.shadow_offset_x ||
        shadow_offset_y != st.shadow_offset_y ||
        vertical_auto_reverse != st.vertical_auto_reverse ||
        baseline != st.baseline || linespacing != st.linespacing ||
        text_color != st.text_color ||
        candidate_text_color != st.candidate_text_color ||
        candidate_back_color != st.candidate_back_color ||
        candidate_shadow_color != st.candidate_shadow_color ||
        candidate_border_color != st.candidate_border_color ||
        hilited_candidate_border_color != st.hilited_candidate_border_color ||
        label_text_color != st.label_text_color ||
        comment_text_color != st.comment_text_color ||
        back_color != st.back_color || shadow_color != st.shadow_color ||
        border_color != st.border_color ||
        hilited_text_color != st.hilited_text_color ||
        hilited_back_color != st.hilited_back_color ||
        hilited_shadow_color != st.hilited_shadow_color ||
        hilited_candidate_text_color != st.hilited_candidate_text_color ||
        hilited_candidate_back_color != st.hilited_candidate_back_color ||
        hilited_candidate_shadow_color != st.hilited_candidate_shadow_color ||
        hilited_label_text_color != st.hilited_label_text_color ||
        hilited_comment_text_color != st.hilited_comment_text_color ||
        hilited_mark_color != st.hilited_mark_color ||
        prevpage_color != st.prevpage_color ||
        nextpage_color != st.nextpage_color);
  }
};
} // namespace weasel

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
#define wtou8(x) wstring_to_string(x, CP_UTF8)
#define wtoacp(x) wstring_to_string(x)
#define u8tow(x) string_to_wstring(x, CP_UTF8)
#define MAX(x, y) ((x > y) ? x : y)
