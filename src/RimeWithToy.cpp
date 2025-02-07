#include "RimeWithToy.h"
#include "key_table.h"
#include <filesystem>
#include <map>
#include <regex>
#include <resource.h>

#define VERSION_STRING(x) #x

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

#define OPEN(x)                                                                \
  ShellExecute(nullptr, _T("open"), data_path(x).c_str(), NULL, NULL,          \
               SW_SHOWNORMAL);

namespace fs = std::filesystem;

int expand_ibus_modifier(int m) { return (m & 0xff) | ((m & 0xff00) << 16); }

namespace weasel {

bool _UpdateUIStyleColor(RimeConfig *config, UIStyle &style, string color);

static fs::path data_path(string subdir) {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append(subdir);
}

static wstring GetLabelText(const wstring label, const wchar_t *format) {
  wchar_t buffer[128];
  swprintf_s<128>(buffer, format, label.c_str());
  return std::wstring(buffer);
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
  DEBUGIF(m_trayIcon->debug())
      << "shared_path: " << shared_path << ", usr_path: " << usr_path
      << ", log_path: " << log_path;
  static auto shared_dir = shared_path.u8string();
  static auto usr_dir = usr_path.u8string();
  static auto log_dir = log_path.u8string();
  traits.shared_data_dir = reinterpret_cast<const char *>(shared_dir.c_str());
  traits.user_data_dir = reinterpret_cast<const char *>(usr_dir.c_str());
  traits.log_dir = reinterpret_cast<const char *>(log_dir.c_str());
  traits.prebuilt_data_dir = traits.shared_data_dir;
  traits.distribution_name = "rime.toy";
  traits.distribution_code_name = "rime.toy";
  // VERSION_INFO should be define in build system or by a #define
  traits.distribution_version = VERSION_INFO;
  traits.app_name = "rime.toy";
  RimeApi *rime_api = rime_get_api();
  rime_api->setup(&traits);
  rime_api->set_notification_handler(&on_message, this);
}

void RimeWithToy::on_message(void *context_object, RimeSessionId session_id,
                             const char *message_type,
                             const char *message_value) {
  // may be running in a thread when deploying rime
  RimeWithToy *self = reinterpret_cast<RimeWithToy *>(context_object);
  if (!self || !message_type || !message_value)
    return;
  m_message_type = message_type;
  m_message_value = message_value;

  if (m_message_type == "deploy") {
    if (m_message_value == "start")
      self->BalloonMsg("开始部署");
    else if (m_message_value == "success")
      self->BalloonMsg("部署完成");
    else if (m_message_value == "failure")
      self->BalloonMsg("部署失败，请查看日志");
  }

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

void RimeWithToy::BalloonMsg(const string &msg) {
  m_trayIcon->ShowBalloonTip(L"rime.toy", u8tow(msg), 500);
}

RimeWithToy::RimeWithToy(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_disabled(false) {
  m_ui = std::make_shared<UI>();
  const auto tooltip = L"rime.toy\n左键点击切换ASCII\n右键菜单可退出^_^";
  m_trayIcon = std::make_unique<TrayIcon>(hInstance, tooltip);
  m_reload_icon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_RELOAD));
  m_trayIcon->SetIcon(m_reload_icon);
  m_trayIcon->Show();
  Initialize();
  m_trayIcon->SetDeployFunc([&]() {
    DEBUGIF(m_trayIcon->debug()) << L"Deploy Menu clicked";
    DEBUGIF(m_disabled) << "Deploy job running, skip this request";
    if (m_disabled)
      return;
    m_disabled = true;
    m_trayIcon->SetIcon(m_reload_icon);
    Finalize();
    Initialize();
    BOOL ascii = rime_api->get_option(m_session_id, "ascii_mode");
    m_trayIcon->SetIcon(ascii ? m_ascii_icon : m_ime_icon);
    m_disabled = false;
  });
  m_trayIcon->SetSwichAsciiFunc([&]() { SwitchAsciiMode(); });
  m_trayIcon->SetSwichDarkFunc([&]() {
    m_current_dark_mode = IsUserDarkMode();
    _LoadSchemaSpecificSettings(m_session_id, GetRimeStatus().schema_id);
    if (m_ui)
      m_ui->Refresh();
  });
  m_trayIcon->SetOpenSharedDirFunc([&]() { OPEN("shared"); });
  m_trayIcon->SetOpenUserdDirFunc([&]() { OPEN("usr"); });
  m_trayIcon->SetOpenLogDirFunc([&]() { OPEN("log"); });
  m_trayIcon->SetSyncFunc([&]() {
    m_disabled = true;
    m_trayIcon->SetIcon(m_reload_icon);
    DEBUGIF(m_trayIcon->debug()) << L"Sync Menu clicked";
    if (!rime_api->sync_user_data())
      BalloonMsg("同步用户数据失败");
    Initialize();
    BOOL ascii = rime_api->get_option(m_session_id, "ascii_mode");
    m_trayIcon->SetIcon(ascii ? m_ascii_icon : m_ime_icon);
    m_disabled = false;
  });
  m_trayIcon->SetRefreshIconFunc([&]() {
    auto status = GetRimeStatus();
    m_trayIcon->SetIcon(status.ascii_mode ? m_ascii_icon : m_ime_icon);
  });
  m_trayIcon->SetIcon(m_ime_icon);
  m_trayIconCallback = [&](const Status &sta) {
    m_trayIcon->SetIcon(sta.ascii_mode ? m_ascii_icon : m_ime_icon);
  };

  if (!m_ui->uiCallback())
    m_ui->SetCallback([=](size_t *const select_index, size_t *const hover_index,
                          bool *const next_page, bool *const scroll_down) {
      HandleUICallback(select_index, hover_index, next_page, scroll_down);
    });
}

void RimeWithToy::Initialize() {
  DEBUGIF(m_trayIcon->debug()) << L"RimeWithToy::Initialize() called";
  setup_rime();
  m_disabled = true;
  rime_api = rime_get_api();
  rime_api->initialize(NULL);
  if (rime_api->start_maintenance(true))
    rime_api->join_maintenance_thread();
  rime_api->deploy_config_file("weasel.yaml", "config_version");
  m_session_id = rime_api->create_session();
  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    _UpdateUIStyle(&config, m_ui.get(), true);
    rime_api->config_close(&config);
  } else
    DEBUGIF(m_trayIcon->debug()) << L"open weasel config failed";
  m_base_style = m_ui->style();
  m_current_dark_mode = IsUserDarkMode();
  Status &status = m_ui->status();
  GetStatus(status);
  rime_api->set_option(m_session_id, "soft_cursor",
                       Bool(!m_ui->style().inline_preedit));
  m_disabled = false;
}

