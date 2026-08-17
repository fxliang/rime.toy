#include "caret.h"
#include "caret_shellcode.h"

#include <UIAutomation.h>
#include <oleacc.h>

#include <ShellScalingApi.h>
#include <combaseapi.h>
#include <psapi.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

// Auto caret positioning algorithm, ported from rabbit's GetCaretPosEx.ahk
// (https://github.com/Tebayaki/AutoHotkeyScripts, MIT License) and its use in
// Lib/RabbitCaret.ahk. It probes, in order:
//   1. GetGUIThreadInfo caret (native edit controls).
//   2. MSAA caret (IAccessible accLocation) or UIA caret, depending on the
//      window class.
//   3. A caret hook that runs UIA in-process inside the target window.
namespace caret {
namespace {

bool g_use_caret_hook = true;

// ---------------------------------------------------------------------------
// DPI helpers. The frontend runs per-monitor DPI aware, so SetWindowPos uses
// physical pixels. UIA/MSAA reported from our process are already physical.
// The caret hook runs inside the target process, whose coordinates may be
// DPI-virtualized, so they need to be scaled back to physical pixels.

UINT GetDpiForWindowCompat(HWND hwnd) {
  if (!hwnd)
    return 0;
  using GetDpiForWindowFunc = UINT(WINAPI *)(HWND);
  static auto const pfn = reinterpret_cast<GetDpiForWindowFunc>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
  return pfn ? pfn(hwnd) : 0;
}

void PhysicalToScreenRect(HWND hwnd, RECT *r) {
  HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {0};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(hmon, &mi))
    return;
  UINT win_dpi = GetDpiForWindowCompat(hwnd);
  if (!win_dpi)
    win_dpi = 96;
  UINT monitor_dpi_x = 96, monitor_dpi_y = 96;
  if (FAILED(GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &monitor_dpi_x,
                              &monitor_dpi_y)))
    return;
  double scale = (double)monitor_dpi_x / win_dpi;
  long ml = mi.rcMonitor.left;
  long mt = mi.rcMonitor.top;
  r->left = ml + (long)llround((r->left - ml) * scale);
  r->top = mt + (long)llround((r->top - mt) * scale);
  r->right = ml + (long)llround((r->right - ml) * scale);
  r->bottom = mt + (long)llround((r->bottom - mt) * scale);
}

// ---------------------------------------------------------------------------
// 1. GetGUIThreadInfo caret.

bool GetGuiThreadInfoCaret(HWND *hwnd_out, RECT *out) {
  GUITHREADINFO gti = {0};
  gti.cbSize = sizeof(GUITHREADINFO);
  if (!GetGUIThreadInfo(0, &gti))
    return false;
  if (gti.hwndCaret && gti.rcCaret.right > gti.rcCaret.left &&
      gti.rcCaret.bottom > gti.rcCaret.top) {
    POINT tl = {gti.rcCaret.left, gti.rcCaret.top};
    POINT br = {gti.rcCaret.right, gti.rcCaret.bottom};
    if (ClientToScreen(gti.hwndCaret, &tl) &&
        ClientToScreen(gti.hwndCaret, &br)) {
      out->left = tl.x;
      out->top = tl.y;
      out->right = br.x;
      out->bottom = br.y;
      *hwnd_out = gti.hwndCaret;
      return true;
    }
  }
  *hwnd_out = gti.hwndFocus;
  return false;
}

// ---------------------------------------------------------------------------
// 2. MSAA caret.

bool GetCaretFromMsaa(HWND hwnd, RECT *out) {
  if (!hwnd)
    return false;
  IAccessible *acc = nullptr;
  if (AccessibleObjectFromWindow(hwnd, OBJID_CARET, IID_IAccessible,
                                 reinterpret_cast<void **>(&acc)) != S_OK ||
      !acc)
    return false;
  VARIANT var_child;
  VariantInit(&var_child);
  var_child.vt = VT_I4;
  var_child.lVal = CHILDID_SELF;
  long x = 0, y = 0, w = 0, h = 0;
  HRESULT hr = acc->accLocation(&x, &y, &w, &h, var_child);
  acc->Release();
  if (FAILED(hr) || w < 0 || h <= 0)
    return false;
  // accLocation reports physical screen coordinates.
  out->left = x;
  out->top = y;
  out->right = x + w;
  out->bottom = y + h;
  return true;
}

