#pragma once
#ifndef _RIME_WITH_TOY
#define _RIME_WITH_TOY

#include "keymodule.h"
#include "trayicon.h"
#include <WeaselIPCData.h>
#include <WeaselUI.h>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <rime_api.h>
#include <string>
#include <thread>
#include <utils.h>
#include <vector>

using std::string;
namespace fs = std::filesystem;
using path = std::filesystem::path;

enum class PositionType {
  kMousePos,
  kCenter,
  kTopLeft,
  kTopCenter,
  kTopRight,
  kBottomLeft,
  kBottomCenter,
  kBottomRight
};

namespace weasel {

typedef std::function<void(const path &)> EvtHandler;

class SimpleFileMonitor {
public:
  SimpleFileMonitor(const std::vector<string> &paths, EvtHandler handler)
      : filePaths(paths), isMonitoring(false), onChange(handler) {
    initializeFileTimes();
  }
  void initializeFileTimes() {
    for (const auto &path : filePaths) {
      getFileModificationTime(path);
    }
  }
  bool getFileModificationTime(const string &filePath) {
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      DEBUG << "无法打开文件: " << filePath;
      return false;
    }
    FILETIME ftWrite;
    if (GetFileTime(hFile, NULL, NULL, &ftWrite)) {
      fileTimes[filePath] = ftWrite;
      CloseHandle(hFile);
      return true;
    }
    CloseHandle(hFile);
    return false;
  }
  bool checkFileModification(const string &filePath) {
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      DEBUG << "文件不存在或无法访问: " << filePath;
      return false;
    }
    FILETIME ftWrite;
    if (GetFileTime(hFile, NULL, NULL, &ftWrite)) {
      auto it = fileTimes.find(filePath);
      if (it != fileTimes.end()) {
        if (CompareFileTime(&ftWrite, &it->second) != 0) {
          fileTimes[filePath] = ftWrite;
          CloseHandle(hFile);
          return true;
        }
      } else {
        fileTimes[filePath] = ftWrite;
      }
    }
    CloseHandle(hFile);
    return false;
  }
  void startMonitoring(int checkIntervalSeconds = 1) {
    isMonitoring = true;
    while (isMonitoring) {
      for (const auto &path : filePaths) {
        if (checkFileModification(path)) {
          if (onChange) {
            onChange(path);
          }
        }
      }

      std::this_thread::sleep_for(std::chrono::seconds(checkIntervalSeconds));
    }
  }
  void stopMonitoring() { isMonitoring = false; }

private:
  std::vector<string> filePaths;
  std::map<string, FILETIME> fileTimes;
  bool isMonitoring;
  EvtHandler onChange;
};

class RimeWithToy {
public:
  RimeWithToy(HINSTANCE hInstance);
  ~RimeWithToy();
  void Initialize(bool first_time = false);
  void Finalize();
  BOOL ProcessKeyEvent(KeyEvent keyEvent);
  void UpdateUI();
  void SwitchAsciiMode();
  RimeSessionId session_id() const { return m_session_id; }
  void UpdateInputPosition(const RECT &rc);
  bool StartUI();
  void DestroyUI();
  void HideUI() {
    if (m_ui)
      m_ui->Hide();
  }
  Status &GetRimeStatus() { return m_ui->status(); }
  wstring &GetCommitStr() { return m_commit_str; }
  HWND UIHwnd() { return m_ui ? m_ui->hwnd() : nullptr; }
  bool CheckCommit();

private:
  void setup_rime();
  static void on_message(void *context_object, RimeSessionId session_id,
                         const char *message_type, const char *message_value);
  void GetStatus(Status &stat);
  void GetCandidateInfo(CandidateInfo &cinfo, RimeContext &ctx);
  void GetContext(Context &context, const Status &status);
  BOOL ShowMessage(Context &ctx, Status &status);
  void BalloonMsg(const string &msg);

  Bool SelectCandidateCurrentPage(size_t index);
  Bool ChangePage(bool backward);
  Bool HighlightCandidateCurrentPage(size_t index);
  void HandleUICallback(size_t *const select_index, size_t *const hover_index,
                        bool *const next_page, bool *const scroll_down);

  void _HandleMousePageEvent(bool *next_page, bool *scroll_down);
  void _LoadSchemaSpecificSettings(RimeSessionId id, const wstring &schema_id);

  static string m_message_type;
  static string m_message_value;
  static string m_message_label;
  static string m_option_name;

  std::function<void(const Status &)> m_trayIconCallback;

  std::unique_ptr<TrayIcon> m_trayIcon;
  HICON m_ime_icon;
  HICON m_ascii_icon;
  HICON m_reload_icon;
  HINSTANCE m_hInstance;
  RimeSessionId m_session_id;
  an<UI> m_ui;
  wstring m_last_schema_id;
  wstring m_commit_str;
  UIStyle m_base_style;
  bool m_disabled;
  bool m_current_dark_mode;
  std::unique_ptr<SimpleFileMonitor> m_file_monitor;
  std::thread m_monitor_thread;
};

void _UpdateUIStyle(RimeConfig *config, UI *ui, bool initialize);

} // namespace weasel

#endif
