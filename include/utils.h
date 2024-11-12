#pragma once

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

#define DEBUG (DebugStream() << __FILE__ << "@L" << __LINE__ << ": ")
} // namespace weasel
