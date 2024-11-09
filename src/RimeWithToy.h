#pragma once
#ifndef _RIME_WITH_TOY
#define _RIME_WITH_TOY

#include <WeaselIPCData.h>
#include <WeaselUI.h>
#include <data.h>
#include <filesystem>
#include <functional>
#include <rime_api.h>

namespace fs = std::filesystem;

namespace weasel {

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

class RimeWithToy {
public:
  RimeWithToy(UI *ui);
  void Initialize();
  void Finalize();
  BOOL ProcessKeyEvent(KeyEvent keyEvent, wstring &commit_str);
  void UpdateUI();
  void SwitchAsciiMode();
  RimeSessionId session_id() const { return m_session_id; }
  void SetTrayIconCallBack(std::function<void(const Status &)> func) {
    m_trayIconCallback = func;
  }

private:
  static fs::path data_path(string subdir);
  static fs::path get_log_path();
  static void setup_rime();
  static void on_message(void *context_object, RimeSessionId session_id,
                         const char *message_type, const char *message_value);
  void GetStatus(Status &stat);
  void GetCandidateInfo(CandidateInfo &cinfo, RimeContext &ctx);
  void GetContext(Context &context);
  BOOL ShowMessage(Context &ctx, Status &status);

  static std::string m_message_type;
  static std::string m_message_value;
  static std::string m_message_label;
  static std::string m_option_name;

  std::function<void(const Status &)> m_trayIconCallback;

  RimeSessionId m_session_id;
  RimeApi *rime_api;
  UI *m_ui;
};

void _UpdateUIStyle(RimeConfig *config, UI *ui, bool initialize);

} // namespace weasel
#endif