// ---------------------------------------------------------------------------
// 3. UIA caret.

bool BoundingRect(IUIAutomationTextRange *range, RECT *out) {
  SAFEARRAY *psa = nullptr;
  if (FAILED(range->GetBoundingRectangles(&psa)) || !psa)
    return false;
  double *data = nullptr;
  if (FAILED(SafeArrayAccessData(psa, reinterpret_cast<void **>(&data))) ||
      !data) {
    SafeArrayDestroy(psa);
    return false;
  }
  bool ok = false;
  long lb = 0, ub = 0;
  if (SUCCEEDED(SafeArrayGetLBound(psa, 1, &lb)) &&
      SUCCEEDED(SafeArrayGetUBound(psa, 1, &ub)) && ub - lb + 1 >= 4) {
    double x = data[lb];
    double y = data[lb + 1];
    double w = data[lb + 2];
    double h = data[lb + 3];
    out->left = static_cast<LONG>(x + 0.5);
    out->top = static_cast<LONG>(y + 0.5);
    out->right = static_cast<LONG>(x + w + 0.5);
    out->bottom = static_cast<LONG>(y + h + 0.5);
    ok = h > 0;
  }
  SafeArrayUnaccessData(psa);
  SafeArrayDestroy(psa);
  return ok;
}

// A caret range is degenerate (zero width). Use the line unit for a stable
// vertical position/height, and the character unit (or the collapsed caret
// itself) for the horizontal position; this avoids the y-jitter caused by
// characters of varying height. The range must be cloned before each
// expansion: ExpandToEnclosingUnit mutates the range in place, so expanding
// the same object twice would leave the second expansion a no-op and make the
// horizontal position stick to the line's left edge.
bool RangeRect(IUIAutomationTextRange *range, RECT *out) {
  RECT caret = {0};
  bool have_caret = BoundingRect(range, &caret);

  IUIAutomationTextRange *line_range = nullptr;
  RECT line = {0};
  bool have_line = false;
  if (SUCCEEDED(range->Clone(&line_range)) && line_range) {
    if (SUCCEEDED(line_range->ExpandToEnclosingUnit(TextUnit_Line)))
      have_line = BoundingRect(line_range, &line);
    line_range->Release();
  }

  IUIAutomationTextRange *chr_range = nullptr;
  RECT chr = {0};
  bool have_chr = false;
  if (SUCCEEDED(range->Clone(&chr_range)) && chr_range) {
    if (SUCCEEDED(chr_range->ExpandToEnclosingUnit(TextUnit_Character)))
      have_chr = BoundingRect(chr_range, &chr);
    chr_range->Release();
  }

  if (!have_line && !have_chr && !have_caret)
    return false;

  if (!have_line) {
    *out = have_caret ? caret : chr;
    out->right = out->left;
    return true;
  }

  out->top = line.top;
  out->bottom = line.bottom;
  if (have_caret) {
    out->left = caret.left; // exact caret x
  } else if (have_chr && chr.top < line.bottom && chr.bottom > line.top) {
    out->left = chr.left; // caret sits within this line
  } else {
    out->left = line.right; // caret at end of line
  }
  out->right = out->left;
  return true;
}

