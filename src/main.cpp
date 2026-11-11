#include "platform.h"
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
constexpr wchar_t ClassName[] = L"KuanfuWeimu.WindowPin.Main";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr UINT TrayMessage = WM_APP + 1, ShowMessage = WM_APP + 2;
enum Control { TitleLabel = 100, Subtitle, Picker, Search, Refresh, Windows, Toggle,
    Restore, ShortcutLabel, Shortcut, Apply, Startup, StartHidden, Language, Status,
    Footer, Hide, Quit, Show = 300 };
struct Config { UINT modifiers = MOD_CONTROL; UINT key = VK_F8; bool hidden = false; bool chinese = false; };

std::wstring ExePath() {
    std::wstring path(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(n);
    return path;
}
std::wstring ReadText(HWND h) {
    const int n = GetWindowTextLengthW(h);
    std::wstring s(static_cast<std::size_t>(n) + 1, L'\0');
    const int copied = GetWindowTextW(h, s.data(), n + 1);
    s.resize(static_cast<std::size_t>(std::max(0, copied)));
    return s;
}
std::filesystem::path IniPath() { return std::filesystem::path(ExePath()).parent_path() / L"WindowPin.ini"; }
bool ValidHotkey(UINT mods, UINT key) {
    return (mods & (MOD_CONTROL | MOD_ALT)) && !(mods & ~(MOD_CONTROL | MOD_ALT | MOD_SHIFT)) &&
        ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || (key >= VK_F1 && key <= VK_F11));
}
Config LoadConfig() {
    Config c;
    const auto path = IniPath();
    c.chinese = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
    c.modifiers = GetPrivateProfileIntW(L"WindowPin", L"Modifiers", c.modifiers, path.c_str());
    c.key = GetPrivateProfileIntW(L"WindowPin", L"Key", c.key, path.c_str());
    c.hidden = GetPrivateProfileIntW(L"WindowPin", L"StartHidden", 0, path.c_str()) != 0;
    c.chinese = GetPrivateProfileIntW(L"WindowPin", L"Chinese", c.chinese ? 1 : 0, path.c_str()) != 0;
    if (!ValidHotkey(c.modifiers, c.key)) { c.modifiers = MOD_CONTROL; c.key = VK_F8; }
    return c;
}
bool SaveConfig(const Config& c) {
    const auto path = IniPath();
    auto temp = path; temp += L".tmp";
    const std::string text = "[WindowPin]\r\nModifiers=" + std::to_string(c.modifiers) +
        "\r\nKey=" + std::to_string(c.key) + "\r\nStartHidden=" + (c.hidden ? "1" : "0") +
        "\r\nChinese=" + (c.chinese ? "1" : "0") + "\r\n";
    { std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
      stream << text; stream.close(); if (!stream) return false; }
    if (MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(temp.c_str());
    return false;
}
std::wstring StartupCommand() { return L"\"" + ExePath() + L"\" --tray"; }
bool StartupEnabled() {
    wchar_t value[32768]{};
    DWORD bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, RunKey, L"WindowPin", RRF_RT_REG_SZ, nullptr, value, &bytes) != ERROR_SUCCESS) return false;
    return value == StartupCommand();
}
bool SetStartup(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    const auto command = StartupCommand();
    const LSTATUS result = enabled ? RegSetValueExW(key, L"WindowPin", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) :
        RegDeleteValueW(key, L"WindowPin");
    RegCloseKey(key);
    return result == ERROR_SUCCESS || (!enabled && result == ERROR_FILE_NOT_FOUND);
}
WORD HotkeyWord(const Config& c) {
    BYTE modifiers = 0;
    if (c.modifiers & MOD_CONTROL) modifiers |= HOTKEYF_CONTROL;
    if (c.modifiers & MOD_ALT) modifiers |= HOTKEYF_ALT;
    if (c.modifiers & MOD_SHIFT) modifiers |= HOTKEYF_SHIFT;
    return MAKEWORD(static_cast<BYTE>(c.key), modifiers);
}
std::wstring HotkeyText(const Config& c) {
    std::wstring s;
    if (c.modifiers & MOD_CONTROL) s += L"Ctrl + ";
    if (c.modifiers & MOD_ALT) s += L"Alt + ";
    if (c.modifiers & MOD_SHIFT) s += L"Shift + ";
    if (c.key >= VK_F1 && c.key <= VK_F11) s += L"F" + std::to_wstring(c.key - VK_F1 + 1);
    else s += static_cast<wchar_t>(c.key);
    return s;
}

