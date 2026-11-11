#include "platform.h"
#include <iostream>
#include <stdexcept>
#include <functional>

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Fake final : wp::Backend {
    struct Item { std::uint32_t pid = 10; bool top = false; bool marked = false; bool fail = false; bool markFail = false; };
    std::map<wp::Window, Item> windows;
    bool Inspect(wp::Window w, wp::Snapshot& s) override {
        auto i = windows.find(w); if (i == windows.end()) return false;
        s = {i->second.pid, i->second.top}; return true;
    }
    bool Mark(wp::Window w) override { if (windows.at(w).markFail) return false; windows.at(w).marked = true; return true; }
    bool Marked(wp::Window w) override { return windows.count(w) && windows.at(w).marked; }
    void Unmark(wp::Window w) override { if (windows.count(w)) windows.at(w).marked = false; }
    bool SetTopmost(wp::Window w, bool top) override { if (!windows.count(w) || windows.at(w).fail) return false; windows.at(w).top = top; return true; }
};
struct HiddenWindow {
    HWND handle;
    explicit HiddenWindow(bool top = false) {
        handle = CreateWindowExW(top ? WS_EX_TOPMOST : 0, L"STATIC", L"WindowPin internal test window",
            WS_OVERLAPPEDWINDOW, 0, 0, 120, 80, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!handle) throw std::runtime_error("Cannot create hidden test window");
    }
    ~HiddenWindow() { if (handle) DestroyWindow(handle); }
    HiddenWindow(const HiddenWindow&) = delete;
    HiddenWindow& operator=(const HiddenWindow&) = delete;
};
int main() {
    int passed = 0, failed = 0;
    auto run = [&](const char* name, const std::function<void()>& test) {
        try { test(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    };
    run("pin and toggle back removes tracking", [] {
        Fake f; f.windows[1] = {}; wp::PinManager p(f);
        Check(p.Toggle(1) == wp::Result::Pinned && f.windows[1].top && p.Count() == 1, "pin failed");
        Check(p.Toggle(1) == wp::Result::Unpinned && !f.windows[1].top && p.Count() == 0 && !f.windows[1].marked, "toggle cleanup failed");
    });
    run("original topmost state restored", [] {
        Fake f; f.windows[1].top = true; wp::PinManager p(f);
        Check(p.Toggle(1) == wp::Result::Unpinned, "unpin failed");
        Check(p.RestoreAll() == 0 && f.windows[1].top, "original topmost lost");
    });
    run("mixed window states restored independently", [] {
        Fake f; f.windows[1] = {}; f.windows[2].top = true; wp::PinManager p(f);
        p.Toggle(1); p.Toggle(2); Check(p.Count() == 2, "tracking count");
        Check(p.RestoreAll() == 0 && !f.windows[1].top && f.windows[2].top, "mixed restore failed");
    });
    run("destroyed window removed", [] {
        Fake f; f.windows[1] = {}; wp::PinManager p(f); p.Toggle(1); f.windows.erase(1);
        Check(p.RestoreAll() == 0, "destroyed window retained");
    });
    run("recycled handle in same process is never restored", [] {
        Fake f; f.windows[1] = {}; wp::PinManager p(f); p.Toggle(1);
        f.windows[1] = {10, true, false, false, false};
        Check(p.RestoreAll() == 0 && f.windows[1].top, "recycled window modified");
    });
    run("process identity mismatch is never restored", [] {
        Fake f; f.windows[1] = {}; wp::PinManager p(f); p.Toggle(1); f.windows[1].pid = 11;
        Check(p.RestoreAll() == 0 && f.windows[1].top, "foreign process modified");
    });
    run("failed initial change rolls back marker", [] {
        Fake f; f.windows[1].fail = true; wp::PinManager p(f);
        Check(p.Toggle(1) == wp::Result::Failed && p.Count() == 0 && !f.windows[1].marked, "failed change retained marker");
    });
    run("denied marker prevents untracked changes", [] {
        Fake f; f.windows[1].markFail = true; wp::PinManager p(f);
        Check(p.Toggle(1) == wp::Result::Failed && !f.windows[1].top && p.Count() == 0, "untracked change applied");
    });
    run("failed restore remains available for retry", [] {
        Fake f; f.windows[1] = {}; wp::PinManager p(f); p.Toggle(1); f.windows[1].fail = true;
        Check(p.RestoreAll() == 1 && f.windows[1].marked, "restore failure was forgotten");
        f.windows[1].fail = false;
        Check(p.RestoreAll() == 0 && !f.windows[1].top, "restore retry failed");
    });
    run("invalid handles rejected", [] { Fake f; wp::PinManager p(f); Check(p.Toggle(0) == wp::Result::Invalid, "invalid accepted"); });
    run("forced exit drops only markers", [] {
        Fake f; f.windows[1] = {}; wp::PinManager p(f); p.Toggle(1); p.Forget();
        Check(p.Count() == 0 && !f.windows[1].marked && f.windows[1].top, "forget changed window state");
    });
    run("native hidden window pins without moving or activating", [] {
        HiddenWindow window;
        wp::WinBackend api; wp::PinManager p(api);
        RECT before{}, after{}; GetWindowRect(window.handle, &before);
        const HWND foreground = GetForegroundWindow();
        Check(p.Toggle(wp::Id(window.handle)) == wp::Result::Pinned, "native pin failed");
        GetWindowRect(window.handle, &after);
        Check(EqualRect(&before, &after) != FALSE, "window geometry changed");
        Check(!IsWindowVisible(window.handle), "test window became visible");
        Check(GetForegroundWindow() == foreground, "foreground changed");
        Check(p.RestoreAll() == 0, "native restore failed");
        Check(!(GetWindowLongPtrW(window.handle, GWL_EXSTYLE) & WS_EX_TOPMOST), "topmost bit remains");
    });
    run("native pre-existing topmost survives session", [] {
        HiddenWindow window(true); wp::WinBackend api; wp::PinManager p(api);
        Check(p.Toggle(wp::Id(window.handle)) == wp::Result::Unpinned, "native unpin failed");
        Check(p.RestoreAll() == 0, "native top restore failed");
        Check((GetWindowLongPtrW(window.handle, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0, "original native state lost");
    });
    run("native marker disappears with destroyed window", [] {
        wp::WinBackend api; wp::PinManager p(api);
        { HiddenWindow window; Check(p.Toggle(wp::Id(window.handle)) == wp::Result::Pinned, "pin failed"); }
        Check(p.Count() == 0, "destroyed native handle retained");
    });
    run("native marker namespaces are independent", [] {
        HiddenWindow window; wp::WinBackend a, b;
        Check(a.Mark(wp::Id(window.handle)), "mark failed");
        Check(a.Marked(wp::Id(window.handle)) && !b.Marked(wp::Id(window.handle)), "marker collision");
        a.Unmark(wp::Id(window.handle));
    });
    run("native hidden windows excluded from user picker", [] {
        HiddenWindow window; Check(!wp::Eligible(window.handle, false), "hidden window eligible");
        Check(!wp::Eligible(nullptr), "null eligible");
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
