#pragma once
#include <windows.h>

// Auto caret positioning, ported from rabbit (AutoHotkey) GetCaretPosEx.
namespace caret {

// Enable or disable the caret hook fallback. The hook injects a small
// shellcode into the target process to locate carets that are not exposed via
// GUI thread info, MSAA, or UIA (e.g. QQ). Defaults to enabled; disable it if
// antivirus software flags the injection.
void SetUseCaretHook(bool enable);

// Resolve the focused window's caret into screen coordinates. Returns false
// when no caret can be located, in which case callers should fall back to
// the mouse position.
bool GetScreenRect(RECT *out);

} // namespace caret