struct App {
    HINSTANCE instance = nullptr;
    HWND main = nullptr;
    Config config = LoadConfig();
    wp::WinBackend backend;
    wp::PinManager pins{backend};
    HFONT font = nullptr, heading = nullptr;
    HICON icon = nullptr;
    HBRUSH background = CreateSolidBrush(RGB(248, 249, 251));
    NOTIFYICONDATAW tray{};
    bool trayAdded = false, dragging = false, quitting = false, isolatedTest = false;
    UINT dpi = 96, taskbarCreated = 0;
    int hotkeyId = 1;
    bool hotkeyRegistered = false;
    std::vector<wp::WindowInfo> rows;
    std::wstring renderedFilter;
    bool renderedChinese = false;
    std::size_t renderedChanges = static_cast<std::size_t>(-1);

    const wchar_t* T(const wchar_t* zh, const wchar_t* en) const { return config.chinese ? zh : en; }
    HWND H(int id) const { return GetDlgItem(main, id); }
    int Px(int n) const { return MulDiv(n, static_cast<int>(dpi), 96); }
    void Text(int id, const wchar_t* zh, const wchar_t* en) { SetWindowTextW(H(id), T(zh, en)); }
    void StatusText(const std::wstring& s, bool notify = false, bool error = false) {
        SetWindowTextW(H(Status), s.c_str());
        if (notify && !IsWindowVisible(main) && trayAdded) {
            tray.uFlags = NIF_INFO;
            wcsncpy_s(tray.szInfoTitle, L"WindowPin", _TRUNCATE);
            wcsncpy_s(tray.szInfo, s.c_str(), _TRUNCATE);
            tray.dwInfoFlags = error ? NIIF_WARNING : NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &tray);
        }
    }
    void Save() {
        if (isolatedTest) return;
        if (!SaveConfig(config)) StatusText(T(L"设置未能保存。请把整个文件夹放在可写的位置。", L"Could not save settings. Move the folder to a writable location."), true, true);
    }
    HWND Add(int id, const wchar_t* cls, DWORD style = 0, DWORD ex = 0) {
        return CreateWindowExW(ex, cls, L"", WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            main, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    }
    void Fonts() {
        if (font) DeleteObject(font);
        if (heading) DeleteObject(heading);
        font = CreateFontW(-Px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        heading = CreateFontW(-Px(28), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        for (HWND h = GetWindow(main, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT))
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(H(TitleLabel), WM_SETFONT, reinterpret_cast<WPARAM>(heading), TRUE);
    }
    void Place(int id, int x, int y, int w, int h) { MoveWindow(H(id), Px(x), Px(y), Px(w), Px(h), TRUE); }
    void Layout() {
        RECT r{}; GetClientRect(main, &r);
        const int w = MulDiv(r.right, 96, static_cast<int>(dpi));
        const int h = MulDiv(r.bottom, 96, static_cast<int>(dpi));
        const int bottom = h - 245;
        Place(TitleLabel, 24, 20, w - 48, 38); Place(Subtitle, 24, 62, w - 48, 24);
        Place(Picker, 24, 102, w - 48, 72);
        Place(Search, 24, 194, w - 148, 30); Place(Refresh, w - 112, 194, 88, 30);
        Place(Windows, 24, 238, w - 48, std::max(90, bottom - 248));
        Place(Toggle, 24, bottom, 154, 32); Place(Restore, 190, bottom, 154, 32);
        Place(ShortcutLabel, 24, bottom + 50, 85, 26); Place(Shortcut, 114, bottom + 46, 190, 30);
        Place(Apply, 316, bottom + 46, 84, 30);
        Place(Language, w - 164, bottom + 46, 140, 200);
        Place(Startup, 24, bottom + 88, 244, 26); Place(StartHidden, 282, bottom + 88, w - 306, 26);
        Place(Status, 24, bottom + 126, w - 48, 42);
        Place(Footer, 24, h - 66, w - 252, 50);
        Place(Hide, w - 216, h - 60, 92, 34); Place(Quit, w - 112, h - 60, 88, 34);
        ListView_SetColumnWidth(H(Windows), 0, Px(110));
        const int titleWidth = std::max(Px(160), static_cast<int>(r.right) - Px(182));
        ListView_SetColumnWidth(H(Windows), 1, titleWidth);
    }
    void Labels() {
        SetWindowTextW(H(TitleLabel), L"WindowPin");
        const auto subtitle = std::wstring(T(L"让重要窗口留在视线里。  ", L"Keep the window you need in view.  ")) + HotkeyText(config);
        SetWindowTextW(H(Subtitle), subtitle.c_str());
        Text(Picker, L"按住这里，拖到目标窗口", L"Hold here and drag onto a window");
        SendMessageW(H(Search), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(T(L"搜索窗口名称", L"Search window titles")));
        Text(Refresh, L"刷新", L"Refresh"); Text(Toggle, L"置顶 / 取消置顶", L"Toggle on top");
        Text(Restore, L"还原所有修改", L"Restore all"); Text(ShortcutLabel, L"快捷键", L"Shortcut");
        Text(Apply, L"应用", L"Apply"); Text(Startup, L"登录 Windows 时启动", L"Start when I sign in");
        Text(StartHidden, L"启动时隐藏到托盘", L"Start in tray");
        Text(Footer, L"关闭窗口后继续在托盘运行。\n退出会还原本次修改。", L"Closing keeps WindowPin in the tray.\nQuitting restores this session's changes.");
        Text(Hide, L"隐藏", L"Hide"); Text(Quit, L"退出", L"Quit");
        LVCOLUMNW col{}; col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(T(L"状态", L"State")); ListView_SetColumn(H(Windows), 0, &col);
        col.pszText = const_cast<wchar_t*>(T(L"窗口", L"Window")); ListView_SetColumn(H(Windows), 1, &col);
        InvalidateRect(H(Picker), nullptr, TRUE);
    }
    HWND Selected() const {
        const int i = ListView_GetNextItem(H(Windows), -1, LVNI_SELECTED);
        if (i < 0) return nullptr;
        LVITEMW item{}; item.mask = LVIF_PARAM; item.iItem = i;
        return ListView_GetItem(H(Windows), &item) ? reinterpret_cast<HWND>(item.lParam) : nullptr;
    }
    void RefreshList() {
        if (!H(Windows)) return;
        const HWND selected = Selected();
        const int firstVisible = ListView_GetTopIndex(H(Windows));
        pins.Prune();
        auto nextRows = isolatedTest ? std::vector<wp::WindowInfo>{} : wp::Enumerate();
        const auto filter = ReadText(H(Search));
        const auto changes = pins.Count();
        const bool unchanged = nextRows.size() == rows.size() &&
            std::equal(nextRows.begin(), nextRows.end(), rows.begin(), [](const auto& a, const auto& b) {
                return a.hwnd == b.hwnd && a.title == b.title && a.topmost == b.topmost;
            });
        if (unchanged && filter == renderedFilter && config.chinese == renderedChinese && changes == renderedChanges) return;
        rows = std::move(nextRows); renderedFilter = filter; renderedChinese = config.chinese; renderedChanges = changes;
        SendMessageW(H(Windows), WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(H(Windows));
        int index = 0;
        for (const auto& row : rows) {
            if (!filter.empty() && FindStringOrdinal(FIND_FROMSTART, row.title.c_str(), -1, filter.c_str(), -1, TRUE) < 0) continue;
            std::wstring state = row.topmost ? T(L"已置顶", L"On top") : T(L"普通", L"Normal");
            if (pins.Changed(wp::Id(row.hwnd))) state += L" *";
            LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = index;
            item.pszText = state.data(); item.lParam = reinterpret_cast<LPARAM>(row.hwnd);
            ListView_InsertItem(H(Windows), &item);
            ListView_SetItemText(H(Windows), index, 1, const_cast<wchar_t*>(row.title.c_str()));
            if (row.hwnd == selected) ListView_SetItemState(H(Windows), index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ++index;
        }
        if (index > 0 && firstVisible > 0) {
            RECT itemRect{};
            if (ListView_GetItemRect(H(Windows), 0, &itemRect, LVIR_BOUNDS))
                ListView_Scroll(H(Windows), 0, std::min(firstVisible, index - 1) * (itemRect.bottom - itemRect.top));
        }
        SendMessageW(H(Windows), WM_SETREDRAW, TRUE, 0); InvalidateRect(H(Windows), nullptr, TRUE);
        EnableWindow(H(Toggle), Selected() != nullptr);
        EnableWindow(H(Restore), pins.Count() > 0);
    }
    void ToggleWindow(HWND target) {
        target = wp::Root(target);
        if (!wp::Eligible(target)) {
            StatusText(T(L"请选择一个可见的应用窗口。", L"Choose a visible application window."), true, true); return;
        }
        const auto title = wp::Title(target);
        const auto result = pins.Toggle(wp::Id(target));
        if (result == wp::Result::Pinned || result == wp::Result::Unpinned) {
            StatusText(std::wstring(result == wp::Result::Pinned ? T(L"已置顶：", L"Pinned: ") : T(L"已取消置顶：", L"Unpinned: ")) + title, true);
        } else {
            StatusText(T(L"窗口拒绝修改或已经关闭。管理员应用需要手动以管理员身份运行 WindowPin。",
                L"Window closed or denied the change. For elevated apps, run WindowPin as administrator manually."), true, true);
        }
        RefreshList();
    }
    void AddTray() {
        if (isolatedTest) return;
        tray = {}; tray.cbSize = sizeof(tray); tray.hWnd = main; tray.uID = 1;
        tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        tray.uCallbackMessage = TrayMessage; tray.hIcon = icon;
        wcscpy_s(tray.szTip, L"WindowPin");
        trayAdded = Shell_NotifyIconW(NIM_ADD, &tray) != FALSE;
        if (trayAdded) { tray.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &tray); }
    }
    void ShowWindowPanel() { ShowWindow(main, SW_RESTORE); SetForegroundWindow(main); RefreshList(); }
    void HideWindowPanel() {
        if (!trayAdded) AddTray();
        if (trayAdded) ShowWindow(main, SW_HIDE);
        else StatusText(T(L"托盘不可用，窗口保持打开。", L"The system tray is unavailable; keeping this window open."));
    }
    void TrayMenu(POINT pt) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, Show, T(L"打开 WindowPin", L"Open WindowPin"));
        AppendMenuW(menu, MF_STRING | (pins.Count() ? MF_ENABLED : MF_GRAYED), Restore, T(L"还原所有修改", L"Restore all changes"));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, Quit, T(L"退出", L"Quit"));
        SetForegroundWindow(main);
        const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, main, nullptr));
        DestroyMenu(menu); PostMessageW(main, WM_NULL, 0, 0);
        if (command) Command(command, 0);
    }
    void SetShortcut() {
        const WORD value = static_cast<WORD>(SendMessageW(H(Shortcut), HKM_GETHOTKEY, 0, 0));
        Config next = config; next.key = LOBYTE(value); next.modifiers = 0;
        if (HIBYTE(value) & HOTKEYF_CONTROL) next.modifiers |= MOD_CONTROL;
        if (HIBYTE(value) & HOTKEYF_ALT) next.modifiers |= MOD_ALT;
        if (HIBYTE(value) & HOTKEYF_SHIFT) next.modifiers |= MOD_SHIFT;
        if (!ValidHotkey(next.modifiers, next.key)) {
            StatusText(T(L"请使用 Ctrl 或 Alt，加字母、数字或 F1–F11。", L"Use Ctrl or Alt with a letter, number or F1–F11.")); return;
        }
        if (hotkeyRegistered && next.modifiers == config.modifiers && next.key == config.key) {
            StatusText(T(L"这个快捷键已经启用。", L"This shortcut is already active.")); return;
        }
        const int nextId = hotkeyId == 1 ? 2 : 1;
        if (!RegisterHotKey(main, nextId, next.modifiers | MOD_NOREPEAT, next.key)) {
            StatusText(T(L"快捷键已被其他程序占用。请换一个组合，原快捷键保持不变。", L"Shortcut is in use. Choose another combination; the previous shortcut is unchanged."), true, true); return;
        }
        if (hotkeyRegistered) UnregisterHotKey(main, hotkeyId);
        hotkeyId = nextId; hotkeyRegistered = true; config = next;
        StatusText(T(L"快捷键已更新。", L"Shortcut updated.")); Labels(); Save();
    }
    void RestoreChanges() {
        const auto failed = pins.RestoreAll(); RefreshList();
        StatusText(failed ? T(L"部分窗口拒绝还原，可再次尝试。", L"Some windows could not be restored. Try again.") :
            T(L"已还原本次修改。", L"This session's changes have been restored."), true, failed != 0);
    }
    void Exit(bool force = false) {
        const auto failed = pins.RestoreAll();
        if (failed && !force) {
            ShowWindowPanel();
            if (MessageBoxW(main, T(L"部分窗口无法恢复。仍要退出吗？这些窗口可能保持当前置顶状态。",
                L"Some windows could not be restored. Quit anyway? They may retain their current on-top state."),
                L"WindowPin", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
        }
        pins.Forget(); quitting = true; DestroyWindow(main);
    }
    void Command(UINT id, UINT notification) {
        switch (id) {
        case Refresh: RefreshList(); break;
        case Search: if (notification == EN_CHANGE) RefreshList(); break;
        case Toggle: ToggleWindow(Selected()); break;
        case Restore: RestoreChanges(); break;
        case Apply: SetShortcut(); break;
        case Startup: {
            const bool wanted = Button_GetCheck(H(Startup)) == BST_CHECKED;
            if (!SetStartup(wanted)) {
                Button_SetCheck(H(Startup), StartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
                StatusText(T(L"无法更改登录启动设置。", L"Could not change sign-in startup."), true, true);
            } else StatusText(wanted ? T(L"已启用登录启动。", L"Sign-in startup enabled.") : T(L"已关闭登录启动。", L"Sign-in startup disabled."));
            break;
        }
        case StartHidden: config.hidden = Button_GetCheck(H(StartHidden)) == BST_CHECKED; Save(); break;
        case Language:
            if (notification == CBN_SELCHANGE) {
                config.chinese = SendMessageW(H(Language), CB_GETCURSEL, 0, 0) == 0;
                Labels(); RefreshList(); StatusText(T(L"语言已更新。", L"Language updated.")); Save();
            } break;
        case Hide: HideWindowPanel(); break;
        case Show: ShowWindowPanel(); break;
        case Quit: Exit(); break;
        }
    }
    void DrawPicker(const DRAWITEMSTRUCT& item) {
        const COLORREF fill = dragging ? RGB(225, 233, 247) : RGB(236, 241, 250);
        HBRUSH brush = CreateSolidBrush(fill); FillRect(item.hDC, &item.rcItem, brush); DeleteObject(brush);
        const int cx = Px(35), cy = (item.rcItem.bottom - item.rcItem.top) / 2;
        HPEN pen = CreatePen(PS_SOLID, Px(2), RGB(47, 89, 173));
        HGDIOBJ oldPen = SelectObject(item.hDC, pen), oldBrush = SelectObject(item.hDC, GetStockObject(NULL_BRUSH));
        Ellipse(item.hDC, cx - Px(10), cy - Px(10), cx + Px(10), cy + Px(10));
        MoveToEx(item.hDC, cx - Px(16), cy, nullptr); LineTo(item.hDC, cx + Px(16), cy);
        MoveToEx(item.hDC, cx, cy - Px(16), nullptr); LineTo(item.hDC, cx, cy + Px(16));
        SelectObject(item.hDC, oldPen); SelectObject(item.hDC, oldBrush); DeleteObject(pen);
        SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, RGB(30, 54, 100));
        HGDIOBJ oldFont = SelectObject(item.hDC, font);
        RECT text = item.rcItem; text.left = Px(66); text.right -= Px(14);
        const auto label = ReadText(H(Picker)); DrawTextW(item.hDC, label.c_str(), -1, &text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (item.itemState & ODS_FOCUS) { RECT focus = item.rcItem; InflateRect(&focus, -Px(3), -Px(3)); DrawFocusRect(item.hDC, &focus); }
        SelectObject(item.hDC, oldFont);
    }
    void Init();
    ~App() { if (font) DeleteObject(font); if (heading) DeleteObject(heading); if (icon) DestroyIcon(icon); DeleteObject(background); }
};
App* app = nullptr;

LRESULT CALLBACK PickerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        SetFocus(hwnd); app->dragging = true; SetCapture(hwnd); SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        InvalidateRect(hwnd, nullptr, TRUE); return 0;
    case WM_MOUSEMOVE:
        if (app->dragging) {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            POINT pt{}; GetCursorPos(&pt); HWND target = wp::Root(WindowFromPoint(pt));
            app->StatusText(wp::Eligible(target) ? wp::Title(target) : app->T(L"拖到目标窗口，松开切换置顶。Esc 取消。", L"Drag to a window and release to toggle. Esc cancels."));
            return 0;
        } break;
    case WM_LBUTTONUP:
        if (app->dragging) {
            POINT pt{}; GetCursorPos(&pt); HWND target = WindowFromPoint(pt);
            app->dragging = false; ReleaseCapture(); InvalidateRect(hwnd, nullptr, TRUE);
            app->ToggleWindow(target); return 0;
        } break;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE && app->dragging) {
            app->dragging = false; ReleaseCapture(); InvalidateRect(hwnd, nullptr, TRUE);
            app->StatusText(app->T(L"已取消选窗。", L"Window selection cancelled.")); return 0;
        } break;
    case WM_CAPTURECHANGED:
        app->dragging = false; InvalidateRect(hwnd, nullptr, TRUE); break;
    case WM_SETCURSOR:
        if (app->dragging) { SetCursor(LoadCursorW(nullptr, IDC_CROSS)); return TRUE; } break;
    }
    return DefSubclassProc(hwnd, msg, w, l);
}
void App::Init() {
    dpi = GetDpiForWindow(main);
    Add(TitleLabel, L"STATIC"); Add(Subtitle, L"STATIC");
    Add(Picker, L"BUTTON", BS_OWNERDRAW | WS_TABSTOP);
    SetWindowSubclass(H(Picker), PickerProc, 1, 0);
    Add(Search, L"EDIT", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
    Add(Refresh, L"BUTTON", WS_TABSTOP);
    Add(Windows, WC_LISTVIEWW, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(H(Windows), LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(L"State"); col.cx = 110;
    ListView_InsertColumn(H(Windows), 0, &col); col.pszText = const_cast<wchar_t*>(L"Window"); col.cx = 450;
    ListView_InsertColumn(H(Windows), 1, &col);
    Add(Toggle, L"BUTTON", WS_TABSTOP); Add(Restore, L"BUTTON", WS_TABSTOP);
    Add(ShortcutLabel, L"STATIC"); Add(Shortcut, HOTKEY_CLASSW, WS_TABSTOP, WS_EX_CLIENTEDGE);
    SendMessageW(H(Shortcut), HKM_SETHOTKEY, HotkeyWord(config), 0); Add(Apply, L"BUTTON", WS_TABSTOP);
    Add(Startup, L"BUTTON", BS_AUTOCHECKBOX | WS_TABSTOP);
    Button_SetCheck(H(Startup), !isolatedTest && StartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
    Add(StartHidden, L"BUTTON", BS_AUTOCHECKBOX | WS_TABSTOP);
    Button_SetCheck(H(StartHidden), config.hidden ? BST_CHECKED : BST_UNCHECKED);
    Add(Language, WC_COMBOBOXW, CBS_DROPDOWNLIST | WS_TABSTOP);
    SendMessageW(H(Language), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"简体中文"));
    SendMessageW(H(Language), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(H(Language), CB_SETCURSEL, config.chinese ? 0 : 1, 0);
    Add(Status, L"STATIC"); Add(Footer, L"STATIC"); Add(Hide, L"BUTTON", WS_TABSTOP); Add(Quit, L"BUTTON", WS_TABSTOP);
    Fonts(); Labels(); Layout(); RefreshList();
    icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, 0));
    SendMessageW(main, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon)); SendMessageW(main, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated"); AddTray();
    hotkeyRegistered = !isolatedTest && RegisterHotKey(main, hotkeyId, config.modifiers | MOD_NOREPEAT, config.key) != FALSE;
    StatusText(hotkeyRegistered ? T(L"选择窗口，或直接使用快捷键。* 表示本次会话的修改。", L"Select a window or use the shortcut. * marks this session's changes.") :
        T(L"快捷键被占用，请更换组合。拖动选窗和列表仍可使用。", L"Shortcut is in use. Change it below; dragging and the list still work."), !hotkeyRegistered, !hotkeyRegistered);
    if (!isolatedTest) SetTimer(main, 1, 2000, nullptr);
}
LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_NCCREATE) { app->main = hwnd; return TRUE; }
    if (app->taskbarCreated && msg == app->taskbarCreated) { app->AddTray(); if (!app->trayAdded) app->ShowWindowPanel(); return 0; }
    switch (msg) {
    case WM_CREATE: app->Init(); return 0;
    case WM_COMMAND: app->Command(LOWORD(w), HIWORD(w)); return 0;
    case WM_HOTKEY: if (static_cast<int>(w) == app->hotkeyId && !app->dragging) app->ToggleWindow(GetForegroundWindow()); return 0;
    case WM_TIMER: if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && !app->dragging) app->RefreshList(); return 0;
    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<NMHDR*>(l);
        if (header->idFrom == Windows) {
            if (header->code == NM_DBLCLK) app->ToggleWindow(app->Selected());
            if (header->code == LVN_ITEMCHANGED) EnableWindow(app->H(Toggle), app->Selected() != nullptr);
        } return 0;
    }
    case WM_DRAWITEM: if (w == Picker) { app->DrawPicker(*reinterpret_cast<DRAWITEMSTRUCT*>(l)); return TRUE; } break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(w); SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, reinterpret_cast<HWND>(l) == app->H(TitleLabel) ? RGB(22, 30, 44) : RGB(75, 85, 100));
        return reinterpret_cast<LRESULT>(app->background);
    }
    case WM_CTLCOLORBTN: return reinterpret_cast<LRESULT>(app->background);
    case WM_ERASEBKGND: { RECT r{}; GetClientRect(hwnd, &r); FillRect(reinterpret_cast<HDC>(w), &r, app->background); return TRUE; }
    case WM_SIZE: if (app->H(Quit)) app->Layout(); return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(l); info->ptMinTrackSize = {app->Px(680), app->Px(680)}; return 0;
    }
    case WM_DPICHANGED: {
        app->dpi = HIWORD(w); app->Fonts(); const auto* r = reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        app->Layout(); return 0;
    }
    case ShowMessage: app->ShowWindowPanel(); return 0;
    case TrayMessage: {
        const UINT event = LOWORD(l);
        if (event == NIN_SELECT || event == NIN_KEYSELECT) app->ShowWindowPanel();
        else if (event == WM_CONTEXTMENU) { POINT pt{GET_X_LPARAM(w), GET_Y_LPARAM(w)}; app->TrayMenu(pt); }
        return 0;
    }
    case WM_CLOSE: app->HideWindowPanel(); return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if (w) app->Exit(true); return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (app->hotkeyRegistered) UnregisterHotKey(hwnd, app->hotkeyId);
        if (app->trayAdded) Shell_NotifyIconW(NIM_DELETE, &app->tray);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
}