void RimeWithToy::Finalize() {
  DEBUGIF(m_trayIcon->debug()) << L"RimeWithToy::Finalize() called";
  rime_api->destroy_session(m_session_id);
  rime_api->finalize();
}

void RimeWithToy::SwitchAsciiMode() {
  DEBUGIF(m_trayIcon->debug()) << L"RimeWithToy::SwitchAsciiMode() called";
  BOOL ascii = rime_api->get_option(m_session_id, "ascii_mode");
  rime_api->set_option(m_session_id, "ascii_mode", !ascii);
  Status status;
  GetStatus(status);
  if (m_trayIconCallback)
    m_trayIconCallback(status);
}

BOOL RimeWithToy::ProcessKeyEvent(KeyEvent keyEvent) {
  if (m_disabled)
    return False;
  auto reprstr = repr(keyEvent.keycode, expand_ibus_modifier(keyEvent.mask));
  DEBUGIF(m_trayIcon->debug()) << "RimeWithToy::ProcessKeyEvent " << reprstr;
  if (m_ui->GetIsReposition()) {
    if (keyEvent.keycode == ibus::Up)
      keyEvent.keycode = ibus::Down;
    else if (keyEvent.keycode == ibus::Down)
      keyEvent.keycode = ibus::Up;
  }
  Bool handled = rime_api->process_key(m_session_id, keyEvent.keycode,
                                       expand_ibus_modifier(keyEvent.mask));
  RIME_STRUCT(RimeCommit, commit);
  if (rime_api->get_commit(m_session_id, &commit)) {
    m_commit_str = u8tow(commit.text);
    rime_api->free_commit(&commit);
  } else {
    m_commit_str.clear();
  }
  UpdateUI();
  return handled;
}

