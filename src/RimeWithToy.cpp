#include "RimeWithToy.h"
#include "key_table.h"
#include <filesystem>
#include <map>
#include <regex>
#include <resource.h>

#define TRANSPARENT_COLOR 0x00000000
#define ARGB2ABGR(value)                                                       \
  ((value & 0xff000000) | ((value & 0x000000ff) << 16) |                       \
   (value & 0x0000ff00) | ((value & 0x00ff0000) >> 16))
#define RGBA2ABGR(value)                                                       \
  (((value & 0xff) << 24) | ((value & 0xff000000) >> 24) |                     \
   ((value & 0x00ff0000) >> 8) | ((value & 0x0000ff00) << 8))
typedef enum { COLOR_ABGR = 0, COLOR_ARGB, COLOR_RGBA } ColorFormat;

#ifdef USE_SHARP_COLOR_CODE
#define HEX_REGEX std::regex("^(0x|#)[0-9a-f]+$", std::regex::icase)
#define TRIMHEAD_REGEX std::regex("0x|#", std::regex::icase)
#else
#define HEX_REGEX std::regex("^0x[0-9a-f]+$", std::regex::icase)
#define TRIMHEAD_REGEX std::regex("0x", std::regex::icase)
#endif
namespace fs = std::filesystem;

int expand_ibus_modifier(int m) { return (m & 0xff) | ((m & 0xff00) << 16); }

namespace weasel {

static fs::path data_path(string subdir) {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append(subdir);
}

string RimeWithToy::m_message_type;
string RimeWithToy::m_message_value;
string RimeWithToy::m_message_label;
string RimeWithToy::m_option_name;

void RimeWithToy::setup_rime() {
  RIME_STRUCT(RimeTraits, traits);
  auto shared_path = data_path("shared");
  auto usr_path = data_path("usr");
  auto log_path = data_path("log");
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
  rime_api->setup(&traits);
  rime_api->set_notification_handler(&on_message, nullptr);
}

void RimeWithToy::on_message(void *context_object, RimeSessionId session_id,
                             const char *message_type,
                             const char *message_value) {
  // may be running in a thread when deploying rime
  // RimeWithToy* self =
  //    reinterpret_cast<RimeWithToy*>(context_object);
  // if (!self || !message_type || !message_value)
  //  return;
  m_message_type = message_type;
  m_message_value = message_value;
  RimeApi *rime_api = rime_get_api();
  if (RIME_API_AVAILABLE(rime_api, get_state_label) &&
      !strcmp(message_type, "option")) {
    Bool state = message_value[0] != '!';
    const char *option_name = message_value + !state;
    m_option_name = option_name;
    const char *state_label =
        rime_api->get_state_label(session_id, option_name, state);
    if (state_label) {
      m_message_label = string(state_label);
    }
  }
}

RimeWithToy::RimeWithToy(UI *ui, HINSTANCE hInstance) : m_ui(ui) {
  Initialize();
  m_ime_icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN));
  m_ascii_icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_ASCII));
  const auto tooltip = L"rime.toy\n左键点击切换ASCII\n右键菜单可退出^_^";
  m_trayIcon = std::make_unique<TrayIcon>(hInstance, tooltip);
  m_trayIcon->SetDeployFunc([&]() {
    DEBUG << L"Deploy Menu clicked";
    Initialize();
  });
  m_trayIcon->SetSwichAsciiFunc([&]() { SwitchAsciiMode(); });
  m_trayIcon->SetIcon(m_ime_icon);
  m_trayIcon->Show();
  m_trayIconCallback = [&](const Status &sta) {
    m_trayIcon->SetIcon(sta.ascii_mode ? m_ascii_icon : m_ime_icon);
  };
}