bool GetCaretFromUia(RECT *out) {
  IUIAutomation *automation = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr,
                              CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation),
                              reinterpret_cast<void **>(&automation))) ||
      !automation)
    return false;

  bool ok = false;
  IUIAutomationElement *focused = nullptr;
  if (SUCCEEDED(automation->GetFocusedElement(&focused)) && focused) {
    IUIAutomationTextPattern2 *text2 = nullptr;
    if (SUCCEEDED(focused->GetCurrentPatternAs(
            UIA_TextPattern2Id, __uuidof(IUIAutomationTextPattern2),
            reinterpret_cast<void **>(&text2))) &&
        text2) {
      BOOL active = FALSE;
      IUIAutomationTextRange *range = nullptr;
      if (SUCCEEDED(text2->GetCaretRange(&active, &range)) && range) {
        ok = RangeRect(range, out);
        range->Release();
      }
      text2->Release();
    }
    if (!ok) {
      // No caret range exposed; fall back to the collapsed selection range.
      IUIAutomationTextPattern *text = nullptr;
      if (SUCCEEDED(focused->GetCurrentPatternAs(
              UIA_TextPatternId, __uuidof(IUIAutomationTextPattern),
              reinterpret_cast<void **>(&text))) &&
          text) {
        IUIAutomationTextRangeArray *ranges = nullptr;
        if (SUCCEEDED(text->GetSelection(&ranges)) && ranges) {
          int len = 0;
          if (SUCCEEDED(ranges->get_Length(&len)) && len > 0) {
            IUIAutomationTextRange *range = nullptr;
            if (SUCCEEDED(ranges->GetElement(len - 1, &range)) && range) {
              range->MoveEndpointByRange(TextPatternRangeEndpoint_Start, range,
                                         TextPatternRangeEndpoint_End);
              ok = RangeRect(range, out);
              range->Release();
            }
          }
          ranges->Release();
        }
        text->Release();
      }
    }
    focused->Release();
  }
  automation->Release();
  return ok;
}

// ---------------------------------------------------------------------------
// 4. Caret hook: run UIA inside the target process for apps that do not expose
//    their caret through any of the above (e.g. QQ).

bool IsX64Process(HANDLE hProcess) {
  BOOL wow64 = FALSE;
  if (IsWow64Process(hProcess, &wow64) && wow64)
    return false;
#ifdef _WIN64
  return true;
#else
  BOOL self_wow64 = FALSE;
  if (IsWow64Process(GetCurrentProcess(), &self_wow64))
    return self_wow64;
  return false;
#endif
}

// Module bases are cached per target process: enumerating modules is the
// expensive part of the hook, and base addresses do not change.
std::mutex g_bases_mutex;
std::map<DWORD, std::pair<uintptr_t, uintptr_t>> g_bases_cache;

bool GetModuleBase(HANDLE hProcess, const wchar_t *name, uintptr_t *base) {
  HMODULE mods[512];
  DWORD needed = 0;
  if (!K32EnumProcessModulesEx(hProcess, mods, sizeof(mods), &needed,
                               LIST_MODULES_ALL))
    return false;
  int count = static_cast<int>(needed / sizeof(HMODULE));
  if (count > 512)
    count = 512;
  for (int i = 0; i < count; i++) {
    wchar_t buf[256] = {0};
    if (K32GetModuleBaseNameW(hProcess, mods[i], buf, 255) &&
        _wcsicmp(buf, name) == 0) {
      *base = reinterpret_cast<uintptr_t>(mods[i]);
      return true;
    }
  }
  return false;
}

bool GetModuleBases(HANDLE hProcess, DWORD pid, uintptr_t *user32,
                    uintptr_t *combase) {
  {
    std::lock_guard<std::mutex> lk(g_bases_mutex);
    auto it = g_bases_cache.find(pid);
    if (it != g_bases_cache.end()) {
      *user32 = it->second.first;
      *combase = it->second.second;
      return true;
    }
  }
  uintptr_t u = 0, c = 0;
  if (!GetModuleBase(hProcess, L"user32.dll", &u) ||
      !GetModuleBase(hProcess, L"combase.dll", &c))
    return false;
  {
    std::lock_guard<std::mutex> lk(g_bases_mutex);
    g_bases_cache[pid] = std::make_pair(u, c);
  }
  *user32 = u;
  *combase = c;
  return true;
}

void PutU32(unsigned char *p, uint32_t v) {
  p[0] = static_cast<unsigned char>(v);
  p[1] = static_cast<unsigned char>(v >> 8);
  p[2] = static_cast<unsigned char>(v >> 16);
  p[3] = static_cast<unsigned char>(v >> 24);
}