void RimeWithToy::UpdateUI() {
  if (!m_ui || m_disabled)
    return;
  Status &status = m_ui->status();
  Context ctx;
  GetStatus(status);
  GetContext(ctx, status);
  m_ui->style().client_caps = m_ui->style().inline_preedit;
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

bool RimeWithToy::CheckCommit() {
  auto committed = !m_commit_str.empty();
  if (!m_commit_str.empty()) {
    inserting = true;
    send_input_to_window(m_commit_str);
    inserting = false;
    m_commit_str.clear();
    if (!m_ui->status().composing)
      HideUI();
    else {
      UpdateUI();
    }
  }
  return committed;
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
    else if (m_message_value == "!ascii_mode") {
      tips = L"中文";
    } else if (m_message_value == "ascii_mode") {
      tips = L"英文";
    }
    if (m_message_value == "full_shape" || m_message_value == "!full_shape")
      status.type = FULL_SHAPE;
    else
      status.type = SCHEMA;
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
  const wstring &label_text_format = m_ui->style().label_text_format;
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
    cinfo.labels[i].str =
        GetLabelText(cinfo.labels[i].str, label_text_format.c_str());
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
  if (status.schema_id != m_last_schema_id) {
    m_last_schema_id = status.schema_id;
    if (m_last_schema_id != L".default")
      _LoadSchemaSpecificSettings(m_session_id, m_last_schema_id);
  }
}

void RimeWithToy::GetContext(Context &context, const Status &status) {
  RIME_STRUCT(RimeContext, ctx);
  if (rime_api->get_context(m_session_id, &ctx)) {
    if (status.composing) {
      auto style = m_ui->style();
      switch (m_ui->style().preedit_type) {
      case UIStyle::PreeditType::PREVIEW_ALL: {
        CandidateInfo cinfo;
        GetCandidateInfo(cinfo, ctx);
        std::string topush = string(ctx.composition.preedit) + "  [";
        auto &candies = ctx.menu.candidates;
        auto hilite = ctx.menu.highlighted_candidate_index;
        for (auto i = 0; i < ctx.menu.num_candidates; i++) {
          string label =
              style.label_font_point > 0 ? wtou8(cinfo.labels[i].str) : "";
          string comment =
              style.comment_font_point > 0 ? wtou8(cinfo.comments[i].str) : "";
          string mark_text =
              style.mark_text.empty() ? "*" : wtou8(style.mark_text);
          string prefix = (i != hilite) ? "" : mark_text;
          topush += " " + prefix + label + (candies[i].text) + " " + comment;
        }
        context.preedit.str = u8tow(topush) + wstring(L"]");
        if (ctx.composition.sel_start <= ctx.composition.sel_end) {
          TextAttribute attr;
          attr.range.start =
              utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start);
          attr.range.end =
              utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end);
          attr.range.cursor =
              utf8towcslen(ctx.composition.preedit, ctx.composition.cursor_pos);
          context.preedit.attributes.push_back(attr);
        }
      } break;
      case UIStyle::PreeditType::PREVIEW: {
        if (ctx.commit_text_preview) {
          string text = ctx.commit_text_preview;
          context.preedit.str = u8tow(text);
          TextAttribute attr;
          attr.range.start = 0;
          attr.range.end = utf8towcslen(text.c_str(), (int)text.size());
          attr.range.cursor = utf8towcslen(text.c_str(), (int)text.size());
          context.preedit.attributes.push_back(attr);
        }
        break;
      }
      case UIStyle::PreeditType::COMPOSITION: {
        auto text = string(ctx.composition.preedit);
        context.preedit.str = u8tow(text);
        if (ctx.composition.sel_start <= ctx.composition.sel_end) {
          TextAttribute attr;
          auto start =
              utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start);
          auto end =
              utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end);
          auto cursor =
              utf8towcslen(ctx.composition.preedit, ctx.composition.cursor_pos);
          attr.range.start = start;
          attr.range.end = end;
          attr.range.cursor = cursor;
          attr.type = HIGHLIGHTED;
          context.preedit.attributes.push_back(attr);
        }
        break;
      }
      default:
        break;
      }
    }
    if (ctx.menu.num_candidates) {
      CandidateInfo &cinfo(context.cinfo);
      GetCandidateInfo(cinfo, ctx);
    }
    rime_api->free_context(&ctx);
  }
}

