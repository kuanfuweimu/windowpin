// Builds the actual application controls in an invisible, isolated test window.
// No tray icon, hotkey registration, enumeration, settings write or UI input.
#define WINDOWPIN_UI_TEST
#include "../src/main.cpp"
#include <iostream>
#include <stdexcept>

void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES};
        Require(InitCommonControlsEx(&common) != FALSE, "common controls initialization");
        App state; app = &state; state.isolatedTest = true; state.instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.hInstance = state.instance;
        cls.lpfnWndProc = MainProc; cls.lpszClassName = ClassName;
        Require(RegisterClassExW(&cls) != 0, "class registration");
        HWND main = CreateWindowExW(WS_EX_CONTROLPARENT, ClassName, L"WindowPin isolated UI test",
            WS_OVERLAPPEDWINDOW, 0, 0, 760, 730, nullptr, nullptr, state.instance, nullptr);
        Require(main != nullptr, "hidden main window creation");
        const int ids[] = {TitleLabel, Subtitle, Picker, Search, Refresh, Windows, Toggle, Restore,
            ShortcutLabel, Shortcut, Apply, Startup, StartHidden, Language, Status, Footer, Hide, Quit};
        int passed = 0;
        for (const UINT dpi : {96u, 144u, 192u}) {
            for (const bool chinese : {false, true}) {
                state.dpi = dpi; state.config.chinese = chinese; state.Fonts(); state.Labels();
                // Exercise both minimum and default logical client dimensions.
                for (const int width : {664, 744}) {
                    RECT rect{0, 0, state.Px(width), state.Px(690)};
                    AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_CONTROLPARENT, dpi);
                    SetWindowPos(main, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
                    state.Layout();
                    RECT client{}; GetClientRect(main, &client);
                    for (const int id : ids) {
                        HWND child = state.H(id); Require(child != nullptr, "missing control");
                        RECT bounds{}; GetWindowRect(child, &bounds);
                        MapWindowPoints(nullptr, main, reinterpret_cast<POINT*>(&bounds), 2);
                        Require(bounds.left >= 0 && bounds.top >= 0 && bounds.right <= client.right && bounds.bottom <= client.bottom,
                            "control exceeds client bounds");
                        Require(bounds.right > bounds.left && bounds.bottom > bounds.top, "empty control bounds");
                    }
                    RECT list{}, toggle{}; GetWindowRect(state.H(Windows), &list); GetWindowRect(state.H(Toggle), &toggle);
                    Require(list.bottom <= toggle.top, "window list overlaps actions");
                    Require(!IsWindowVisible(main), "test window became visible");
                    ++passed;
                }
            }
        }
        Require(!state.trayAdded && !state.hotkeyRegistered, "test registered global shell resources");
        Require(ListView_GetItemCount(state.H(Windows)) == 0, "test enumerated user windows");
        Require(ValidHotkey(MOD_CONTROL, VK_F8), "default shortcut rejected");
        Require(!ValidHotkey(0, 'A') && !ValidHotkey(MOD_CONTROL, VK_F12), "unsafe shortcut accepted");
        Require(HotkeyText(Config{}) == L"Ctrl + F8", "shortcut formatting");
        state.quitting = true; DestroyWindow(main);
        std::cout << passed << " hidden UI layout cases passed; shortcut validation passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
