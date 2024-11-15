﻿#pragma once

#include <chrono>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl.h>

namespace weasel {

using namespace Microsoft::WRL;
using wstring = std::wstring;
using string = std::string;
template <typename T> using vector = std::vector<T>;

struct ComException {
  HRESULT result;
  ComException(HRESULT const value) : result(value) {}
};
inline void HR(HRESULT const result) {
  if (S_OK != result) {
    throw ComException(result);
  }
}

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

#define wtou8(x) wstring_to_string(x, CP_UTF8)
#define wtoacp(x) wstring_to_string(x)
#define u8tow(x) string_to_wstring(x, CP_UTF8)
#define acptow(x) string_to_wstring(x, CP_ACP)
#define MAX(x, y) ((x > y) ? x : y)

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

private:
  std::wstringstream ss;
};
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
#define DEBUG                                                                  \
  (DebugStream() << "[" << current_time() << " " << __FILE__ << ":"            \
                 << __LINE__ << "] ")
} // namespace weasel