Bool RimeWithToy::SelectCandidateCurrentPage(size_t index) {
  return rime_api->select_candidate_on_current_page(m_session_id, index);
}

Bool RimeWithToy::ChangePage(bool backward) {
  return rime_api->change_page(m_session_id, !!(backward));
}

Bool RimeWithToy::HighlightCandidateCurrentPage(size_t index) {
  return rime_api->highlight_candidate_on_current_page(m_session_id, index);
}

void RimeWithToy::_HandleMousePageEvent(bool *next_page, bool *scroll_down) {
  // from scrolling event
  bool handled = false;
  if (scroll_down) {
    if (m_ui->style().paging_on_scroll)
      handled = ChangePage(!(*scroll_down));
    else {
      UINT current_select = 0, cand_count = 0;
      current_select = m_ui->ctx().cinfo.highlighted;
      cand_count = m_ui->ctx().cinfo.candies.size();
      bool is_reposition = m_ui->GetIsReposition();
      int offset = *scroll_down ? 1 : -1;
      offset = offset * (is_reposition ? -1 : 1);
      int index = (int)current_select + offset;
      if (index >= 0 && index < (int)cand_count)
        HighlightCandidateCurrentPage(index);
      else {
        KeyEvent ke{0, 0};
        ke.keycode = (index < 0) ? ibus::Up : ibus::Down;
        handled = rime_api->process_key(m_session_id, ke.keycode,
                                        expand_ibus_modifier(ke.mask));
      }
    }
  } else if (next_page) { // from click event
    ChangePage(!(*next_page));
  }
  UpdateUI();
}

void RimeWithToy::HandleUICallback(size_t *select_index, size_t *hover_index,
                                   bool *next_page, bool *scroll_down) {
  if (next_page || scroll_down)
    _HandleMousePageEvent(next_page, scroll_down);
  else if (select_index) {
    rime_api->select_candidate_on_current_page(m_session_id, *select_index);
    ProcessKeyEvent(0);
    CheckCommit();
  } else if (hover_index) {
    rime_api->highlight_candidate_on_current_page(m_session_id, *hover_index);
    UpdateUI();
  }
}

void RimeWithToy::UpdateInputPosition(const RECT &rc) {
  if (m_ui)
    m_ui->UpdateInputPosition(rc);
}

void RimeWithToy::DestroyUI() {
  if (m_ui) {
    m_ui->ctx().clear();
    rime_api->clear_composition(m_session_id);
    m_ui->Destroy();
  }
}