void RimeWithToy::Initialize() {
  DEBUG << L"RimeWithToy::Initialize() called";
  setup_rime();
  rime_api = rime_get_api();
  rime_api->initialize(NULL);
  if (rime_api->start_maintenance(true))
    rime_api->join_maintenance_thread();
  rime_api->deploy_config_file("weasel.yaml", "config_version");
  m_session_id = rime_api->create_session();
  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    _UpdateUIStyle(&config, m_ui, true);
    rime_api->config_close(&config);
  } else
    DEBUG << L"open weasel config failed";
  rime_api->set_option(m_session_id, "soft_cursor",
                       Bool(!m_ui->style().inline_preedit));
}

void RimeWithToy::Finalize() {
  DEBUG << L"RimeWithToy::Finalize() called";
  rime_api->destroy_session(m_session_id);
  rime_api->finalize();
}

void RimeWithToy::SwitchAsciiMode() {
  DEBUG << L"RimeWithToy::SwitchAsciiMode() called";
  BOOL ascii = rime_api->get_option(m_session_id, "ascii_mode");
  rime_api->set_option(m_session_id, "ascii_mode", !ascii);
  Status status;
  GetStatus(status);
  if (m_trayIconCallback)
    m_trayIconCallback(status);
}

BOOL RimeWithToy::ProcessKeyEvent(KeyEvent keyEvent, wstring &commit_str) {
  // DEBUG << L"keyEvent.keycode = " << std::hex << keyEvent.keycode
  //       << L", keyEvent.mask = " << std::hex << keyEvent.mask;
  auto reprstr = repr(keyEvent.keycode, expand_ibus_modifier(keyEvent.mask));
  DEBUG << "RimeWithToy::ProcessKeyEvent " << reprstr;
  Bool handled = rime_api->process_key(m_session_id, keyEvent.keycode,
                                       expand_ibus_modifier(keyEvent.mask));
  RIME_STRUCT(RimeCommit, commit);
  if (rime_api->get_commit(m_session_id, &commit)) {
    commit_str = u8tow(commit.text);
    rime_api->free_commit(&commit);
  } else {
    commit_str.clear();
  }
  UpdateUI();
  return handled;
}

void RimeWithToy::UpdateUI() {
  if (!m_ui)
    return;
  Status &status = m_ui->status();
  Context ctx;
  GetStatus(status);
  GetContext(ctx);
  if (status.composing) {
    m_ui->Update(ctx, status);
    m_ui->Show();
  } else if (!ShowMessage(ctx, status)) {
    m_ui->Hide();
    m_ui->Update(ctx, status);
  }

  if (m_trayIconCallback)
    m_trayIconCallback(status);

  m_message_type.clear();
  m_message_value.clear();
  m_message_label.clear();
  m_option_name.clear();
}

BOOL RimeWithToy::ShowMessage(Context &ctx, Status &status) {
  // show as auxiliary string
  std::wstring &tips(ctx.aux.str);
  if (m_message_type == "deploy") {
    if (m_message_value == "start")
      tips = L"正在部署";
    else if (m_message_value == "success")
      tips = L"部署完成";
    else if (m_message_value == "failure")
      tips = L"有错误，请查看日志 %TEMP%\\rime.toy\\rime.toy.*.INFO";
  } else if (m_message_type == "option") {
    if (!m_message_label.empty())
      tips = string_to_wstring(m_message_label, CP_UTF8);
    if (m_message_value == "!ascii_mode") {
      tips = L"中文";
    } else if (m_message_value == "ascii_mode") {
      tips = L"英文";
    }
  }

  if (tips.empty())
    return m_ui->IsCountingDown();
  m_ui->Update(ctx, status);
  m_ui->ShowWithTimeout(500);
  return true;
}

void RimeWithToy::GetCandidateInfo(CandidateInfo &cinfo, RimeContext &ctx) {
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
      cinfo.labels[i].str = std::to_wstring((i + 1) % 10);
    }
  }
  cinfo.highlighted = ctx.menu.highlighted_candidate_index;
  cinfo.currentPage = ctx.menu.page_no;
  cinfo.is_last_page = ctx.menu.is_last_page;
}

