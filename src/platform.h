#pragma once
#include "core.h"
#include <windows.h>

namespace wp {
struct WindowInfo { HWND hwnd; std::wstring title; bool topmost; };
class WinBackend final : public Backend {
    std::wstring property_;
public:
    WinBackend();
    bool Inspect(Window, Snapshot&) override;
    bool Mark(Window) override;
    bool Marked(Window) override;
    void Unmark(Window) override;
    bool SetTopmost(Window, bool) override;
};
Window Id(HWND hwnd);
std::wstring Title(HWND hwnd);
bool Eligible(HWND hwnd, bool excludeSelf = true);
HWND Root(HWND hwnd);
std::vector<WindowInfo> Enumerate();
}