void PutU64(unsigned char *p, uint64_t v) {
  PutU32(p, static_cast<uint32_t>(v));
  PutU32(p + 4, static_cast<uint32_t>(v >> 32));
}

// Inject the shellcode into the target and read back the caret rect. The wait
// is bounded so a hung target cannot stall the caller.
bool GetCaretFromHook(HWND hwnd, RECT *out) {
  if (!hwnd || !g_use_caret_hook)
    return false;
  static UINT WM_GET_CARET_POS = RegisterWindowMessageW(L"WM_GET_CARET_POS");

  DWORD pid = 0;
  DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
  if (!tid || !pid)
    return false;

  // Nudge the target to refresh its caret before probing.
  SendMessageTimeoutW(hwnd, WM_IME_COMPOSITION, 0, 0, SMTO_ABORTIFHUNG, 30,
                      nullptr);

  HANDLE hProcess =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  FALSE, pid);
  if (!hProcess)
    return false;

  bool is_x64 = IsX64Process(hProcess);
  uintptr_t user32_base = 0, combase_base = 0;
  if (!GetModuleBases(hProcess, pid, &user32_base, &combase_base)) {
    CloseHandle(hProcess);
    return false;
  }

  const unsigned char *sc;
  size_t sc_size;
  size_t entry_offset;
  size_t rect_offset;
  if (is_x64) {
    sc = kCaretHookShellcode64;
    sc_size = sizeof(kCaretHookShellcode64);
    entry_offset = 0x4e0;
    rect_offset = 56;
  } else {
    sc = kCaretHookShellcode32;
    sc_size = sizeof(kCaretHookShellcode32);
    entry_offset = 0x43c;
    rect_offset = 32;
  }

  std::vector<unsigned char> buf(sc, sc + sc_size);
  if (is_x64) {
    PutU64(&buf[0], user32_base);
    PutU64(&buf[8], combase_base);
    PutU64(&buf[16], reinterpret_cast<uint64_t>(hwnd));
    PutU32(&buf[24], tid);
    PutU32(&buf[28], WM_GET_CARET_POS);
  } else {
    PutU32(&buf[0], static_cast<uint32_t>(user32_base));
    PutU32(&buf[4], static_cast<uint32_t>(combase_base));
    PutU32(&buf[8], static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hwnd)));
    PutU32(&buf[12], tid);
    PutU32(&buf[16], WM_GET_CARET_POS);
  }

  void *remote = VirtualAllocEx(hProcess, nullptr, sc_size, MEM_COMMIT,
                                PAGE_EXECUTE_READWRITE);
  if (!remote) {
    CloseHandle(hProcess);
    return false;
  }

  bool result = false;
  bool thread_created = false;
  bool thread_finished = false;
  SIZE_T written = 0;
  if (WriteProcessMemory(hProcess, remote, buf.data(), sc_size, &written)) {
    FlushInstructionCache(hProcess, remote, sc_size);
    LPTHREAD_START_ROUTINE entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<unsigned char *>(remote) + entry_offset);
    HANDLE hThread =
        CreateRemoteThread(hProcess, nullptr, 0, entry, remote, 0, nullptr);
    if (hThread) {
      thread_created = true;
      thread_finished = (WaitForSingleObject(hThread, 150) == WAIT_OBJECT_0);
      if (thread_finished) {
        DWORD exit_code = 0;
        if (GetExitCodeThread(hThread, &exit_code) && exit_code == 0) {
          RECT r = {0};
          SIZE_T bytes_read = 0;
          void *prect = reinterpret_cast<unsigned char *>(remote) + rect_offset;
          if (ReadProcessMemory(hProcess, prect, &r, sizeof(r), &bytes_read) &&
              bytes_read == sizeof(r)) {
            PhysicalToScreenRect(hwnd, &r);
            *out = r;
            result = true;
          }
        }
      }
      CloseHandle(hThread);
    }
  }

  // Release the remote allocation only once the thread is done; freeing it
  // under a still-running thread would crash the target process. On timeout
  // the page is leaked (the thread finishes on its own internal timeouts).
  if (!thread_created || thread_finished)
    VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
  CloseHandle(hProcess);
  return result;
}