void RimeWithToy::GetStatus(Status &status) {
  RIME_STRUCT(RimeStatus, status_);
  if (rime_api->get_status(m_session_id, &status_)) {
    status.ascii_mode = !!status_.is_ascii_mode;
    status.composing = !!status_.is_composing;
    status.disabled = !!status_.is_disabled;
    status.full_shape = !!status_.is_full_shape;
    status.schema_id = u8tow(status_.schema_id);
    status.schema_name = u8tow(status_.schema_name);
    rime_api->free_status(&status_);
  }
}

void RimeWithToy::GetContext(Context &context) {
  RIME_STRUCT(RimeContext, ctx);
  if (rime_api->get_context(m_session_id, &ctx)) {
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
      GetCandidateInfo(cinfo, ctx);
    }
    rime_api->free_context(&ctx);
  }
}

// ----------------------------------------------------------------------------

static inline COLORREF blend_colors(COLORREF fcolor, COLORREF bcolor) {
  return RGB((GetRValue(fcolor) * 2 + GetRValue(bcolor)) / 3,
             (GetGValue(fcolor) * 2 + GetGValue(bcolor)) / 3,
             (GetBValue(fcolor) * 2 + GetBValue(bcolor)) / 3) |
         ((((fcolor >> 24) + (bcolor >> 24) / 2) << 24));
}

// convertions from color format to COLOR_ABGR
static inline int ConvertColorToAbgr(int color, ColorFormat fmt = COLOR_ABGR) {
  if (fmt == COLOR_ABGR)
    return color & 0xffffffff;
  else if (fmt == COLOR_ARGB)
    return ARGB2ABGR(color) & 0xffffffff;
  else
    return RGBA2ABGR(color) & 0xffffffff;
}
// parse color value, with fallback value
static Bool _RimeConfigGetColor32bWithFallback(RimeConfig *config,
                                               const string key, int &value,
                                               const ColorFormat &fmt,
                                               const unsigned int &fallback) {
  RimeApi *rime_api = rime_get_api();
  char color[256] = {0};
  if (!rime_api->config_get_string(config, key.c_str(), color, 256)) {
    value = fallback;
    return False;
  }

  string color_str = string(color);
  auto alpha = [&](int &value) {
    value = (fmt != COLOR_RGBA) ? (value | 0xff000000)
                                : ((value << 8) | 0x000000ff);
  };
  if (std::regex_match(color_str, HEX_REGEX)) {
    string tmp = std::regex_replace(color_str, TRIMHEAD_REGEX, "").substr(0, 8);
    switch (tmp.length()) {
    case 6: // color code without alpha, xxyyzz add alpha ff
      value = std::stoul(tmp, 0, 16);
      alpha(value);
      break;
    case 3: // color hex code xyz => xxyyzz and alpha ff
      tmp = string(2, tmp[0]) + string(2, tmp[1]) + string(2, tmp[2]);
      value = std::stoul(tmp, 0, 16);
      alpha(value);
      break;
    case 4: // color hex code vxyz => vvxxyyzz
      tmp = string(2, tmp[0]) + string(2, tmp[1]) + string(2, tmp[2]) +
            string(2, tmp[3]);
      value = std::stoul(tmp, 0, 16);
      alpha(value);
      break;
    case 7:
    case 8: // color code with alpha
      value = std::stoul(tmp, 0, 16);
      break;
    default: // invalid length
      value = fallback;
      return False;
    }
    value = ConvertColorToAbgr(value, fmt);
    return True;
  } else {
    int tmp = 0;
    if (!rime_api->config_get_int(config, key.c_str(), &tmp)) {
      value = fallback;
      return False;
    }
    alpha(value);
    value = ConvertColorToAbgr(value, fmt);
    return True;
  }
}
// for remove useless spaces around seperators, begining and ending
static inline void _RemoveSpaceAroundSep(std::wstring &str) {
  str = std::regex_replace(str, std::wregex(L"\\s*(,|:|^|$)\\s*"), L"$1");
}
// parset bool type configuration to T type value trueValue / falseValue
template <class T>
static void _RimeGetBool(RimeConfig *config, const char *key, bool cond,
                         T &value, const T &trueValue, const T &falseValue) {
  RimeApi *rime_api = rime_get_api();
  Bool tempb = False;
  if (rime_api->config_get_bool(config, key, &tempb) || cond)
    value = (!!tempb) ? trueValue : falseValue;
}
//	parse string option to T type value, with fallback
template <typename T>
void _RimeParseStringOptWithFallback(RimeConfig *config, const string key,
                                     T &value, const std::map<string, T> amap,
                                     const T &fallback) {
  RimeApi *rime_api = rime_get_api();
  char str_buff[256] = {0};
  if (rime_api->config_get_string(config, key.c_str(), str_buff,
                                  sizeof(str_buff) - 1)) {
    auto it = amap.find(string(str_buff));
    value = (it != amap.end()) ? it->second : fallback;
  } else
    value = fallback;
}
static inline void _abs(int *value) {
  *value = abs(*value);
} // turn *value to be non-negative
// get int type value with fallback key fb_key, and func to execute after
// reading
static void _RimeGetIntWithFallback(RimeConfig *config, const char *key,
                                    int *value, const char *fb_key = NULL,
                                    std::function<void(int *)> func = NULL) {
  RimeApi *rime_api = rime_get_api();
  if (!rime_api->config_get_int(config, key, value) && fb_key != NULL) {
    rime_api->config_get_int(config, fb_key, value);
  }
  if (func)
    func(value);
}
// get string value, with fallback value *fallback, and func to execute after
// reading
static void
_RimeGetStringWithFunc(RimeConfig *config, const char *key, std::wstring &value,
                       const std::wstring *fallback = NULL,
                       const std::function<void(std::wstring &)> func = NULL) {
  RimeApi *rime_api = rime_get_api();
  const int BUF_SIZE = 2047;
  char buffer[BUF_SIZE + 1] = {0};
  if (rime_api->config_get_string(config, key, buffer, BUF_SIZE)) {
    std::wstring tmp = string_to_wstring(buffer, CP_UTF8);
    if (func)
      func(tmp);
    value = tmp;
  } else if (fallback)
    value = *fallback;
}

