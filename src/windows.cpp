#include "platform.h"
#include <dwmapi.h>
#include <algorithm>
#include <cwchar>

namespace wp {
Window Id(HWND hwnd) { return reinterpret_cast<Window>(hwnd); }
WinBackend::WinBackend() : property_(L"WindowPin." + std::to_wstring(GetCurrentProcessId()) + L"." +
    std::to_wstring(GetTickCount64()) + L"." + std::to_wstring(reinterpret_cast<std::uintptr_t>(this))) {}
bool WinBackend::Inspect(Window w, Snapshot& out) {
    HWND hwnd = reinterpret_cast<HWND>(w);
    if (!IsWindow(hwnd)) return false;
    DWORD pid = 0;
    if (!GetWindowThreadProcessId(hwnd, &pid)) return false;
    out = {pid, (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0};
    return true;
}
bool WinBackend::Mark(Window w) { return SetPropW(reinterpret_cast<HWND>(w), property_.c_str(), reinterpret_cast<HANDLE>(1)) != FALSE; }
bool WinBackend::Marked(Window w) { return GetPropW(reinterpret_cast<HWND>(w), property_.c_str()) == reinterpret_cast<HANDLE>(1); }
void WinBackend::Unmark(Window w) { RemovePropW(reinterpret_cast<HWND>(w), property_.c_str()); }
bool WinBackend::SetTopmost(Window w, bool enabled) {
    HWND hwnd = reinterpret_cast<HWND>(w);
    if (!SetWindowPos(hwnd, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER)) return false;
    Snapshot s{};
    return Inspect(w, s) && s.topmost == enabled;
}
std::wstring Title(HWND hwnd) {
    const int n = GetWindowTextLengthW(hwnd);
    if (n <= 0 || n > 32767) return {};
    std::wstring result(static_cast<std::size_t>(n) + 1, L'\0');
    const int copied = GetWindowTextW(hwnd, result.data(), n + 1);
    result.resize(static_cast<std::size_t>(std::max(copied, 0)));
    return result;
}
HWND Root(HWND hwnd) { return hwnd ? GetAncestor(hwnd, GA_ROOTOWNER) : nullptr; }
bool Eligible(HWND hwnd, bool excludeSelf) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || (excludeSelf && pid == GetCurrentProcessId())) return false;
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) return false;
    wchar_t cls[128]{};
    GetClassNameW(hwnd, cls, 128);
    if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
        wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return false;
    return !Title(hwnd).empty();
}
std::vector<WindowInfo> Enumerate() {
    std::vector<WindowInfo> result;
    EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
        if (Root(hwnd) == hwnd && Eligible(hwnd)) {
            auto& rows = *reinterpret_cast<std::vector<WindowInfo>*>(data);
            rows.push_back({hwnd, Title(hwnd), (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0});
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.topmost != b.topmost) return a.topmost > b.topmost;
        return _wcsicmp(a.title.c_str(), b.title.c_str()) < 0;
    });
    return result;
}
} // namespace wp
