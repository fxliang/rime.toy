#pragma once
#include <WeaselUI.h>
#include <algorithm>
#include <data.h>
#include <functional>
#include <map>
#include <regex>
#include <rime_api.h>
#include <string>
#include <windef.h>
#include <wingdi.h>

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

using UIStyle = weasel::UIStyle;
using UI = weasel::UI;

static inline COLORREF blend_colors(COLORREF fcolor, COLORREF bcolor) {
  return RGB((GetRValue(fcolor) * 2 + GetRValue(bcolor)) / 3,
             (GetGValue(fcolor) * 2 + GetGValue(bcolor)) / 3,
             (GetBValue(fcolor) * 2 + GetBValue(bcolor)) / 3) |
         ((((fcolor >> 24) + (bcolor >> 24) / 2) << 24));
}

// convertions from color format to COLOR_ABGR
static inline int ConvertColorToAbgr(int color, ColorFormat fmt = COLOR_ABGR) {
  if (fmt == COLOR_ABGR)
    return color;
  else if (fmt == COLOR_ARGB)
    return ARGB2ABGR(color);
  else
    return RGBA2ABGR(color);
}
// parse color value, with fallback value
static Bool _RimeConfigGetColor32bWithFallback(RimeConfig *config,
                                               const std::string key,
                                               int &value,
                                               const ColorFormat &fmt,
                                               const int &fallback) {
  RimeApi *rime_api = rime_get_api();
  char color[256] = {0};
  if (!rime_api->config_get_string(config, key.c_str(), color, 256)) {
    value = fallback;
    return False;
  }
  std::string color_str = std::string(color);
  // color code hex
  if (std::regex_match(color_str, HEX_REGEX)) {
    std::string tmp = std::regex_replace(color_str, TRIMHEAD_REGEX, "");
    // limit first 8 code
    tmp = tmp.substr(0, 8);
    if (tmp.length() == 6) // color code without alpha, xxyyzz add alpha ff
    {
      value = std::stoi(tmp, 0, 16);
      if (fmt != COLOR_RGBA)
        value |= 0xff000000;
      else
        value = (value << 8) | 0x000000ff;
    } else if (tmp.length() == 3) // color hex code xyz => xxyyzz and alpha ff
    {
      tmp = tmp.substr(0, 1) + tmp.substr(0, 1) + tmp.substr(1, 1) +
            tmp.substr(1, 1) + tmp.substr(2, 1) + tmp.substr(2, 1);

      value = std::stoi(tmp, 0, 16);
      if (fmt != COLOR_RGBA)
        value |= 0xff000000;
      else
        value = (value << 8) | 0x000000ff;
    } else if (tmp.length() == 4) // color hex code vxyz => vvxxyyzz
    {
      tmp = tmp.substr(0, 1) + tmp.substr(0, 1) + tmp.substr(1, 1) +
            tmp.substr(1, 1) + tmp.substr(2, 1) + tmp.substr(2, 1) +
            tmp.substr(3, 1) + tmp.substr(3, 1);

      std::string tmp1 = tmp.substr(0, 6);
      int value1 = std::stoi(tmp1, 0, 16);
      tmp1 = tmp.substr(6);
      int value2 = std::stoi(tmp1, 0, 16);
      value = (value1 << (tmp1.length() * 4)) | value2;
    } else if (tmp.length() > 6 &&
               tmp.length() <= 8) /* color code with alpha */
    {
      // stoi limitation, split to handle
      std::string tmp1 = tmp.substr(0, 6);
      int value1 = std::stoi(tmp1, 0, 16);
      tmp1 = tmp.substr(6);
      int value2 = std::stoi(tmp1, 0, 16);
      value = (value1 << (tmp1.length() * 4)) | value2;
    } else // reject other code, length less then 3 or length == 5
    {
      value = fallback;
      return False;
    }
    value = ConvertColorToAbgr(value, fmt);
    value = (value & 0xffffffff);
    return True;
  }
  // regular number or other stuff, if user use pure dec number, they should
  // take care themselves
  else {
    int tmp = 0;
    if (!rime_api->config_get_int(config, key.c_str(), &tmp)) {
      value = fallback;
      return False;
    }
    if (fmt != COLOR_RGBA)
      value = (tmp | 0xff000000) & 0xffffffff;
    else
      value = ((tmp << 8) | 0x000000ff) & 0xffffffff;
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
static void _RimeGetBool(RimeConfig *config, char *key, bool cond, T &value,
                         const T &trueValue, const T &falseValue) {
  RimeApi *rime_api = rime_get_api();
  Bool tempb = False;
  if (rime_api->config_get_bool(config, key, &tempb) || cond)
    value = (!!tempb) ? trueValue : falseValue;
}
//	parse string option to T type value, with fallback
template <typename T>
void _RimeParseStringOptWithFallback(RimeConfig *config, const std::string key,
                                     T &value,
                                     const std::map<std::string, T> amap,
                                     const T &fallback) {
  RimeApi *rime_api = rime_get_api();
  char str_buff[256] = {0};
  if (rime_api->config_get_string(config, key.c_str(), str_buff,
                                  sizeof(str_buff) - 1)) {
    auto it = amap.find(std::string(str_buff));
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
bool _UpdateUIStyleColor(RimeConfig *config, UIStyle &style,
                         std::string color) {
  RimeApi *rime_api = rime_get_api();
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  std::string color_mark = "style/color_scheme";
  // color scheme
  if (rime_api->config_get_string(config, color_mark.c_str(), buffer,
                                  BUF_SIZE) ||
      !color.empty()) {
    std::string prefix("preset_color_schemes/");
    prefix += (color.empty()) ? buffer : color;
    // define color format, default abgr if not set
    ColorFormat fmt = COLOR_ABGR;
    const std::map<std::string, ColorFormat> _colorFmt = {
        {std::string("argb"), COLOR_ARGB},
        {std::string("rgba"), COLOR_RGBA},
        {std::string("abgr"), COLOR_ABGR}};
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
  const std::map<std::string, UIStyle::PreeditType> _preeditMap = {
      {std::string("composition"), UIStyle::COMPOSITION},
      {std::string("preview"), UIStyle::PREVIEW},
      {std::string("preview_all"), UIStyle::PREVIEW_ALL}};
  _RimeParseStringOptWithFallback(config, "style/preedit_type",
                                  style.preedit_type, _preeditMap,
                                  style.preedit_type);
  const std::map<std::string, UIStyle::AntiAliasMode> _aliasModeMap = {
      {std::string("force_dword"), UIStyle::FORCE_DWORD},
      {std::string("cleartype"), UIStyle::CLEARTYPE},
      {std::string("grayscale"), UIStyle::GRAYSCALE},
      {std::string("aliased"), UIStyle::ALIASED},
      {std::string("default"), UIStyle::DEFAULT}};
  _RimeParseStringOptWithFallback(config, "style/antialias_mode",
                                  style.antialias_mode, _aliasModeMap,
                                  style.antialias_mode);
  const std::map<std::string, UIStyle::HoverType> _hoverTypeMap = {
      {std::string("none"), UIStyle::HoverType::NONE},
      {std::string("semi_hilite"), UIStyle::HoverType::SEMI_HILITE},
      {std::string("hilite"), UIStyle::HoverType::HILITE}};
  _RimeParseStringOptWithFallback(config, "style/hover_type", style.hover_type,
                                  _hoverTypeMap, style.hover_type);
  const std::map<std::string, UIStyle::LayoutAlignType> _alignType = {
      {std::string("top"), UIStyle::ALIGN_TOP},
      {std::string("center"), UIStyle::ALIGN_CENTER},
      {std::string("bottom"), UIStyle::ALIGN_BOTTOM}};
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
  const std::map<std::string, bool> _text_orientation = {
      {std::string("horizontal"), false}, {std::string("vertical"), true}};
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
  const std::map<std::string, UIStyle::LayoutType> _layoutMap = {
      {std::string("vertical"), UIStyle::LAYOUT_VERTICAL},
      {std::string("horizontal"), UIStyle::LAYOUT_HORIZONTAL},
      {std::string("vertical_text"), UIStyle::LAYOUT_VERTICAL_TEXT},
      {std::string("vertical+fullscreen"), UIStyle::LAYOUT_VERTICAL_FULLSCREEN},
      {std::string("horizontal+fullscreen"),
       UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN}};
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
    style.spacing = std::max(style.spacing, style.hilite_padding_y * 2);
    // hilite_padding vs candidate_spacing
    if (style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN ||
        style.layout_type == UIStyle::LAYOUT_VERTICAL) {
      // vertical, if hilite_padding_y over candidate spacing,
      // increase candidate spacing
      style.candidate_spacing =
          std::max(style.candidate_spacing, style.hilite_padding_y * 2);
    } else {
      // horizontal, if hilite_padding_x over candidate
      // spacing, increase candidate spacing
      style.candidate_spacing =
          std::max(style.candidate_spacing, style.hilite_padding_x * 2);
    }
    // hilite_padding_x vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing =
          std::max(style.hilite_spacing, style.hilite_padding_x);
  } else // LAYOUT_VERTICAL_TEXT
  {
    // hilite_padding_x vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = std::max(style.spacing, style.hilite_padding_x * 2);
    // hilite_padding vs candidate_spacing
    // if hilite_padding_x over candidate
    // spacing, increase candidate spacing
    style.candidate_spacing =
        std::max(style.candidate_spacing, style.hilite_padding_x * 2);
    // vertical_text_with_wrap and hilite_padding_y over candidate_spacing
    if (style.vertical_text_with_wrap)
      style.candidate_spacing =
          std::max(style.candidate_spacing, style.hilite_padding_y * 2);
    // hilite_padding_y vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing =
          std::max(style.hilite_spacing, style.hilite_padding_y);
  }
  // fix padding and margin settings
  int scale = style.margin_x < 0 ? -1 : 1;
  style.margin_x =
      scale * std::max(style.hilite_padding_x, abs(style.margin_x));
  scale = style.margin_y < 0 ? -1 : 1;
  style.margin_y =
      scale * std::max(style.hilite_padding_y, abs(style.margin_y));
  // get enhanced_position
  _RimeGetBool(config, "style/enhanced_position", initialize,
               style.enhanced_position, true, false);
  // get color scheme
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  if (initialize && rime_api->config_get_string(config, "style/color_scheme",
                                                buffer, BUF_SIZE))
    _UpdateUIStyleColor(config, style, "");
}