// load color configs to style, by "style/color_scheme" or specific scheme name
// "color" which is default empty
bool _UpdateUIStyleColor(RimeConfig *config, UIStyle &style, string color) {
  RimeApi *rime_api = rime_get_api();
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  string color_mark = "style/color_scheme";
  // color scheme
  if (rime_api->config_get_string(config, color_mark.c_str(), buffer,
                                  BUF_SIZE) ||
      !color.empty()) {
    string prefix("preset_color_schemes/");
    prefix += (color.empty()) ? buffer : color;
    // define color format, default abgr if not set
    ColorFormat fmt = COLOR_ABGR;
    const std::map<string, ColorFormat> _colorFmt = {
        {string("argb"), COLOR_ARGB},
        {string("rgba"), COLOR_RGBA},
        {string("abgr"), COLOR_ABGR}};
    _RimeParseStringOptWithFallback(config, (prefix + "/color_format"), fmt,
                                    _colorFmt, COLOR_ABGR);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/back_color"),
                                       style.back_color, fmt, 0xffffffff);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/shadow_color"),
                                       style.shadow_color, fmt,
                                       TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/prevpage_color"),
                                       style.prevpage_color, fmt,
                                       TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/nextpage_color"),
                                       style.nextpage_color, fmt,
                                       TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/text_color"),
                                       style.text_color, fmt, 0xff000000);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/candidate_text_color"), style.candidate_text_color,
        fmt, style.text_color);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/candidate_back_color"), style.candidate_back_color,
        fmt, TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/border_color"),
                                       style.border_color, fmt,
                                       style.text_color);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/hilited_text_color"),
                                       style.hilited_text_color, fmt,
                                       style.text_color);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/hilited_back_color"),
                                       style.hilited_back_color, fmt,
                                       style.back_color);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_candidate_text_color"),
        style.hilited_candidate_text_color, fmt, style.hilited_text_color);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_candidate_back_color"),
        style.hilited_candidate_back_color, fmt, style.hilited_back_color);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_candidate_shadow_color"),
        style.hilited_candidate_shadow_color, fmt, TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_shadow_color"), style.hilited_shadow_color,
        fmt, TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/candidate_shadow_color"),
        style.candidate_shadow_color, fmt, TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/candidate_border_color"),
        style.candidate_border_color, fmt, TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_candidate_border_color"),
        style.hilited_candidate_border_color, fmt, TRANSPARENT_COLOR);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/label_color"), style.label_text_color, fmt,
        blend_colors(style.candidate_text_color, style.candidate_back_color));
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_label_color"),
        style.hilited_label_text_color, fmt,
        blend_colors(style.hilited_candidate_text_color,
                     style.hilited_candidate_back_color));
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/comment_text_color"),
                                       style.comment_text_color, fmt,
                                       style.label_text_color);
    _RimeConfigGetColor32bWithFallback(
        config, (prefix + "/hilited_comment_text_color"),
        style.hilited_comment_text_color, fmt, style.hilited_label_text_color);
    _RimeConfigGetColor32bWithFallback(config, (prefix + "/hilited_mark_color"),
                                       style.hilited_mark_color, fmt,
                                       TRANSPARENT_COLOR);
    return true;
  }
  return false;
}

