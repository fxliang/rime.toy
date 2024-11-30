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
  RimeWithToy(HINSTANCE hInstance, wstring &commit_str);
  void Initialize();
  void Finalize();
  BOOL ProcessKeyEvent(KeyEvent keyEvent);
  void UpdateUI();
  void SwitchAsciiMode();
  RimeSessionId session_id() const { return m_session_id; }
  void UpdateInputPosition(const RECT &rc);
  bool StartUI();
  void DestroyUI();
  Status GetRimeStatus();
  HWND UIHwnd() { return m_ui ? m_ui->hwnd() : nullptr; }

private:
  void setup_rime();
  static void on_message(void *context_object, RimeSessionId session_id,
                         const char *message_type, const char *message_value);
  void GetStatus(Status &stat);
  void GetCandidateInfo(CandidateInfo &cinfo, RimeContext &ctx);
  void GetContext(Context &context, const Status &status);
  BOOL ShowMessage(Context &ctx, Status &status);

  Bool SelectCandidateCurrentPage(size_t index);
  Bool ChangePage(bool backward);
  Bool HighlightCandidateCurrentPage(size_t index);
  void HandleUICallback(size_t *const select_index, size_t *const hover_index,
                        bool *const next_page, bool *const scroll_next_page);

  void _HandleMousePageEvent(bool *next_page, bool *scroll_next_page);
  void _LoadSchemaSpecificSettings(RimeSessionId id, const wstring &schema_id);

  static std::string m_message_type;
  static std::string m_message_value;
  static std::string m_message_label;
  static std::string m_option_name;

  std::function<void(const Status &)> m_trayIconCallback;

  std::unique_ptr<TrayIcon> m_trayIcon;
  HICON m_ime_icon;
  HICON m_ascii_icon;
  HICON m_reload_icon;
  HINSTANCE m_hInstance;
  RimeSessionId m_session_id;
  RimeApi *rime_api;
  an<UI> m_ui;
  wstring m_last_schema_id;
  wstring &m_commit_str;
  UIStyle m_base_style;
  bool m_disabled;
  bool m_current_dark_mode;
};

void _UpdateUIStyle(RimeConfig *config, UI *ui, bool initialize);

} // namespace weasel

#endif