void RimeWithToy::_LoadSchemaSpecificSettings(RimeSessionId id,
                                              const wstring &schema_id) {
  RimeConfig config;
  if (!rime_api->schema_open(wtou8(schema_id).c_str(), &config))
    return;
  UIStyle &style = m_ui->style();
  style = m_base_style;
  _UpdateUIStyle(&config, m_ui.get(), false);
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  if (!m_current_dark_mode &&
      rime_api->config_get_string(&config, "style/color_scheme", buffer,
                                  BUF_SIZE)) {
    std::string color_name(buffer);
    RimeConfigIterator preset = {0};
    if (rime_api->config_begin_map(
            &preset, &config, ("preset_color_schemes/" + color_name).c_str())) {
      _UpdateUIStyleColor(&config, style, color_name);
      rime_api->config_end(&preset);
    } else {
      RimeConfig weaselconfig;
      if (rime_api->config_open("weasel", &weaselconfig)) {
        _UpdateUIStyleColor(&weaselconfig, style, color_name);
        rime_api->config_close(&weaselconfig);
      }
    }
  } else if (m_current_dark_mode &&
             rime_api->config_get_string(&config, "style/color_scheme_dark",
                                         buffer, BUF_SIZE)) {
    std::string color_name(buffer);
    RimeConfigIterator preset = {0};
    if (rime_api->config_begin_map(
            &preset, &config, ("preset_color_schemes/" + color_name).c_str())) {
      _UpdateUIStyleColor(&config, style, color_name);
      rime_api->config_end(&preset);
    } else {
      RimeConfig weaselconfig;
      if (rime_api->config_open("weasel", &weaselconfig)) {
        _UpdateUIStyleColor(&weaselconfig, style, color_name);
        rime_api->config_close(&weaselconfig);
      }
    }
  }
  Bool inline_preedit = false;
  if (rime_api->config_get_bool(&config, "style/inline_preedit",
                                &inline_preedit)) {
    style.inline_preedit = !!inline_preedit;
  }
  const auto load_icon = [](RimeConfig &config, const char *key,
                            const char *backup = nullptr) {
    auto user_dir = data_path("usr");
    auto shared_dir = data_path("shared");
    RimeApi *api = rime_get_api();
    const int BUF_SIZE = 255;
    char buffer[BUF_SIZE + 1] = {0};
    if (api->config_get_string(&config, key, buffer, BUF_SIZE) ||
        (backup && api->config_get_string(&config, backup, buffer, BUF_SIZE))) {
      wstring resource = string_to_wstring(buffer, CP_UTF8);
      if (fs::is_regular_file(user_dir / resource))
        return (user_dir / resource).wstring();
      else if (fs::is_regular_file(shared_dir / resource))
        return (shared_dir / resource).wstring();
    }
    return wstring();
  };
  style.current_zhung_icon =
      load_icon(config, "schema/icon", "schema/zhung_icon");
  style.current_ascii_icon = load_icon(config, "schema/ascii_icon");
  style.current_full_icon = load_icon(config, "schema/full_icon");
  style.current_half_icon = load_icon(config, "schema/half_icon");
  const int STATUS_ICON_SIZE = GetSystemMetrics(SM_CXICON);
  if (!style.current_zhung_icon.empty()) {
    m_ime_icon =
        (HICON)LoadImage(NULL, style.current_zhung_icon.c_str(), IMAGE_ICON,
                         STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_LOADFROMFILE);
  } else {
    m_ime_icon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN));
  }
  if (!style.current_ascii_icon.empty()) {
    m_ascii_icon =
        (HICON)LoadImage(NULL, style.current_ascii_icon.c_str(), IMAGE_ICON,
                         STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_LOADFROMFILE);
  } else {
    m_ascii_icon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON_ASCII));
  }
  rime_api->config_close(&config);
}

bool RimeWithToy::StartUI() { return m_ui->Create(nullptr); }
// ----------------------------------------------------------------------------

