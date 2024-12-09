#pragma once

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <tchar.h>
#include <vector>
#include <windows.h>
#include <winerror.h>
#include <wrl.h>

namespace weasel {

using namespace Microsoft::WRL;
using wstring = std::wstring;
using string = std::string;
template <typename T> using vector = std::vector<T>;

// convert string to wstring, in code_page
inline std::wstring string_to_wstring(const std::string &str,
                                      int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != CP_ACP && code_page != CP_UTF8)
    return L"";
  int len = MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(),
                                nullptr, 0);
  if (len <= 0)
    return L"";
  // Use unique_ptr to manage the buffer memory automatically
  std::unique_ptr<WCHAR[]> buffer = std::make_unique<WCHAR[]>(len + 1);
  MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), buffer.get(),
                      len);
  buffer[len] = L'\0'; // Null-terminate the wide string
  return std::wstring(buffer.get());
}
// convert wstring to string, in code_page
inline std::string wstring_to_string(const std::wstring &wstr,
                                     int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != CP_ACP && code_page != CP_UTF8)
    return "";
  int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return "";
  // Use unique_ptr to manage the buffer memory automatically
  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len + 1);
  WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                      buffer.get(), len, nullptr, nullptr);
  buffer[len] = '\0'; // Null-terminate the string
  return std::string(buffer.get());
}

inline int utf8towcslen(const char *utf8_str, int utf8_len) {
  return MultiByteToWideChar(CP_UTF8, 0, utf8_str, utf8_len, NULL, 0);
}

#define wtou8(x) wstring_to_string(x, CP_UTF8)
#define wtoacp(x) wstring_to_string(x)
#define u8tow(x) string_to_wstring(x, CP_UTF8)
#define acptow(x) string_to_wstring(x, CP_ACP)
#define MAX(x, y) ((x > y) ? x : y)
#define MIN(x, y) ((x < y) ? x : y)

class DebugStream {
public:
  DebugStream() = default;
  ~DebugStream() { OutputDebugString(ss.str().c_str()); }
  template <typename T> DebugStream &operator<<(const T &value) {
    ss << value;
    return *this;
  }
  DebugStream &operator<<(const char *value) {
    if (value) {
      std::wstring wvalue(u8tow(value)); // utf-8
      ss << wvalue;
    }
    return *this;
  }
  DebugStream &operator<<(const string value) {
    std::wstring wvalue(u8tow(value)); // utf-8
    ss << wvalue;
    return *this;
  }
  DebugStream &operator<<(const RECT &rc) {
    ss << L"rc = {" << rc.left << L", " << rc.top << L", " << rc.right << L", "
       << rc.bottom << L"}";
    return *this;
  }
  DebugStream &operator<<(const POINT &pt) {
    ss << L"pt = (" << pt.x << L", " << pt.y << L")";
    return *this;
  }
  DebugStream &operator<<(const std::filesystem::path &path) {
    ss << path.c_str();
    return *this;
  }

private:
  std::wstringstream ss;
};
// get current time string
inline std::string current_time() {
  using namespace std::chrono;
  // 获取当前时间
  auto now = system_clock::now();
  // 转换为系统时间（time_point）
  auto time_point = system_clock::to_time_t(now);
  // 获取时间戳的纳秒部分，先转换为微秒
  auto ns = duration_cast<microseconds>(now.time_since_epoch()); // 转换为微秒
  // 转换为本地时间
  std::tm tm = *std::localtime(&time_point);
  // 构造时间字符串
  std::ostringstream oss;
  oss << std::put_time(&tm,
                       "%Y%m%d %H:%M:%S"); // 日期时间格式：20241113 08:54:34
  oss << "." << std::setw(6) << std::setfill('0')
      << ns.count() % 1000000; // 微秒部分
  return oss.str();
}
// output debug string
#define DEBUG                                                                  \
  (weasel::DebugStream() << "[" << weasel::current_time() << " " << __FILE__   \
                         << ":" << __LINE__ << "] ")

// DEBUG info with condition
#define DEBUGIF(x)                                                             \
  if (x)                                                                       \
  DEBUG

// HRESULT to wstring info
inline std::basic_string<TCHAR> StrzHr(HRESULT hr) {
  TCHAR buffer[1024] = {0};
  if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, hr, 0, buffer, sizeof(buffer) / sizeof(TCHAR),
                    NULL)) {
    return std::basic_string<TCHAR>(buffer);
  } else {
    std::basic_ostringstream<TCHAR> oss;
    oss << _T("Unknown error: 0x") << std::hex << hr;
    return oss.str();
  }
}

struct ComException {
  HRESULT result;
  ComException(HRESULT const value) : result(value) {}
};

// check result, if FAILED(result) then OutputDebugString result and throw
// exception
#define HR(result) HR_Impl(result, __FILE__, __LINE__)

// if FAILED(result) OutputDebugString result info and return result
#define FR(result)                                                             \
  if (FAILED(result)) {                                                        \
    weasel::DebugStream() << "[" << weasel::current_time() << " " << __FILE__  \
                          << ":" << __LINE__ << "] "                           \
                          << weasel::StrzHr(result);                           \
    return result;                                                             \
  }

// if FAILED(hr), do a list of actions
#define FAILEDACTION(hr, ...)                                                  \
  if (FAILED(hr)) {                                                            \
    __VA_ARGS__;                                                               \
  }

inline void HR_Impl(HRESULT const result, const char *file, int line) {
  if (S_OK != result) {
    weasel::DebugStream() << "[" << weasel::current_time() << " " << file << ":"
                          << line << "] " << weasel::StrzHr(result);
    throw ComException(result);
  }
}

// release a ComPtr
template <typename T> void SafeRelease(ComPtr<T> &t) {
  if (t) {
    t.Reset();
    t = nullptr;
  }
}
// release a group of ComPtrs
template <typename... Ts> void SafeReleaseAll(Ts &&...args) {
  (SafeRelease(std::forward<Ts>(args)), ...);
}

inline BOOL GetVersionEx2(LPOSVERSIONINFOW lpVersionInformation) {
  HMODULE hNtDll = GetModuleHandleW(L"NTDLL"); // 获取ntdll.dll的句柄
  typedef NTSTATUS(NTAPI * tRtlGetVersion)(PRTL_OSVERSIONINFOW povi);
  tRtlGetVersion pRtlGetVersion =
      hNtDll ? (tRtlGetVersion)GetProcAddress(hNtDll, "RtlGetVersion") : 0;
  return (pRtlGetVersion &&
          pRtlGetVersion((PRTL_OSVERSIONINFOW)lpVersionInformation) >= 0);
}

inline BOOL IsWinVersionGreaterThan(DWORD major, DWORD minor) {
  OSVERSIONINFOEXW ovi = {sizeof ovi};
  GetVersionEx2((LPOSVERSIONINFOW)&ovi);
  return ((ovi.dwMajorVersion == major && ovi.dwMinorVersion >= minor) ||
          ovi.dwMajorVersion > major);
}

// Use WinBlue for Windows 8.1
#define IsWindowsBlueOrLaterEx() IsWinVersionGreaterThan(6, 3)

} // namespace weasel