#ifndef WINDOWPIN_UI_TEST
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\KuanfuWeimu.WindowPin");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(ClassName, nullptr)) PostMessageW(existing, ShowMessage, 0, 0);
        CloseHandle(mutex); return 0;
    }
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES}; InitCommonControlsEx(&common);
    App state; app = &state; state.instance = instance;
    WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.hInstance = instance; cls.lpfnWndProc = MainProc;
    cls.lpszClassName = ClassName; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    if (!RegisterClassExW(&cls)) { CloseHandle(mutex); return 1; }
    const UINT dpi = GetDpiForSystem(); state.dpi = dpi;
    HWND main = CreateWindowExW(WS_EX_CONTROLPARENT, ClassName, L"WindowPin", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(760, static_cast<int>(dpi), 96), MulDiv(730, static_cast<int>(dpi), 96),
        nullptr, nullptr, instance, nullptr);
    if (!main) { CloseHandle(mutex); return 1; }
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool hidden = state.config.hidden;
    for (int i = 1; argv && i < argc; ++i) if (wcscmp(argv[i], L"--tray") == 0) hidden = true;
    if (argv) LocalFree(argv);
    if (!hidden || !state.trayAdded || !state.hotkeyRegistered) ShowWindow(main, SW_SHOWNORMAL);
    MSG msg{}; BOOL received = 0;
    while ((received = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        // Preserve Escape cancellation in the capture control before dialog navigation.
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && state.dragging) { DispatchMessageW(&msg); continue; }
        if (!IsDialogMessageW(main, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (!state.quitting) { state.pins.RestoreAll(); state.pins.Forget(); }
    CloseHandle(mutex);
    return received == -1 ? 1 : static_cast<int>(msg.wParam);
}
#endif