static inline COLORREF blend_colors(COLORREF fcolor, COLORREF bcolor) {
  // 提取各通道的值
  BYTE fA = (fcolor >> 24) & 0xFF; // 获取前景的 alpha 通道
  BYTE fB = (fcolor >> 16) & 0xFF; // 获取前景的 blue 通道
  BYTE fG = (fcolor >> 8) & 0xFF;  // 获取前景的 green 通道
  BYTE fR = fcolor & 0xFF;         // 获取前景的 red 通道
  BYTE bA = (bcolor >> 24) & 0xFF; // 获取背景的 alpha 通道
  BYTE bB = (bcolor >> 16) & 0xFF; // 获取背景的 blue 通道
  BYTE bG = (bcolor >> 8) & 0xFF;  // 获取背景的 green 通道
  BYTE bR = bcolor & 0xFF;         // 获取背景的 red 通道
  // 将 alpha 通道转换为 [0, 1] 的浮动值
  float fAlpha = fA / 255.0f;
  float bAlpha = bA / 255.0f;
  // 计算每个通道的加权平均值
  float retAlpha = fAlpha + (1 - fAlpha) * bAlpha;
  // 混合红、绿、蓝通道
  BYTE retR = (BYTE)((fR * fAlpha + bR * bAlpha * (1 - fAlpha)) / retAlpha);
  BYTE retG = (BYTE)((fG * fAlpha + bG * bAlpha * (1 - fAlpha)) / retAlpha);
  BYTE retB = (BYTE)((fB * fAlpha + bB * bAlpha * (1 - fAlpha)) / retAlpha);
  // 返回合成后的颜色
  return (BYTE)(retAlpha * 255) << 24 | retB << 16 | retG << 8 | retR;
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
// parset bool type configuration to T type value trueValue / falseValue
template <typename T>
void _RimeGetBool(RimeConfig *config, const char *key, bool cond, T &value,
                  const T &trueValue = true, const T &falseValue = false) {
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

template <typename T>
void _RimeGetIntStr(RimeConfig *config, const char *key, T &value,
                    const char *fb_key = nullptr,
                    const void *fb_value = nullptr,
                    const std::function<void(T &)> &func = nullptr) {
  RimeApi *rime_api = rime_get_api();
  if constexpr (std::is_same<T, int>::value) {
    if (!rime_api->config_get_int(config, key, &value) && fb_key != 0)
      rime_api->config_get_int(config, fb_key, &value);
  } else if constexpr (std::is_same<T, std::wstring>::value) {
    const int BUF_SIZE = 2047;
    char buffer[BUF_SIZE + 1] = {0};
    if (rime_api->config_get_string(config, key, buffer, BUF_SIZE) ||
        rime_api->config_get_string(config, fb_key, buffer, BUF_SIZE)) {
      value = u8tow(buffer);
    } else if (fb_value) {
      value = *(T *)fb_value;
    }
  }
  if (func)
    func(value);
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
  const std::function<void(std::wstring &)> rmspace = [](std::wstring &str) {
    str = std::regex_replace(str, std::wregex(L"\\s*(,|:|^|$)\\s*"), L"$1");
  };
  const std::function<void(int &)> _abs = [](int &value) {
    value = abs(value);
  };
  // get font faces
  _RimeGetIntStr(config, "style/font_face", style.font_face, 0, 0, rmspace);
  std::wstring *const pFallbackFontFace = initialize ? &style.font_face : 0;
  _RimeGetIntStr(config, "style/label_font_face", style.label_font_face, 0,
                 pFallbackFontFace, rmspace);
  _RimeGetIntStr(config, "style/comment_font_face", style.comment_font_face, 0,
                 pFallbackFontFace, rmspace);
  // able to set label font/comment font empty, force fallback to font face.
  if (style.label_font_face.empty())
    style.label_font_face = style.font_face;
  if (style.comment_font_face.empty())
    style.comment_font_face = style.font_face;
  _RimeGetIntStr(config, "style/font_point", style.font_point, 0, 0, _abs);
  // get font points
  _RimeGetIntStr(config, "style/label_font_point", style.label_font_point, 0,
                 &style.font_point, _abs);
  _RimeGetIntStr(config, "style/comment_font_point", style.comment_font_point,
                 0, &style.font_point, _abs);
  _RimeGetIntStr(config, "style/candidate_abbreviate_length",
                 style.candidate_abbreviate_length, 0, 0, _abs);
  _RimeGetBool(config, "style/inline_preedit", initialize,
               style.inline_preedit);
  _RimeGetBool(config, "style/vertical_auto_reverse", initialize,
               style.vertical_auto_reverse);
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
               style.display_tray_icon);
  _RimeGetBool(config, "style/ascii_tip_follow_cursor", initialize,
               style.ascii_tip_follow_cursor);
  _RimeGetBool(config, "style/horizontal", initialize, style.layout_type,
               UIStyle::LAYOUT_HORIZONTAL, UIStyle::LAYOUT_VERTICAL);
  _RimeGetBool(config, "style/paging_on_scroll", initialize,
               style.paging_on_scroll);
  _RimeGetBool(config, "style/click_to_capture", initialize,
               style.click_to_capture);
  _RimeGetBool(config, "style/fullscreen", false, style.layout_type,
               ((style.layout_type == UIStyle::LAYOUT_HORIZONTAL)
                    ? UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN
                    : UIStyle::LAYOUT_VERTICAL_FULLSCREEN),
               style.layout_type);
  _RimeGetBool(config, "style/vertical_text", false, style.layout_type,
               UIStyle::LAYOUT_VERTICAL_TEXT, style.layout_type);
  _RimeGetBool(config, "style/vertical_text_left_to_right", false,
               style.vertical_text_left_to_right);
  _RimeGetBool(config, "style/vertical_text_with_wrap", false,
               style.vertical_text_with_wrap);
  const std::map<string, bool> _text_orientation = {
      {string("horizontal"), false}, {string("vertical"), true}};
  bool _text_orientation_bool = false;
  _RimeParseStringOptWithFallback(config, "style/text_orientation",
                                  _text_orientation_bool, _text_orientation,
                                  _text_orientation_bool);
  if (_text_orientation_bool)
    style.layout_type = UIStyle::LAYOUT_VERTICAL_TEXT;
  _RimeGetIntStr(config, "style/label_format", style.label_text_format);
  _RimeGetIntStr(config, "style/mark_text", style.mark_text);
  _RimeGetIntStr(config, "style/layout/baseline", style.baseline, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/linespacing", style.linespacing, 0, 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/min_width", style.min_width, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/max_width", style.max_width, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/min_height", style.min_height, 0, 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/max_height", style.max_height, 0, 0,
                 _abs);
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
  _RimeGetIntStr(config, "style/layout/border", style.border,
                 "style/layout/border_width", 0, _abs);
  _RimeGetIntStr(config, "style/layout/margin_x", style.margin_x);
  _RimeGetIntStr(config, "style/layout/margin_y", style.margin_y);
  _RimeGetIntStr(config, "style/layout/spacing", style.spacing, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/candidate_spacing",
                 style.candidate_spacing, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/hilite_spacing", style.hilite_spacing, 0,
                 0, _abs);
  _RimeGetIntStr(config, "style/layout/hilite_padding_x",
                 style.hilite_padding_x, "style/layout/hilite_padding", 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/hilite_padding_y",
                 style.hilite_padding_y, "style/layout/hilite_padding", 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/shadow_radius", style.shadow_radius, 0,
                 0, _abs);
  _RimeGetIntStr(config, "style/layout/shadow_offset_x", style.shadow_offset_x);
  _RimeGetIntStr(config, "style/layout/shadow_offset_y", style.shadow_offset_y);
  // round_corner as alias of hilited_corner_radius
  _RimeGetIntStr(config, "style/layout/hilited_corner_radius",
                 style.round_corner, "style/layout/round_corner", 0, _abs);
  // corner_radius not set, fallback to round_corner
  _RimeGetIntStr(config, "style/layout/corner_radius", style.round_corner_ex,
                 "style/layout/round_corner", 0, _abs);
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
               style.enhanced_position);
  // get color scheme
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  if (initialize && rime_api->config_get_string(config, "style/color_scheme",
                                                buffer, BUF_SIZE))
    _UpdateUIStyleColor(config, style, string(""));
}
} // namespace weasel
