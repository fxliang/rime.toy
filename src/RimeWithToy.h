#pragma once
#ifndef _RIME_WITH_TOY
#define _RIME_WITH_TOY

#include "keymodule.h"
#include "trayicon.h"
#include <WeaselIPCData.h>
#include <WeaselUI.h>
#include <rime_api.h>
#include <utils.h>

namespace weasel {

class RimeWithToy {
public:
  RimeWithToy(UI *ui, HINSTANCE hInstance);
  void Initialize();
  void Finalize();
  BOOL ProcessKeyEvent(KeyEvent keyEvent, wstring &commit_str);
  void UpdateUI();
  void SwitchAsciiMode();
  RimeSessionId session_id() const { return m_session_id; }

private:
  void setup_rime();
  static void on_message(void *context_object, RimeSessionId session_id,
                         const char *message_type, const char *message_value);
  void GetStatus(Status &stat);
  void GetCandidateInfo(CandidateInfo &cinfo, RimeContext &ctx);
  void GetContext(Context &context, const Status &status);
  BOOL ShowMessage(Context &ctx, Status &status);

  static std::string m_message_type;
  static std::string m_message_value;
  static std::string m_message_label;
  static std::string m_option_name;

  std::function<void(const Status &)> m_trayIconCallback;

  std::unique_ptr<TrayIcon> m_trayIcon;
  HICON m_ime_icon;
  HICON m_ascii_icon;
  HINSTANCE m_hInstance;
  RimeSessionId m_session_id;
  RimeApi *rime_api;
  UI *m_ui;
  wstring m_last_schema_id;
};

void _UpdateUIStyle(RimeConfig *config, UI *ui, bool initialize);

} // namespace weasel

#endif