// The hook is too expensive to run inside the low-level keyboard hook on every
// keystroke, so its result is cached and refreshed asynchronously. The first
// probe for a window is synchronous (immediate positioning); later probes
// return the cache and trigger a background refresh.
std::mutex g_hook_mutex;
HWND g_hook_hwnd = nullptr;
RECT g_hook_rect{0};
std::chrono::steady_clock::time_point g_hook_time{};
bool g_hook_valid = false;
std::atomic<bool> g_hook_probe_inflight{false};
// The worker is kept joinable (never detached) so it can be reaped on
// shutdown. A detached thread could still be running GetCaretFromHook while
// these globals are destroyed at process exit, causing an access violation.
std::thread g_hook_worker;

void TriggerAsyncHook(HWND hwnd) {
  bool expected = false;
  if (!g_hook_probe_inflight.compare_exchange_strong(expected, true))
    return;
  // Reap the previous worker. The flag was false, so the previous thread has
  // already released the mutex and is about to exit; this join is immediate.
  if (g_hook_worker.joinable())
    g_hook_worker.join();
  g_hook_worker = std::thread([hwnd]() {
    RECT r{0};
    if (GetCaretFromHook(hwnd, &r)) {
      std::lock_guard<std::mutex> lk(g_hook_mutex);
      g_hook_rect = r;
      g_hook_hwnd = hwnd;
      g_hook_time = std::chrono::steady_clock::now();
      g_hook_valid = true;
    }
    g_hook_probe_inflight = false;
  });
}

bool GetCaretViaHook(HWND hwnd, RECT *out) {
  auto now = std::chrono::steady_clock::now();
  bool stale = false;
  {
    std::lock_guard<std::mutex> lk(g_hook_mutex);
    if (g_hook_valid && g_hook_hwnd == hwnd) {
      *out = g_hook_rect;
      stale = now - g_hook_time > std::chrono::milliseconds(300);
      if (!stale)
        return true;
    }
  }
  if (stale) {
    TriggerAsyncHook(hwnd);
    return true;
  }
  RECT r{0};
  if (!GetCaretFromHook(hwnd, &r))
    return false;
  {
    std::lock_guard<std::mutex> lk(g_hook_mutex);
    g_hook_rect = r;
    g_hook_hwnd = hwnd;
    g_hook_time = std::chrono::steady_clock::now();
    g_hook_valid = true;
  }
  *out = r;
  return true;
}

bool IsUwpClass(HWND hwnd) {
  wchar_t cls[256] = {0};
  if (!GetClassNameW(hwnd, cls, 255))
    return false;
  return wcsncmp(cls, L"Windows.UI", 10) == 0 ||
         wcsncmp(cls, L"Microsoft.UI", 12) == 0;
}

} // namespace

void SetUseCaretHook(bool enable) { g_use_caret_hook = enable; }

void Shutdown() {
  if (g_hook_worker.joinable())
    g_hook_worker.join();
}

bool GetScreenRect(RECT *out) {
  if (!out)
    return false;
  HWND hwnd = nullptr;
  if (GetGuiThreadInfoCaret(&hwnd, out))
    return true;
  if (!hwnd)
    hwnd = GetForegroundWindow();
  if (!hwnd)
    return false;

  // UWP surfaces prefer UIA; legacy apps prefer MSAA first. The hook is the
  // last resort for both.
  if (IsUwpClass(hwnd)) {
    if (GetCaretFromUia(out))
      return true;
    if (GetCaretFromMsaa(hwnd, out))
      return true;
    return GetCaretViaHook(hwnd, out);
  }
  if (GetCaretFromMsaa(hwnd, out))
    return true;
  if (GetCaretFromUia(out))
    return true;
  return GetCaretViaHook(hwnd, out);
}

} // namespace caret