// update ui's style parameters, ui has been check before referenced
void _UpdateUIStyle(RimeConfig *config, UI *ui, bool initialize) {
  RimeApi *rime_api = rime_get_api();
  UIStyle &style(ui->style());
  // get font faces
  _RimeGetStringWithFunc(config, "style/font_face", style.font_face, NULL,
                         _RemoveSpaceAroundSep);
  std::wstring *const pFallbackFontFace = initialize ? &style.font_face : NULL;
  _RimeGetStringWithFunc(config, "style/label_font_face", style.label_font_face,
                         pFallbackFontFace, _RemoveSpaceAroundSep);
  _RimeGetStringWithFunc(config, "style/comment_font_face",
                         style.comment_font_face, pFallbackFontFace,
                         _RemoveSpaceAroundSep);
  // able to set label font/comment font empty, force fallback to font face.
  if (style.label_font_face.empty())
    style.label_font_face = style.font_face;
  if (style.comment_font_face.empty())
    style.comment_font_face = style.font_face;
  // get font points
  _RimeGetIntWithFallback(config, "style/font_point", &style.font_point);
  if (style.font_point <= 0)
    style.font_point = 12;
  _RimeGetIntWithFallback(config, "style/label_font_point",
                          &style.label_font_point, "style/font_point", _abs);
  _RimeGetIntWithFallback(config, "style/comment_font_point",
                          &style.comment_font_point, "style/font_point", _abs);
  _RimeGetIntWithFallback(config, "style/candidate_abbreviate_length",
                          &style.candidate_abbreviate_length, NULL, _abs);
  _RimeGetBool(config, "style/inline_preedit", initialize, style.inline_preedit,
               true, false);
  _RimeGetBool(config, "style/vertical_auto_reverse", initialize,
               style.vertical_auto_reverse, true, false);
  const std::map<string, UIStyle::PreeditType> _preeditMap = {
      {string("composition"), UIStyle::COMPOSITION},
      {string("preview"), UIStyle::PREVIEW},
      {string("preview_all"), UIStyle::PREVIEW_ALL}};
  _RimeParseStringOptWithFallback(config, "style/preedit_type",
                                  style.preedit_type, _preeditMap,
                                  style.preedit_type);
  const std::map<string, UIStyle::AntiAliasMode> _aliasModeMap = {
      {string("force_dword"), UIStyle::FORCE_DWORD},
      {string("cleartype"), UIStyle::CLEARTYPE},
      {string("grayscale"), UIStyle::GRAYSCALE},
      {string("aliased"), UIStyle::ALIASED},
      {string("default"), UIStyle::DEFAULT}};
  _RimeParseStringOptWithFallback(config, "style/antialias_mode",
                                  style.antialias_mode, _aliasModeMap,
                                  style.antialias_mode);
  const std::map<string, UIStyle::HoverType> _hoverTypeMap = {
      {string("none"), UIStyle::HoverType::NONE},
      {string("semi_hilite"), UIStyle::HoverType::SEMI_HILITE},
      {string("hilite"), UIStyle::HoverType::HILITE}};
  _RimeParseStringOptWithFallback(config, "style/hover_type", style.hover_type,
                                  _hoverTypeMap, style.hover_type);
  const std::map<string, UIStyle::LayoutAlignType> _alignType = {
      {string("top"), UIStyle::ALIGN_TOP},
      {string("center"), UIStyle::ALIGN_CENTER},
      {string("bottom"), UIStyle::ALIGN_BOTTOM}};
  _RimeParseStringOptWithFallback(config, "style/layout/align_type",
                                  style.align_type, _alignType,
                                  style.align_type);
  _RimeGetBool(config, "style/display_tray_icon", initialize,
               style.display_tray_icon, true, false);
  _RimeGetBool(config, "style/ascii_tip_follow_cursor", initialize,
               style.ascii_tip_follow_cursor, true, false);
  _RimeGetBool(config, "style/horizontal", initialize, style.layout_type,
               UIStyle::LAYOUT_HORIZONTAL, UIStyle::LAYOUT_VERTICAL);
  _RimeGetBool(config, "style/paging_on_scroll", initialize,
               style.paging_on_scroll, true, false);
  _RimeGetBool(config, "style/click_to_capture", initialize,
               style.click_to_capture, true, false);
  _RimeGetBool(config, "style/fullscreen", false, style.layout_type,
               ((style.layout_type == UIStyle::LAYOUT_HORIZONTAL)
                    ? UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN
                    : UIStyle::LAYOUT_VERTICAL_FULLSCREEN),
               style.layout_type);
  _RimeGetBool(config, "style/vertical_text", false, style.layout_type,
               UIStyle::LAYOUT_VERTICAL_TEXT, style.layout_type);
  _RimeGetBool(config, "style/vertical_text_left_to_right", false,
               style.vertical_text_left_to_right, true, false);
  _RimeGetBool(config, "style/vertical_text_with_wrap", false,
               style.vertical_text_with_wrap, true, false);
  const std::map<string, bool> _text_orientation = {
      {string("horizontal"), false}, {string("vertical"), true}};
  bool _text_orientation_bool = false;
  _RimeParseStringOptWithFallback(config, "style/text_orientation",
                                  _text_orientation_bool, _text_orientation,
                                  _text_orientation_bool);
  if (_text_orientation_bool)
    style.layout_type = UIStyle::LAYOUT_VERTICAL_TEXT;
  _RimeGetStringWithFunc(config, "style/label_format", style.label_text_format);
  _RimeGetStringWithFunc(config, "style/mark_text", style.mark_text);
  _RimeGetIntWithFallback(config, "style/layout/baseline", &style.baseline,
                          NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/linespacing",
                          &style.linespacing, NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/min_width", &style.min_width,
                          NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/max_width", &style.max_width,
                          NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/min_height", &style.min_height,
                          NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/max_height", &style.max_height,
                          NULL, _abs);
  // layout (alternative to style/horizontal)
  const std::map<string, UIStyle::LayoutType> _layoutMap = {
      {string("vertical"), UIStyle::LAYOUT_VERTICAL},
      {string("horizontal"), UIStyle::LAYOUT_HORIZONTAL},
      {string("vertical_text"), UIStyle::LAYOUT_VERTICAL_TEXT},
      {string("vertical+fullscreen"), UIStyle::LAYOUT_VERTICAL_FULLSCREEN},
      {string("horizontal+fullscreen"), UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN}};
  _RimeParseStringOptWithFallback(config, "style/layout/type",
                                  style.layout_type, _layoutMap,
                                  style.layout_type);
  // disable max_width when full screen
  if (style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN ||
      style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN) {
    style.max_width = 0;
    style.inline_preedit = false;
  }
  _RimeGetIntWithFallback(config, "style/layout/border", &style.border,
                          "style/layout/border_width", _abs);
  _RimeGetIntWithFallback(config, "style/layout/margin_x", &style.margin_x);
  _RimeGetIntWithFallback(config, "style/layout/margin_y", &style.margin_y);
  _RimeGetIntWithFallback(config, "style/layout/spacing", &style.spacing, NULL,
                          _abs);
  _RimeGetIntWithFallback(config, "style/layout/candidate_spacing",
                          &style.candidate_spacing, NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/hilite_spacing",
                          &style.hilite_spacing, NULL, _abs);
  _RimeGetIntWithFallback(config, "style/layout/hilite_padding_x",
                          &style.hilite_padding_x,
                          "style/layout/hilite_padding", _abs);
  _RimeGetIntWithFallback(config, "style/layout/hilite_padding_y",
                          &style.hilite_padding_y,
                          "style/layout/hilite_padding", _abs);
  _RimeGetIntWithFallback(config, "style/layout/shadow_radius",
                          &style.shadow_radius, NULL, _abs);
  // disable shadow for fullscreen layout
  style.shadow_radius *=
      (!(style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN ||
         style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN));
  _RimeGetIntWithFallback(config, "style/layout/shadow_offset_x",
                          &style.shadow_offset_x);
  _RimeGetIntWithFallback(config, "style/layout/shadow_offset_y",
                          &style.shadow_offset_y);
  // round_corner as alias of hilited_corner_radius
  _RimeGetIntWithFallback(config, "style/layout/hilited_corner_radius",
                          &style.round_corner, "style/layout/round_corner",
                          _abs);
  // corner_radius not set, fallback to round_corner
  _RimeGetIntWithFallback(config, "style/layout/corner_radius",
                          &style.round_corner_ex, "style/layout/round_corner",
                          _abs);
  // fix padding and spacing settings
  if (style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT) {
    // hilite_padding vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = MAX(style.spacing, style.hilite_padding_y * 2);
    // hilite_padding vs candidate_spacing
    if (style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN ||
        style.layout_type == UIStyle::LAYOUT_VERTICAL) {
      // vertical, if hilite_padding_y over candidate spacing,
      // increase candidate spacing
      style.candidate_spacing =
          MAX(style.candidate_spacing, style.hilite_padding_y * 2);
    } else {
      // horizontal, if hilite_padding_x over candidate
      // spacing, increase candidate spacing
      style.candidate_spacing =
          MAX(style.candidate_spacing, style.hilite_padding_x * 2);
    }
    // hilite_padding_x vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing = MAX(style.hilite_spacing, style.hilite_padding_x);
  } else // LAYOUT_VERTICAL_TEXT
  {
    // hilite_padding_x vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = MAX(style.spacing, style.hilite_padding_x * 2);
    // hilite_padding vs candidate_spacing
    // if hilite_padding_x over candidate
    // spacing, increase candidate spacing
    style.candidate_spacing =
        MAX(style.candidate_spacing, style.hilite_padding_x * 2);
    // vertical_text_with_wrap and hilite_padding_y over candidate_spacing
    if (style.vertical_text_with_wrap)
      style.candidate_spacing =
          MAX(style.candidate_spacing, style.hilite_padding_y * 2);
    // hilite_padding_y vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing = MAX(style.hilite_spacing, style.hilite_padding_y);
  }
  // fix padding and margin settings
  int scale = style.margin_x < 0 ? -1 : 1;
  style.margin_x = scale * MAX(style.hilite_padding_x, abs(style.margin_x));
  scale = style.margin_y < 0 ? -1 : 1;
  style.margin_y = scale * MAX(style.hilite_padding_y, abs(style.margin_y));
  // get enhanced_position
  _RimeGetBool(config, "style/enhanced_position", initialize,
               style.enhanced_position, true, false);
  // get color scheme
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  if (initialize && rime_api->config_get_string(config, "style/color_scheme",
                                                buffer, BUF_SIZE))
    _UpdateUIStyleColor(config, style, string(""));
}
} // namespace weasel
