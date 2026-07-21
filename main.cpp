#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

#include "Resource.h"
#include "AudioEngine.h"
#include "PlaylistManager.h"
#include "ListeningHistory.h"
#include <chrono>

namespace {
    const wchar_t CLASS_NAME[]  = L"MusicPlayerClass";
    const wchar_t WINDOW_TITLE[] = L"本地音乐播放器";
    constexpr int MIN_W = 680;
    constexpr int MIN_H = 400;
    constexpr int SEEK_RES = 10000;
    constexpr int LV_ROW_HEIGHT = 36;
}

static std::wstring FormatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int total = (int)seconds;
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    wchar_t buf[24];
    if (h > 0) swprintf(buf, 24, L"%d:%02d:%02d", h, m, s);
    else       swprintf(buf, 24, L"%02d:%02d", m, s);
    return buf;
}

static std::wstring FormatDuration(double seconds) {
    if (seconds <= 0) return L"--:--";
    return FormatTime(seconds);
}

static std::wstring GetDisplayName(const std::wstring& path) {
    size_t pos = path.rfind(L'\\');
    if (pos == std::wstring::npos) pos = path.rfind(L'/');
    std::wstring file = (pos == std::wstring::npos) ? path : path.substr(pos + 1);
    size_t dot = file.rfind(L'.');
    if (dot != std::wstring::npos) {
        std::wstring ext = file.substr(dot);
        for (auto& c : ext) c = towlower(c);
        if (ext == L".mp3" || ext == L".flac" || ext == L".wav")
            file = file.substr(0, dot);
    }
    return file;
}

static std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* last = wcsrchr(path, L'\\');
    if (last) *last = L'\0';
    return path;
}

static const wchar_t* COL_LABELS[4] = { L"#", L"标题", L"专辑", L"时长" };
static const int COL_WIDTHS[4] = { 40, 280, 180, 80 };

// Hotkey key names by array position (0-6)
static const char* HK_KEY_NAMES[7] = {
    "playpause", "prev", "next", "volup", "voldn", "restore", "minimize"
};

// HotkeyBinding - 单个快捷键配置
struct HotkeyBinding {
    int    id;
    const wchar_t* actionName;
    int    vk;
    int    mod;       // MOD_CONTROL, MOD_ALT, or combination
};

// Key name lookup tables
static const struct { int vk; const wchar_t* name; } VK_NAMES[] = {
    { VK_LEFT,  L"Left"  }, { VK_RIGHT, L"Right" },
    { VK_UP,    L"Up"    }, { VK_DOWN,  L"Down"  },
    { VK_SPACE, L"Space" }, { VK_RETURN,L"Enter" },
    { VK_TAB,   L"Tab"   }, { VK_DELETE,L"Del"   },
    { VK_ESCAPE,L"Esc"   }, { VK_BACK,  L"Back"  },
    { VK_HOME,  L"Home"  }, { VK_END,   L"End"   },
    { VK_PRIOR, L"PgUp"  }, { VK_NEXT,  L"PgDn"  },
    { VK_OEM_PLUS, L"+"  }, { VK_OEM_MINUS, L"-" },
    { 0, NULL }
};

static std::wstring VkToName(int vk) {
    for (auto& e : VK_NAMES) { if (e.vk == vk) return e.name; }
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    wchar_t buf[16]; swprintf(buf, 16, L"VK_%d", vk); return buf;
}

static int NameToVk(const std::wstring& name) {
    for (auto& e : VK_NAMES) { if (name == e.name) return e.vk; }
    if (name.size() == 1) {
        wchar_t c = name[0];
        if (c >= '0' && c <= '9') return (int)c;
        if (c >= 'A' && c <= 'Z') return (int)c;
        if (c >= 'a' && c <= 'z') return (int)(c - 32);
    }
    return 0;
}

static std::wstring HotkeyToString(int vk, int mod) {
    std::wstring s;
    if (mod & MOD_CONTROL) s += L"Ctrl+";
    if (mod & MOD_ALT)    s += L"Alt+";
    s += VkToName(vk);
    return s;
}

// Encode binding as short key: "CA+Left", "C+P", "A+Space"
static std::wstring BindingToCode(int vk, int mod) {
    std::wstring s;
    if (mod & MOD_CONTROL) s += L"C";
    if (mod & MOD_ALT)    s += L"A";
    if (!s.empty()) s += L"+";
    s += VkToName(vk);
    return s;
}

static bool CodeToBinding(const std::wstring& code, int& vk, int& mod) {
    vk = 0; mod = 0;
    size_t pos = code.find(L'+');
    if (pos == std::wstring::npos) return false;
    std::wstring modPart = code.substr(0, pos);
    std::wstring keyPart = code.substr(pos + 1);
    for (auto c : modPart) {
        if (c == L'C' || c == L'c') mod |= MOD_CONTROL;
        if (c == L'A' || c == L'a') mod |= MOD_ALT;
    }
    vk = NameToVk(keyPart);
    return vk != 0;
}

// Forward declaration
class MainWindow;

// DlgCtx - Hotkey dialog context (needed by both MainWindow and HotkeyDlgProc)
struct HKDlgCtx { HotkeyBinding* bindings; int recording; int count; MainWindow* win; int result; };

static LRESULT CALLBACK HotkeyDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

struct StatsDlgCtx {
    MainWindow* win;
    int rangeDays;
    HWND hRadio7, hRadio30, hRadioCustom, hEditCustom;
    HWND hDayList, hWeekList, hTotalText;
    HWND hDlg;
};
static LRESULT CALLBACK StatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

// MainWindow
class MainWindow {
public:
    MainWindow()
        : m_hwnd(NULL), m_hInst(NULL)
        , m_playlistLV(NULL), m_searchEdit(NULL)
        , m_btnPrev(NULL), m_btnPlay(NULL), m_btnNext(NULL), m_btnMode(NULL)
        , m_trackSeek(NULL), m_sliderVol(NULL), m_staticVolPct(NULL)
        , m_staticTime(NULL), m_staticSong(NULL)
        , m_currentIndex(-1), m_userDraggingSeek(false)
        , m_sortColumn(-1), m_sortAscending(true)
        , m_shufflePos(0)
        , m_settingsAutoplay(true), m_settingsRememberProgress(true)
        , m_settingsTray(true), m_trayIconAdded(false)
        , m_ctrlPanel(NULL)
        , m_listening(false), m_saveTick(0)
    {
        srand((unsigned)time(NULL));
        InitDefaultHotkeys();
    }

    bool Create(HINSTANCE hInst, int nCmdShow) {
        m_hInst = hInst;

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = StaticWndProc;
        wc.hInstance     = hInst;
        wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLASS_NAME;
        wc.hIconSm       = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));

        if (!RegisterClassExW(&wc)) return false;

        RECT rc = { 0, 0, 860, 520 };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);

        m_hwnd = CreateWindowExW(0, CLASS_NAME, WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, hInst, this);

        if (!m_hwnd) return false;

        CenterWindow();
        ShowWindow(m_hwnd, nCmdShow);
        UpdateWindow(m_hwnd);
        return true;
    }

    // Public accessor for history export (used by visualization tools)
    std::string ExportHistoryToJson(const std::string& from = "", const std::string& to = "") {
        return m_history.GetExportData(from, to).ToJson();
    }

private:
    // ---- Controls ----
    HWND m_hwnd;
    HINSTANCE m_hInst;
    HWND m_playlistLV;
    HWND m_searchEdit;
    HWND m_btnPrev, m_btnPlay, m_btnNext, m_btnMode;
    HWND m_trackSeek, m_sliderVol, m_staticVolPct;
    HWND m_staticTime, m_staticSong;
    HWND m_ctrlPanel;

    // ---- State ----
    AudioEngine      m_audio;
    PlaylistManager  m_playlist;
    int              m_currentIndex;
    bool             m_userDraggingSeek;
    int              m_sortColumn;
    bool             m_sortAscending;
    std::vector<int> m_filterMap;  // display row → playlist index

    // ---- Shuffle ----
    std::vector<int> m_shuffleOrder;
    int              m_shufflePos;

    // ---- Settings ----
    bool m_settingsAutoplay;
    bool m_settingsRememberProgress;
    bool m_settingsTray;
    bool m_trayIconAdded;

    // ---- Hotkeys ----
    HotkeyBinding m_hotkeys[7];
    int m_hotkeyCount;

    // ---- Menu handles (for nested submenus) ----
    HMENU m_settingsMenu;
    HMENU m_playSubMenu;

    // ---- Listening history ----
    ListeningHistory m_history;
    bool m_listening;
    std::chrono::steady_clock::time_point m_listenStart;
    int m_saveTick;

    HFONT m_hFont;

    friend LRESULT CALLBACK StatsDlgProc(HWND, UINT, WPARAM, LPARAM);

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MainWindow* win = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            win = (MainWindow*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);
        }
        if (win) {
            if (!win->m_hwnd) win->m_hwnd = hwnd;
            return win->HandleMessage(msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_KEYDOWN) {
            if (HandleAccelerator((int)wp)) return 0;
        }
        switch (msg) {
            case WM_CREATE:            OnCreate();                  return 0;
            case WM_DESTROY:           PostQuitMessage(0);          return 0;
            case WM_CLOSE:             OnCloseRequest();            return 0;
            case WM_SIZE:              OnSize(LOWORD(lp), HIWORD(lp)); return 0;
            case WM_GETMINMAXINFO:     OnMinMaxInfo((MINMAXINFO*)lp);  return 0;
            case WM_COMMAND:           OnCommand(wp, lp);            return 0;
            case WM_HSCROLL:           OnHScroll(wp, lp);            return 0;
            case WM_TIMER:             if (wp == TIMER_ID_SEEK) OnTimer(); return 0;
            case WM_HOTKEY:            OnGlobalHotkey((int)wp);     return 0;
            case WM_NOTIFY:            return OnNotify(wp, lp);
            case WM_CTLCOLORSTATIC: {
                HWND hCtrl = (HWND)lp;
                if (hCtrl == m_ctrlPanel || hCtrl == m_staticVolPct ||
                    hCtrl == m_staticTime || hCtrl == m_staticSong) {
                    HDC hdc = (HDC)wp;
                    SetDCBrushColor(hdc, RGB(240, 240, 240));
                    SetBkColor(hdc, RGB(240, 240, 240));
                    return (LRESULT)GetStockObject(DC_BRUSH);
                }
                return DefWindowProcW(m_hwnd, msg, wp, lp);
            }
            default:
                if (msg == WM_USER_SONG_END) { OnSongEnd(); return 0; }
                if (msg == WM_APP_TRAY) { HandleTrayMessage(wp, lp); return 0; }
                return DefWindowProcW(m_hwnd, msg, wp, lp);
        }
    }

    void InitDefaultHotkeys() {
        m_hotkeyCount = 7;
        m_hotkeys[0] = { HKID_PLAYPAUSE, L"播放/暂停",     'P',   MOD_CONTROL };
        m_hotkeys[1] = { HKID_PREV,      L"上一首",         VK_LEFT,  MOD_CONTROL };
        m_hotkeys[2] = { HKID_NEXT,      L"下一首",         VK_RIGHT, MOD_CONTROL };
        m_hotkeys[3] = { HKID_VOLUP,     L"音量增加",       VK_UP,    MOD_CONTROL };
        m_hotkeys[4] = { HKID_VOLDN,     L"音量减小",       VK_DOWN,  MOD_CONTROL };
        m_hotkeys[5] = { HKID_RESTORE,   L"从托盘恢复",     'J',     MOD_CONTROL };
        m_hotkeys[6] = { HKID_MINIMIZE,  L"最小化到托盘",   'K',     MOD_CONTROL };
    }

    void OnCreate() {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&icc);
        CreateMenuBar();
        CreateControls();

        RegisterStatsWindowClass();

        if (!m_audio.Initialize(m_hwnd)) {
            MessageBoxW(m_hwnd,
                L"无法初始化音频引擎 (bass.dll)。\n\n"
                L"请确保 bass.dll 位于程序目录或系统路径中。\n"
                L"下载地址: https://www.un4seen.com/bass.html",
                L"音频初始化失败", MB_OK | MB_ICONWARNING);
        }

        LoadSettings();
        UpdateSettingsMenu();
        UpdateModeUI();

        if (!LoadPlaylist()) {
            LoadFromLastFolder();
        }

        LoadVolume();
        m_history.Load(GetExeDirectory() + L"\\.history.txt");
        m_audio.SetNotifyWindow(m_hwnd, WM_USER_SONG_END);

        if (m_settingsRememberProgress && !m_playlist.IsEmpty()) {
            if (LoadLastSong()) {
            } else if (m_settingsAutoplay && !m_playlist.IsEmpty()) {
                PlayFile(0);
            }
        } else if (m_settingsAutoplay && !m_playlist.IsEmpty()) {
            PlayFile(0);
        }

        RegisterHotKeys();
        UpdateUI();
    }

    void OnCloseRequest() {
        if (m_settingsTray) {
            MinimizeToTray();
        } else {
            OnRealClose();
        }
    }

    void OnRealClose() {
        StopListening();
        m_history.Save(GetExeDirectory() + L"\\.history.txt");
        if (m_settingsRememberProgress) SaveLastSong();
        SavePlaylist();
        SaveVolume();
        SaveSettings();
        UnregisterHotKeys();
        RemoveTrayIcon();
        m_audio.Cleanup();
        DestroyWindow(m_hwnd);
    }

    // Menu bar
    void CreateMenuBar() {
        HMENU bar = CreateMenu();

        HMENU fileMenu = CreatePopupMenu();
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_OPENFOLDER, L"打开文件夹(&O)...");
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_ADDFILES, L"添加歌曲(&A)...");
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_EXPORT_PLAYLIST, L"导出歌单(&E)...");
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_EXIT, L"退出(&X)");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)fileMenu, L"文件(&F)");

        m_settingsMenu = CreatePopupMenu();
        AppendMenuW(m_settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_AUTOPLAY,
            L"启动后自动播放");
        AppendMenuW(m_settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_REMEMBER,
            L"记住播放进度");
        AppendMenuW(m_settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_TRAY,
            L"最小化到托盘");
        AppendMenuW(m_settingsMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_HOTKEYS,
            L"配置快捷键...");
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_STATS, L"统计");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)m_settingsMenu, L"设置(&S)");

        m_playSubMenu = CreatePopupMenu();
        AppendMenuW(m_playSubMenu, MF_STRING | MF_CHECKED, ID_PLAY_SEQUENTIAL, L"顺序播放(&S)");
        AppendMenuW(m_playSubMenu, MF_STRING, ID_PLAY_REPEATONE, L"单曲循环(&R)");
        AppendMenuW(m_playSubMenu, MF_STRING, ID_PLAY_SHUFFLE, L"随机播放(&H)");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)m_playSubMenu, L"播放(&P)");

        SetMenu(m_hwnd, bar);
    }

    void CreateControls() {
        NONCLIENTMETRICSW ncm = { sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        m_hFont = CreateFontIndirectW(&ncm.lfMenuFont);

        m_playlistLV = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER |
            LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_PLAYLIST, m_hInst, NULL);

        ListView_SetExtendedListViewStyle(m_playlistLV,
            LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP);

        HIMAGELIST himl = ImageList_Create(1, LV_ROW_HEIGHT, ILC_COLOR32, 1, 1);
        ListView_SetImageList(m_playlistLV, himl, LVSIL_SMALL);

        LVCOLUMNW lc = {};
        lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lc.fmt  = LVCFMT_LEFT;
        for (int i = 0; i < 4; i++) {
            lc.cx = COL_WIDTHS[i];
            lc.pszText = (LPWSTR)COL_LABELS[i];
            ListView_InsertColumn(m_playlistLV, i, &lc);
        }

        // ---- Search ----
        CreateWindowExW(0, L"STATIC", L"搜索:",
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, m_hwnd, NULL, m_hInst, NULL);
        m_searchEdit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_SEARCH_EDIT, m_hInst, NULL);

        // Background panel for bottom controls
        m_ctrlPanel = CreateWindowExW(0, L"STATIC", NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_CTRL_PANEL, m_hInst, NULL);

        m_btnMode = CreateWindowExW(0, L"BUTTON", L"顺序播放",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_MODE, m_hInst, NULL);
        m_btnPrev = CreateWindowExW(0, L"BUTTON", L"⏮",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_PREV, m_hInst, NULL);
        m_btnPlay = CreateWindowExW(0, L"BUTTON", L"▶",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_PLAY, m_hInst, NULL);
        m_btnNext = CreateWindowExW(0, L"BUTTON", L"⏭",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_NEXT, m_hInst, NULL);

        m_staticVolPct = CreateWindowExW(0, L"STATIC", L"80%",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_STAT_VOL, m_hInst, NULL);

        m_sliderVol = CreateWindowExW(0, TRACKBAR_CLASSW, NULL,
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_FIXEDLENGTH | TBS_NOTICKS,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_SLIDER_VOL, m_hInst, NULL);
        SendMessageW(m_sliderVol, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(m_sliderVol, TBM_SETPOS, TRUE, 80);
        SendMessageW(m_sliderVol, TBM_SETPAGESIZE, 0, 10);

        m_trackSeek = CreateWindowExW(0, TRACKBAR_CLASSW, NULL,
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_FIXEDLENGTH | TBS_NOTICKS,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_TRACK_SEEK, m_hInst, NULL);
        SendMessageW(m_trackSeek, TBM_SETRANGE, TRUE, MAKELPARAM(0, SEEK_RES));
        SendMessageW(m_trackSeek, TBM_SETPOS, TRUE, 0);
        SendMessageW(m_trackSeek, TBM_SETPAGESIZE, 0, SEEK_RES / 20);

        m_staticTime = CreateWindowExW(0, L"STATIC", L"00:00 / 00:00",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
            m_hwnd, (HMENU)IDC_STAT_TIME, m_hInst, NULL);
        m_staticSong = CreateWindowExW(0, L"STATIC", L"就绪",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_STAT_SONG, m_hInst, NULL);

        HWND ctls[] = { m_btnMode, m_btnPrev, m_btnPlay, m_btnNext,
                        m_sliderVol, m_trackSeek, m_staticVolPct,
                        m_staticTime, m_staticSong };
        for (auto c : ctls) SendMessageW(c, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        SendMessageW(m_playlistLV, WM_SETFONT, (WPARAM)m_hFont, TRUE);

        LayoutControls();
    }

    void LayoutControls() {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        const int M = 8;
        const int searchH = 22;
        const int ctrlPanelH = 96;  // bottom panel height: controls + padding
        const int lvY = M + searchH + M;
        int panelY = h - ctrlPanelH;
        int listH = panelY - lvY - M;  // M gap between listview and panel
        if (listH < 30) listH = 30;

        // Search label + edit at top
        HWND hSearchLabel = FindWindowExW(m_hwnd, NULL, L"STATIC", L"搜索:");
        if (hSearchLabel) {
            SetWindowPos(hSearchLabel, NULL, M, M, 40, searchH, SWP_NOZORDER);
            SendMessageW(hSearchLabel, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        }
        if (m_searchEdit) {
            SetWindowPos(m_searchEdit, NULL, M + 42, M, w - 2 * M - 42, searchH, SWP_NOZORDER);
            SendMessageW(m_searchEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        }

        // Listview
        SetWindowPos(m_playlistLV, NULL, M, lvY, w - 2 * M, listH, SWP_NOZORDER);

        // Control panel at bottom (created before buttons, so naturally behind them)
        SetWindowPos(m_ctrlPanel, NULL, 0, panelY, w, ctrlPanelH, SWP_NOZORDER);

        // Controls within panel area
        int y = panelY;
        const int BH = 28;

        SetWindowPos(m_btnMode, NULL, M, y + 4, 90, BH, SWP_NOZORDER);
        int bx = M + 96;
        SetWindowPos(m_btnPrev, NULL, bx, y + 4, 36, BH, SWP_NOZORDER);
        bx += 42;
        SetWindowPos(m_btnPlay, NULL, bx, y + 4, 36, BH, SWP_NOZORDER);
        bx += 42;
        SetWindowPos(m_btnNext, NULL, bx, y + 4, 36, BH, SWP_NOZORDER);

        int volPctW = 36;
        int volW = 130;
        int volX = w - M - volPctW - volW;
        SetWindowPos(m_staticVolPct, NULL, volX, y + 6, volPctW, 20, SWP_NOZORDER);
        SetWindowPos(m_sliderVol, NULL, volX + volPctW, y + 4, volW, BH, SWP_NOZORDER);

        y += BH + 6;
        SetWindowPos(m_trackSeek, NULL, M, y + 2, w - 2 * M, 24, SWP_NOZORDER);

        y += 28;
        int timeW = 170;
        SetWindowPos(m_staticTime, NULL, M, y + 2, timeW, 20, SWP_NOZORDER);
        SetWindowPos(m_staticSong, NULL, M + timeW + 8, y + 2,
                     w - M - timeW - 16, 20, SWP_NOZORDER);

        // Force immediate redraw of the control panel area
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void CenterWindow() {
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(m_hwnd, NULL,
            (sw - (rc.right - rc.left)) / 2,
            (sh - (rc.bottom - rc.top)) / 2,
            0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    void OnSize(int, int) { LayoutControls(); }
    void OnMinMaxInfo(MINMAXINFO* mmi) {
        mmi->ptMinTrackSize.x = MIN_W;
        mmi->ptMinTrackSize.y = MIN_H;
    }

    // WM_COMMAND
    void OnCommand(WPARAM wp, LPARAM lp) {
        WORD id = LOWORD(wp);
        HWND hCtrl = (HWND)lp;

        if (hCtrl == NULL) {
            switch (id) {
                case ID_FILE_OPENFOLDER:      OpenFolder(); break;
                case ID_FILE_ADDFILES:        AddFiles();   break;
                case ID_FILE_EXPORT_PLAYLIST: ExportPlaylist(); break;
                case ID_FILE_EXIT:            OnRealClose(); break;
                case ID_SETTINGS_AUTOPLAY:
                    m_settingsAutoplay = !m_settingsAutoplay;
                    UpdateSettingsMenu();
                    break;
                case ID_SETTINGS_REMEMBER:
                    m_settingsRememberProgress = !m_settingsRememberProgress;
                    UpdateSettingsMenu();
                    break;
                case ID_SETTINGS_TRAY:
                    m_settingsTray = !m_settingsTray;
                    UpdateSettingsMenu();
                    if (!m_settingsTray) RemoveTrayIcon();
                    break;
                case ID_SETTINGS_HOTKEYS:
                    ShowHotkeyDialog();
                    break;
                case ID_SETTINGS_STATS:
                    ShowStatsWindow();
                    break;
                case ID_PLAY_SEQUENTIAL: SetPlayMode(PlayMode::Sequential); break;
                case ID_PLAY_REPEATONE:  SetPlayMode(PlayMode::RepeatOne);  break;
                case ID_PLAY_SHUFFLE:
                    SetPlayMode(PlayMode::Shuffle);
                    Reshuffle();
                    break;
                case ID_TRAY_RESTORE:
                    ShowWindow(m_hwnd, SW_RESTORE);
                    SetForegroundWindow(m_hwnd);
                    RemoveTrayIcon();
                    break;
                case ID_TRAY_EXIT:
                    m_settingsTray = false;
                    OnRealClose();
                    break;
            }
        } else {
            WORD code = HIWORD(wp);
            if (code == BN_CLICKED) {
                if      (hCtrl == m_btnPlay) OnPlayPause();
                else if (hCtrl == m_btnPrev) OnPrev();
                else if (hCtrl == m_btnNext) OnNext();
                else if (hCtrl == m_btnMode) OnCycleMode();
            } else if (code == EN_CHANGE && hCtrl == m_searchEdit) {
                OnSearchChanged();
            }
        }
    }

    // WM_HSCROLL
    void OnHScroll(WPARAM wp, LPARAM lp) {
        HWND hCtrl = (HWND)lp;
        WORD code = LOWORD(wp);

        if (hCtrl == m_trackSeek) {
            int pos = (int)SendMessageW(m_trackSeek, TBM_GETPOS, 0, 0);
            double len = m_audio.GetLength();
            if (code == TB_THUMBTRACK) {
                m_userDraggingSeek = true;
                if (len > 0) {
                    double p = len * pos / SEEK_RES;
                    SetWindowTextW(m_staticTime,
                        (FormatTime(p) + L" / " + FormatTime(len)).c_str());
                }
            } else {
                if (len > 0) {
                    m_audio.SetPosition(len * pos / SEEK_RES);
                    UpdateTimeDisplay();
                }
                if (code == TB_ENDTRACK) {
                    m_userDraggingSeek = false;
                    if (m_settingsRememberProgress) SaveLastSong();
                }
            }
        } else if (hCtrl == m_sliderVol) {
            int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0);
            m_audio.SetVolume(vol);
            UpdateVolLabel();
        }
    }

    // WM_NOTIFY
    LRESULT OnNotify(WPARAM, LPARAM lp) {
        LPNMHDR nmh = (LPNMHDR)lp;
        if (nmh->hwndFrom == m_playlistLV) {
            switch (nmh->code) {
                case LVN_COLUMNCLICK: {
                    LPNMLISTVIEW lv = (LPNMLISTVIEW)lp;
                    OnLVColumnClick(lv->iSubItem);
                    return 0;
                }
                case NM_DBLCLK: {
                    LPNMITEMACTIVATE ia = (LPNMITEMACTIVATE)lp;
                    if (ia->iItem >= 0 && ia->iItem < (int)m_filterMap.size()) {
                        PlayFile(m_filterMap[ia->iItem]);
                    }
                    return 0;
                }
            }
        }
        return 0;
    }

    // Sort
    void UpdateColumnHeaders(int activeColumn) {
        for (int i = 0; i < 4; i++) {
            std::wstring label = COL_LABELS[i];
            if (i == activeColumn)
                label += m_sortAscending ? L" ▲" : L" ▼";
            LVCOLUMNW lc = {};
            lc.mask = LVCF_TEXT;
            lc.pszText = &label[0];
            ListView_SetColumn(m_playlistLV, i, &lc);
        }
    }

    void OnLVColumnClick(int column) {
        if (column == m_sortColumn)
            m_sortAscending = !m_sortAscending;
        else {
            m_sortColumn = column;
            m_sortAscending = (column != 3);
        }

        std::wstring curPath;
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount())
            curPath = m_playlist.GetFile(m_currentIndex);

        m_playlist.Sort(column, m_sortAscending);
        UpdateColumnHeaders(column);

        RefreshPlaylistUI();

        if (!curPath.empty()) {
            m_currentIndex = -1;
            for (int i = 0; i < m_playlist.GetCount(); i++) {
                if (m_playlist.GetFile(i) == curPath) {
                    m_currentIndex = i;
                    break;
                }
            }
            UpdatePlaylistSelection();
        }
    }

    // Timer
    void OnTimer() {
        if (m_audio.IsLoaded() && !m_userDraggingSeek) {
            UpdateSeekDisplay();
            UpdateTimeDisplay();
            if (m_settingsRememberProgress) {
                if (++m_saveTick >= 20) {
                    m_saveTick = 0;
                    SaveLastSong();
                }
            }
        }
    }

    // Fisher-Yates shuffle
    void Reshuffle() {
        int count = m_playlist.GetCount();
        if (count == 0) return;
        m_shuffleOrder.resize(count);
        for (int i = 0; i < count; i++) m_shuffleOrder[i] = i;
        for (int i = count - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            std::swap(m_shuffleOrder[i], m_shuffleOrder[j]);
        }
        m_shufflePos = 0;
    }

    // Song end
    void OnSongEnd() {
        KillTimer(m_hwnd, TIMER_ID_SEEK);
        m_audio.NotifyEndOfSong();

        int count = m_playlist.GetCount();
        if (count == 0) return;

        int next = -1;
        switch (m_audio.GetPlayMode()) {
            case PlayMode::RepeatOne:
                next = m_currentIndex;
                break;
            case PlayMode::Shuffle:
                m_shufflePos++;
                if (m_shufflePos >= (int)m_shuffleOrder.size())
                    Reshuffle();
                if (!m_shuffleOrder.empty())
                    next = m_shuffleOrder[m_shufflePos];
                break;
            case PlayMode::Sequential:
            default:
                if (m_currentIndex + 1 < count)
                    next = m_currentIndex + 1;
                break;
        }

        if (next >= 0 && next < count) {
            PlayFile(next);
        } else {
            m_currentIndex = -1;
            SetWindowTextW(m_staticSong, L"播放完毕");
            SetWindowTextW(m_staticTime, L"00:00 / 00:00");
            SendMessageW(m_trackSeek, TBM_SETPOS, TRUE, 0);
            EnableWindow(m_trackSeek, FALSE);
            UpdateUI();
        }
    }

    // Open folder
    void OpenFolder() {
        BROWSEINFOW bi = {};
        bi.hwndOwner = m_hwnd;
        bi.lpszTitle = L"选择音乐文件夹";
        bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (!pidl) return;

        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            m_audio.Unload();
            KillTimer(m_hwnd, TIMER_ID_SEEK);
            m_currentIndex = -1;
            m_userDraggingSeek = false;

            SaveLastFolder(path);
            m_playlist.ScanFolder(path);
            m_sortColumn = 1;
            m_sortAscending = true;
            UpdateColumnHeaders(1);
            RefreshPlaylistUI();

            if (m_shuffleOrder.size() != (size_t)m_playlist.GetCount())
                Reshuffle();

            if (!m_playlist.IsEmpty()) {
                PlayFile(0);
            } else {
                SetWindowTextW(m_staticSong, L"所选文件夹中没有找到音频文件 (MP3/FLAC/WAV)");
                UpdateUI();
            }
        }
        CoTaskMemFree(pidl);
    }

    // Add files
    void AddFiles() {
        wchar_t buf[65536] = {};

        OPENFILENAMEW ofn = {};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = m_hwnd;
        ofn.lpstrFile       = buf;
        ofn.nMaxFile        = 65536;
        ofn.lpstrFilter     = L"音频文件 (*.mp3;*.flac;*.wav)\0*.mp3;*.flac;*.wav\0所有文件 (*.*)\0*.*\0";
        ofn.nFilterIndex    = 1;
        ofn.Flags           = OFN_ALLOWMULTISELECT | OFN_EXPLORER |
                              OFN_HIDEREADONLY | OFN_FILEMUSTEXIST |
                              OFN_LONGNAMES | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&ofn)) return;

        std::wstring dir = buf;
        size_t offset = dir.size() + 1;
        bool added = false;
        bool hadItems = !m_playlist.IsEmpty();

        if (buf[offset] == L'\0') {
            std::wstring ext;
            const wchar_t* dot = wcsrchr(buf, L'.');
            if (dot) ext = dot;
            if (PlaylistManager::IsAudioExtension(ext)) {
                m_playlist.AddFile(buf);
                added = true;
            }
        } else {
            while (buf[offset] != L'\0') {
                std::wstring fullPath = dir + L"\\" + (buf + offset);
                std::wstring ext;
                const wchar_t* dot = wcsrchr(buf + offset, L'.');
                if (dot) ext = dot;
                if (PlaylistManager::IsAudioExtension(ext)) {
                    bool dup = false;
                    for (int i = 0; i < m_playlist.GetCount(); i++) {
                        if (m_playlist.GetFile(i) == fullPath) { dup = true; break; }
                    }
                    if (!dup) { m_playlist.AddFile(fullPath); added = true; }
                }
                offset += wcslen(buf + offset) + 1;
            }
        }

        if (added) {
            // Sort by title after adding files
            std::wstring curPath;
            if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount())
                curPath = m_playlist.GetFile(m_currentIndex);
            m_playlist.Sort(1, true);
            m_sortColumn = 1;
            m_sortAscending = true;
            UpdateColumnHeaders(1);
            // Restore current index after sort
            if (!curPath.empty()) {
                m_currentIndex = -1;
                for (int i = 0; i < m_playlist.GetCount(); i++) {
                    if (m_playlist.GetFile(i) == curPath) {
                        m_currentIndex = i;
                        break;
                    }
                }
            }
            RefreshPlaylistUI();
            UpdatePlaylistSelection();
            if (m_audio.GetPlayMode() == PlayMode::Shuffle) Reshuffle();
            if (!hadItems) PlayFile(0);
            else {
                SetWindowTextW(m_staticSong,
                    (L"已添加 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
                UpdateUI();
            }
        }
    }

    // Export playlist to JSON file
    void ExportPlaylist() {
        if (m_playlist.IsEmpty()) {
            MessageBoxW(m_hwnd, L"播放列表为空，没有可导出的内容。", L"导出歌单", MB_OK | MB_ICONINFORMATION);
            return;
        }

        wchar_t filePath[MAX_PATH] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = m_hwnd;
        ofn.lpstrFile    = filePath;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = L"JSON 文件 (*.json)\0*.json\0所有文件 (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt  = L"json";
        ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameW(&ofn)) return;

        // Build JSON
        std::string json = "{\n";
        json += "  \"playlist\": [\n";
        for (int i = 0; i < m_playlist.GetCount(); i++) {
            const auto& song = m_playlist.GetSong(i);
            char fileBuf[1024], titleBuf[512], artistBuf[256], albumBuf[256];
            snprintf(fileBuf,  sizeof(fileBuf),  "%ls", song.filePath.c_str());
            snprintf(titleBuf, sizeof(titleBuf), "%ls", song.title.c_str());
            snprintf(artistBuf, sizeof(artistBuf), "%ls", song.artist.c_str());
            snprintf(albumBuf, sizeof(albumBuf),  "%ls", song.album.c_str());

            json += "    {\n";
            json += "      \"filePath\": \"" + EscapeJson(fileBuf) + "\",\n";
            json += "      \"title\": \""    + EscapeJson(titleBuf) + "\",\n";
            json += "      \"artist\": \""   + EscapeJson(artistBuf) + "\",\n";
            json += "      \"album\": \""    + EscapeJson(albumBuf) + "\",\n";
            json += "      \"duration\": "   + std::to_string(song.duration) + "\n";
            json += "    }";
            if (i < m_playlist.GetCount() - 1) json += ",";
            json += "\n";
        }
        json += "  ]\n";
        json += "}\n";

        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            MessageBoxW(m_hwnd, L"无法写入文件。", L"导出失败", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD written;
        // Write UTF-8 BOM for compatibility
        const BYTE bomUtf8[] = { 0xEF, 0xBB, 0xBF };
        WriteFile(hFile, bomUtf8, 3, &written, NULL);
        WriteFile(hFile, json.c_str(), (DWORD)json.size(), &written, NULL);
        CloseHandle(hFile);

        std::wstring msg = L"成功导出 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲。";
        MessageBoxW(m_hwnd, msg.c_str(), L"导出完成", MB_OK | MB_ICONINFORMATION);
    }

    static std::string EscapeJson(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '\"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if ((unsigned char)c < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    }

    // Playback controls
    void OnPlayPause() {
        if (!m_audio.IsLoaded()) return;
        if (m_audio.IsPlaying()) {
            m_audio.Pause();
            StopListening();
            SetWindowTextW(m_staticSong, L"已暂停");
            UpdateTrayTip();
        } else {
            m_audio.Play();
            StartListening();
            if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
                std::wstring meta = m_audio.GetFormattedMetadata();
                const auto& path = m_playlist.GetFile(m_currentIndex);
                if (!meta.empty()) {
                    SetWindowTextW(m_staticSong, (L"正在播放: " + meta).c_str());
                    UpdateTrayTip();
                } else {
                    SetWindowTextW(m_staticSong, (L"正在播放: " + GetDisplayName(path)).c_str());
                    UpdateTrayTip();
                }
            }
            SetTimer(m_hwnd, TIMER_ID_SEEK, 500, NULL);
        }
        UpdatePlayButton();
    }

    void OnPrev() {
        int count = m_playlist.GetCount();
        if (count == 0) return;
        int idx = (m_currentIndex <= 0) ? count - 1 : m_currentIndex - 1;
        PlayFile(idx);
    }

    void OnNext() {
        int count = m_playlist.GetCount();
        if (count == 0) return;
        int idx;
        if (m_audio.GetPlayMode() == PlayMode::Shuffle) {
            m_shufflePos++;
            if (m_shufflePos >= (int)m_shuffleOrder.size()) Reshuffle();
            idx = m_shuffleOrder[m_shufflePos];
        } else {
            idx = (m_currentIndex + 1) % count;
        }
        PlayFile(idx);
    }

    void OnCycleMode() {
        PlayMode old = m_audio.GetPlayMode();
        m_audio.CyclePlayMode();
        if (m_audio.GetPlayMode() == PlayMode::Shuffle && old != PlayMode::Shuffle)
            Reshuffle();
        UpdateModeUI();
    }

    void SetPlayMode(PlayMode mode) {
        m_audio.SetPlayMode(mode);
        if (mode == PlayMode::Shuffle) Reshuffle();
        UpdateModeUI();
    }

    void UpdateModeUI() {
        PlayMode pm = m_audio.GetPlayMode();
        const wchar_t* labels[] = { L"顺序播放", L"单曲循环", L"随机播放" };
        SetWindowTextW(m_btnMode, labels[(int)pm]);

        CheckMenuItem(m_playSubMenu, ID_PLAY_SEQUENTIAL,
            MF_BYCOMMAND | (pm == PlayMode::Sequential ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_playSubMenu, ID_PLAY_REPEATONE,
            MF_BYCOMMAND | (pm == PlayMode::RepeatOne ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_playSubMenu, ID_PLAY_SHUFFLE,
            MF_BYCOMMAND | (pm == PlayMode::Shuffle ? MF_CHECKED : MF_UNCHECKED));
    }

    void UpdateSettingsMenu() {
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_AUTOPLAY,
            MF_BYCOMMAND | (m_settingsAutoplay ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_REMEMBER,
            MF_BYCOMMAND | (m_settingsRememberProgress ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_TRAY,
            MF_BYCOMMAND | (m_settingsTray ? MF_CHECKED : MF_UNCHECKED));
    }

    
    // Search / Filter
    
    bool SearchMatches(int playlistIdx) {
        const auto& song = m_playlist.GetSong(playlistIdx);
        if (song.title.empty() && song.artist.empty() && song.album.empty())
            return true;
        // Get search text
        if (!m_searchEdit) return true;
        wchar_t searchBuf[256] = {};
        GetWindowTextW(m_searchEdit, searchBuf, 256);
        if (searchBuf[0] == L'\0') return true;

        // Case-insensitive comparison
        std::wstring q = searchBuf;
        for (auto& c : q) c = towlower(c);

        auto contains = [&](const std::wstring& s) -> bool {
            std::wstring ls = s;
            for (auto& c : ls) c = towlower(c);
            return ls.find(q) != std::wstring::npos;
        };

        return contains(song.title) || contains(song.artist) || contains(song.album);
    }

    void RebuildFilter() {
        m_filterMap.clear();
        m_filterMap.reserve(m_playlist.GetCount());
        for (int i = 0; i < m_playlist.GetCount(); i++) {
            m_filterMap.push_back(i);
        }
        if (m_searchEdit) {
            wchar_t searchBuf[256] = {};
            GetWindowTextW(m_searchEdit, searchBuf, 256);
            if (searchBuf[0] != L'\0') {
                std::vector<int> filtered;
                for (int idx : m_filterMap) {
                    if (SearchMatches(idx)) filtered.push_back(idx);
                }
                m_filterMap.swap(filtered);
            }
        }
    }

    void OnSearchChanged() {
        RebuildFilter();
        RefreshPlaylistUI();
        // Try to keep current song selected if visible
        if (m_currentIndex >= 0) {
            UpdatePlaylistSelection();
        }
    }

    
    // Keyboard shortcuts
    
    bool HandleAccelerator(int vk) {
        int heldMod = 0;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) heldMod |= MOD_CONTROL;
        if (GetAsyncKeyState(VK_MENU) & 0x8000)    heldMod |= MOD_ALT;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   heldMod |= 0x0004; // MOD_SHIFT

        for (int i = 0; i < m_hotkeyCount; i++) {
            if (vk == m_hotkeys[i].vk && heldMod == m_hotkeys[i].mod) {
                // Don't trigger for plain key presses (no modifier) — let text through
                if (heldMod == 0) continue;
                ExecuteHotkey(i);
                return true;
            }
        }
        return false;
    }

    void OnGlobalHotkey(int id) {
        for (int i = 0; i < m_hotkeyCount; i++) {
            if (m_hotkeys[i].id == id || m_hotkeys[i].id + 1000 == id) {
                ExecuteHotkey(i); return;
            }
        }
    }

    void ExecuteHotkey(int idx) {
        switch (m_hotkeys[idx].id) {
            case HKID_PLAYPAUSE: OnPlayPause();  break;
            case HKID_PREV:      OnPrev();        break;
            case HKID_NEXT:      OnNext();        break;
            case HKID_VOLUP:     AdjustVolume(5);   break;
            case HKID_VOLDN:     AdjustVolume(-5);  break;
            case HKID_RESTORE:   ShowWindow(m_hwnd, SW_RESTORE); SetForegroundWindow(m_hwnd); RemoveTrayIcon(); break;
            case HKID_MINIMIZE:  MinimizeToTray(); break;
        }
    }

    void RegisterHotKeys() {
        HWND h = m_hwnd;
        for (int i = 0; i < m_hotkeyCount; i++) {
            // In-app: user-configured modifiers (default: Ctrl)
            RegisterHotKey(h, m_hotkeys[i].id, m_hotkeys[i].mod, m_hotkeys[i].vk);
            // Global: auto-add Alt (default: Ctrl+Alt)
            int globalMod = m_hotkeys[i].mod | MOD_ALT;
            RegisterHotKey(h, m_hotkeys[i].id + 1000, globalMod, m_hotkeys[i].vk);
        }
    }

    void UnregisterHotKeys() {
        HWND h = m_hwnd;
        for (int i = 0; i < m_hotkeyCount; i++) {
            UnregisterHotKey(h, m_hotkeys[i].id);
            UnregisterHotKey(h, m_hotkeys[i].id + 1000);
        }
    }

    void AdjustVolume(int delta) {
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0) + delta;
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        m_audio.SetVolume(vol);
        SendMessageW(m_sliderVol, TBM_SETPOS, TRUE, vol);
        UpdateVolLabel();
    }

    
    // Hotkey config dialog
    
    void ShowHotkeyDialog() {
        HotkeyBinding tmp[7];
        memcpy(tmp, m_hotkeys, sizeof(m_hotkeys));

        const wchar_t DLG_CLASS[] = L"HotkeyConfigDlg";
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = HotkeyDlgProc;
        wc.hInstance     = m_hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = DLG_CLASS;
        if (!RegisterClassExW(&wc)) return;

        int dlgW = 460, dlgH = 350;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

        HWND hDlg = CreateWindowExW(0, DLG_CLASS, L"配置快捷键",
            WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            x, y, dlgW, dlgH, m_hwnd, NULL, m_hInst, NULL);
        if (!hDlg) return;

        // Store context in dialog's GWLP_USERDATA
        HKDlgCtx* ctx = new HKDlgCtx{ tmp, -1, m_hotkeyCount, this, 0 };
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

        HFONT hGuiFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);

        int rowY = 20;
        for (int i = 0; i < m_hotkeyCount; i++) {
            wchar_t label[64];
            swprintf(label, 64, L"%ls:", m_hotkeys[i].actionName);
            CreateWindowExW(0, L"STATIC", label,
                WS_CHILD | WS_VISIBLE, 20, rowY, 140, 24, hDlg, NULL, m_hInst, NULL);

            std::wstring keyText = HotkeyToString(tmp[i].vk, tmp[i].mod);
            HWND hKey = CreateWindowExW(0, L"STATIC", keyText.c_str(),
                WS_CHILD | WS_VISIBLE | SS_CENTER | SS_SUNKEN,
                170, rowY, 200, 24, hDlg, (HMENU)(size_t)(100 + i), m_hInst, NULL);
            SendMessageW(hKey, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

            CreateWindowExW(0, L"BUTTON", L"更改",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                380, rowY, 60, 24, hDlg, (HMENU)(size_t)(200 + i), m_hInst, NULL);
            rowY += 32;
        }

        CreateWindowExW(0, L"STATIC", L"提示: 点击\"更改\"后按下新的按键组合",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            20, rowY + 10, dlgW - 40, 20, hDlg, NULL, m_hInst, NULL);

        CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            dlgW / 2 - 110, rowY + 40, 90, 28, hDlg, (HMENU)(size_t)IDOK, m_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            dlgW / 2 + 20, rowY + 40, 90, 28, hDlg, (HMENU)(size_t)IDCANCEL, m_hInst, NULL);

        EnableWindow(m_hwnd, FALSE);
        MSG msg;
        while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
            // WM_KEYDOWN from the thread queue — capture for recording
            if (msg.message == WM_KEYDOWN) {
                HKDlgCtx* c = (HKDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
                if (c && c->recording >= 0 && c->recording < c->count) {
                    int mod = 0;
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
                    if (GetAsyncKeyState(VK_MENU) & 0x8000)    mod |= MOD_ALT;
                    int vk = (int)msg.wParam;
                    if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT ||
                        vk == VK_ESCAPE || vk == VK_RETURN) continue;
                    if (mod == 0) mod = MOD_CONTROL;
                    int idx = c->recording;
                    c->bindings[idx].vk = vk;
                    c->bindings[idx].mod = mod;
                    std::wstring ks = HotkeyToString(vk, mod);
                    SetWindowTextW(GetDlgItem(hDlg, 100 + idx), ks.c_str());
                    c->recording = -1;
                    continue; // Don't dispatch to focused control
                }
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        EnableWindow(m_hwnd, TRUE);
        SetForegroundWindow(m_hwnd);

        if (ctx->result == 1) {
            memcpy(m_hotkeys, ctx->bindings, sizeof(m_hotkeys));
            UnregisterHotKeys();
            RegisterHotKeys();
            SaveHotkeyBindings();
        }

        delete ctx;
        DeleteObject(hGuiFont);
        UnregisterClassW(DLG_CLASS, m_hInst);
    }


    // Stats window


    static void FormatListenTime(double seconds, wchar_t* buf, int bufLen) {
        int total = (int)seconds;
        int h = total / 3600;
        int m = (total % 3600) / 60;
        int s = total % 60;
        if (h > 0)
            swprintf(buf, bufLen, L"%d小时%d分%d秒", h, m, s);
        else if (m > 0)
            swprintf(buf, bufLen, L"%d分%d秒", m, s);
        else
            swprintf(buf, bufLen, L"%d秒", s);
    }

    void RegisterStatsWindowClass() {
        const wchar_t STATS_CLASS[] = L"StatsWindow";
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = StatsDlgProc;
        wc.hInstance     = m_hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = STATS_CLASS;
        RegisterClassExW(&wc);
    }

    void ShowStatsWindow() {
        const wchar_t STATS_CLASS[] = L"StatsWindow";
        int dlgW = 620, dlgH = 520;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

        HWND hDlg = CreateWindowExW(0, STATS_CLASS, L"听歌时长统计",
            WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            x, y, dlgW, dlgH, m_hwnd, NULL, m_hInst, NULL);
        if (!hDlg) return;

        StatsDlgCtx* ctx = new StatsDlgCtx();
        ctx->win = this;
        ctx->rangeDays = 30;
        ctx->hRadio7 = NULL;
        ctx->hRadio30 = NULL;
        ctx->hRadioCustom = NULL;
        ctx->hEditCustom = NULL;
        ctx->hDayList = NULL;
        ctx->hWeekList = NULL;
        ctx->hTotalText = NULL;
        ctx->hDlg = hDlg;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

        HFONT hGuiFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
        HFONT hBoldFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);

        int yPos = 15;

        CreateWindowExW(0, L"STATIC", L"统计范围",
            WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, m_hInst, NULL);

        yPos += 22;
        ctx->hRadio7 = CreateWindowExW(0, L"BUTTON", L"7天",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            30, yPos, 60, 22, hDlg, (HMENU)300, m_hInst, NULL);
        SendMessageW(ctx->hRadio7, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        ctx->hRadio30 = CreateWindowExW(0, L"BUTTON", L"30天",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            110, yPos, 60, 22, hDlg, (HMENU)301, m_hInst, NULL);
        SendMessageW(ctx->hRadio30, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        SendMessageW(ctx->hRadio30, BM_SETCHECK, BST_CHECKED, 0);

        ctx->hRadioCustom = CreateWindowExW(0, L"BUTTON", L"自定义",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            190, yPos, 65, 22, hDlg, (HMENU)302, m_hInst, NULL);
        SendMessageW(ctx->hRadioCustom, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        ctx->hEditCustom = CreateWindowExW(0, L"EDIT", L"2",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_NUMBER,
            265, yPos - 1, 40, 22, hDlg, (HMENU)303, m_hInst, NULL);
        SendMessageW(ctx->hEditCustom, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        SendMessageW(ctx->hEditCustom, EM_SETLIMITTEXT, 3, 0);

        CreateWindowExW(0, L"STATIC", L"×30天",
            WS_CHILD | WS_VISIBLE, 308, yPos + 3, 50, 20, hDlg, NULL, m_hInst, NULL);

        CreateWindowExW(0, L"BUTTON", L"刷新",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, yPos - 2, 70, 26, hDlg, (HMENU)304, m_hInst, NULL);

        yPos += 34;

        CreateWindowExW(0, L"STATIC", L"每日详情",
            WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, m_hInst, NULL);

        yPos += 20;
        ctx->hDayList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
            15, yPos, dlgW - 30, 190, hDlg, NULL, m_hInst, NULL);
        ListView_SetExtendedListViewStyle(ctx->hDayList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

        LVCOLUMNW lc = {};
        lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lc.fmt  = LVCFMT_LEFT;
        lc.cx = 120; lc.pszText = (LPWSTR)L"日期"; ListView_InsertColumn(ctx->hDayList, 0, &lc);
        lc.cx = 70;  lc.pszText = (LPWSTR)L"星期"; ListView_InsertColumn(ctx->hDayList, 1, &lc);
        lc.cx = 150; lc.pszText = (LPWSTR)L"听歌时长"; ListView_InsertColumn(ctx->hDayList, 2, &lc);

        yPos += 198;

        CreateWindowExW(0, L"STATIC", L"每周统计",
            WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, m_hInst, NULL);

        yPos += 20;
        ctx->hWeekList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
            15, yPos, dlgW - 30, 100, hDlg, NULL, m_hInst, NULL);
        ListView_SetExtendedListViewStyle(ctx->hWeekList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

        lc.cx = 100; lc.pszText = (LPWSTR)L"周"; ListView_InsertColumn(ctx->hWeekList, 0, &lc);
        lc.cx = 140; lc.pszText = (LPWSTR)L"日期范围"; ListView_InsertColumn(ctx->hWeekList, 1, &lc);
        lc.cx = 150; lc.pszText = (LPWSTR)L"累计时长"; ListView_InsertColumn(ctx->hWeekList, 2, &lc);

        yPos += 108;

        ctx->hTotalText = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            15, yPos, dlgW - 30, 22, hDlg, (HMENU)400, m_hInst, NULL);
        SendMessageW(ctx->hTotalText, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

        yPos += 28;
        CreateWindowExW(0, L"BUTTON", L"导出数据(JSON)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            dlgW / 2 - 120, yPos, 120, 28, hDlg, (HMENU)305, m_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"关闭",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            dlgW / 2 + 20, yPos, 90, 28, hDlg, (HMENU)IDCANCEL, m_hInst, NULL);

        RefreshStatsDisplay(ctx);

        DeleteObject(hBoldFont);
        DeleteObject(hGuiFont);
    }

    void RefreshStatsDisplay(StatsDlgCtx* ctx) {
        time_t now_t = time(NULL);
        struct tm tm_now = *localtime(&now_t);
        tm_now.tm_mday -= ctx->rangeDays;
        tm_now.tm_isdst = -1;
        mktime(&tm_now);
        char fromBuf[32];
        snprintf(fromBuf, sizeof(fromBuf), "%04d-%02d-%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
        std::string fromDate(fromBuf);

        struct tm tm_today = *localtime(&now_t);
        char toBuf[32];
        snprintf(toBuf, sizeof(toBuf), "%04d-%02d-%02d",
                 tm_today.tm_year + 1900, tm_today.tm_mon + 1, tm_today.tm_mday);
        std::string toDate(toBuf);

        auto daily = m_history.GetDailyRecords(fromDate, toDate);
        double total = m_history.GetTotalSeconds(fromDate, toDate);
        auto weekly = m_history.GetWeeklySummaries(fromDate, toDate);

        ListView_DeleteAllItems(ctx->hDayList);
        for (size_t i = 0; i < daily.size(); i++) {
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = (int)i;
            item.pszText = (LPWSTR)daily[i].date.c_str();
            ListView_InsertItem(ctx->hDayList, &item);
            ListView_SetItemText(ctx->hDayList, (int)i, 1, (LPWSTR)daily[i].weekday.c_str());
            wchar_t tbuf[64];
            FormatListenTime(daily[i].seconds, tbuf, 64);
            ListView_SetItemText(ctx->hDayList, (int)i, 2, tbuf);
        }

        ListView_DeleteAllItems(ctx->hWeekList);
        for (size_t i = 0; i < weekly.size(); i++) {
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = (int)i;
            item.pszText = (LPWSTR)weekly[i].label.c_str();
            ListView_InsertItem(ctx->hWeekList, &item);
            ListView_SetItemText(ctx->hWeekList, (int)i, 1, (LPWSTR)weekly[i].dateRange.c_str());
            wchar_t tbuf[64];
            FormatListenTime(weekly[i].seconds, tbuf, 64);
            ListView_SetItemText(ctx->hWeekList, (int)i, 2, tbuf);
        }

        wchar_t tbuf[128], label[256];
        FormatListenTime(total, tbuf, 128);
        swprintf(label, 256, L"总计: %ls", tbuf);
        SetWindowTextW(ctx->hTotalText, label);
    }


    // Listening time tracking


    void StartListening() {
        if (!m_listening) {
            m_listening = true;
            m_listenStart = std::chrono::steady_clock::now();
        }
    }

    void StopListening() {
        if (m_listening) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - m_listenStart).count();
            if (elapsed > 0.5) // ignore sub-second glitches
                m_history.AddSession(elapsed);
            m_listening = false;
        }
    }


    // Play file

    void PlayFile(int index) {
        if (index < 0 || index >= m_playlist.GetCount()) return;

        KillTimer(m_hwnd, TIMER_ID_SEEK);
        const std::wstring& path = m_playlist.GetFile(index);

        if (!m_audio.Load(path)) {
            SetWindowTextW(m_staticSong,
                (L"无法加载: " + GetDisplayName(path)).c_str());
            return;
        }

        m_currentIndex = index;
        m_audio.Play();
        StartListening();
        UpdateUI();
        UpdatePlaylistSelection();

        double len = m_audio.GetLength();
        std::wstring meta = m_audio.GetFormattedMetadata();

        std::wstring artist, title;
        size_t dash = meta.find(L" - ");
        if (dash != std::wstring::npos) {
            artist = meta.substr(0, dash);
            title  = meta.substr(dash + 3);
        }

        m_playlist.UpdateMetadata(index, artist, title, L"", len);
        // Find display index for this playlist index
        for (int di = 0; di < (int)m_filterMap.size(); di++) {
            if (m_filterMap[di] == index) { UpdateLVItem(di); break; }
        }

        if (!meta.empty())
            SetWindowTextW(m_staticSong, (L"正在播放: " + meta).c_str());
        else
            SetWindowTextW(m_staticSong, (L"正在播放: " + GetDisplayName(path)).c_str());

        SetTimer(m_hwnd, TIMER_ID_SEEK, 500, NULL);
        m_saveTick = 0;

        UpdateTrayTip();
    }

    
    // UI helpers
    
    void UpdateUI() {
        bool loaded = m_audio.IsLoaded();
        bool hasItems = !m_playlist.IsEmpty();
        EnableWindow(m_btnPlay,  loaded);
        EnableWindow(m_btnPrev,  hasItems);
        EnableWindow(m_btnNext,  hasItems);
        EnableWindow(m_trackSeek, loaded);
        EnableWindow(m_sliderVol, loaded);
        UpdatePlayButton();
        UpdateModeUI();
        UpdateVolLabel();
        if (!loaded) {
            UpdateSeekDisplay();
            UpdateTimeDisplay();
        }
    }

    void UpdatePlayButton() {
        SetWindowTextW(m_btnPlay, m_audio.IsPlaying() ? L"⏸" : L"▶");
    }

    void UpdateVolLabel() {
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0);
        wchar_t buf[16];
        swprintf(buf, 16, L"%d%%", vol);
        SetWindowTextW(m_staticVolPct, buf);
    }

    void UpdateSeekDisplay() {
        double len = m_audio.GetLength();
        double pos = m_audio.GetPosition();
        if (len > 0)
            SendMessageW(m_trackSeek, TBM_SETPOS, TRUE, (int)(pos / len * SEEK_RES));
    }

    void UpdateTimeDisplay() {
        double len = m_audio.GetLength();
        double pos = m_audio.GetPosition();
        SetWindowTextW(m_staticTime,
            (FormatTime(pos) + L" / " + FormatTime(len)).c_str());
    }

    void UpdatePlaylistSelection() {
        int displayIdx = -1;
        for (int i = 0; i < (int)m_filterMap.size(); i++) {
            if (m_filterMap[i] == m_currentIndex) { displayIdx = i; break; }
        }
        if (displayIdx >= 0) {
            ListView_SetItemState(m_playlistLV, displayIdx,
                LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_playlistLV, displayIdx, FALSE);
        }
    }

    void UpdateLVItem(int displayIdx) {
        if (displayIdx < 0 || displayIdx >= (int)m_filterMap.size()) return;
        int playlistIdx = m_filterMap[displayIdx];
        const auto& song = m_playlist.GetSong(playlistIdx);

        wchar_t num[16];
        swprintf(num, 16, L"%d", displayIdx + 1);
        ListView_SetItemText(m_playlistLV, displayIdx, 0, num);

        std::wstring display = song.title;
        if (!song.artist.empty())
            display = song.title + L" - " + song.artist;
        ListView_SetItemText(m_playlistLV, displayIdx, 1, &display[0]);

        std::wstring alb = song.album.empty() ? L"" : song.album;
        ListView_SetItemText(m_playlistLV, displayIdx, 2, &alb[0]);

        std::wstring dur = FormatDuration(song.duration);
        ListView_SetItemText(m_playlistLV, displayIdx, 3, &dur[0]);
    }

    void RefreshPlaylistUI() {
        SendMessageW(m_playlistLV, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(m_playlistLV);

        RebuildFilter();

        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        for (int i = 0; i < (int)m_filterMap.size(); i++) {
            li.iItem = i;
            ListView_InsertItem(m_playlistLV, &li);
            UpdateLVItem(i);
        }

        if (m_sortColumn >= 0) {
            for (int i = 0; i < 4; i++) {
                std::wstring label = COL_LABELS[i];
                if (i == m_sortColumn)
                    label += m_sortAscending ? L" ▲" : L" ▼";
                LVCOLUMNW lc = {};
                lc.mask = LVCF_TEXT;
                lc.pszText = &label[0];
                ListView_SetColumn(m_playlistLV, i, &lc);
            }
        }

        SendMessageW(m_playlistLV, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_playlistLV, NULL, TRUE);
    }

    
    // Hotkey bindings persistence
    
    // Hotkey bindings persistence (.hotkeys.txt) - ANSI
    
    void SaveHotkeyBindings() {
        std::wstring filePath = GetExeDirectory() + L"\\.hotkeys.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        for (int i = 0; i < m_hotkeyCount; i++) {
            std::wstring code = BindingToCode(m_hotkeys[i].vk, m_hotkeys[i].mod);
            char line[128];
            sprintf(line, "hk_%s=%S\n", HK_KEY_NAMES[i], code.c_str());
            WriteFile(hFile, line, (DWORD)strlen(line), &written, NULL);
        }
        CloseHandle(hFile);
    }

    void LoadHotkeyBindings() {
        std::wstring filePath = GetExeDirectory() + L"\\.hotkeys.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0 && size < 1024) {
            char buf[1024] = {};
            DWORD read;
            ReadFile(hFile, buf, size, &read, NULL);
            char* p = buf;
            while (*p) {
                char* nl = strchr(p, '\n');
                if (!nl) nl = p + strlen(p);
                char* end = nl;
                while (end > p && *(end - 1) == '\r') --end;
                char saved = *end;
                *end = '\0';
                if (strncmp(p, "hk_", 3) == 0) {
                    char* eq = strchr(p, '=');
                    if (eq) {
                        *eq = '\0';
                        const char* keyName = p + 3;
                        const char* codeStr = eq + 1;
                        // Find position by key name
                        for (int i = 0; i < m_hotkeyCount; i++) {
                            if (strcmp(keyName, HK_KEY_NAMES[i]) == 0) {
                                wchar_t codeW[64];
                                swprintf(codeW, 64, L"%S", codeStr);
                                int vk, mod;
                                if (CodeToBinding(codeW, vk, mod)) {
                                    m_hotkeys[i].vk = vk;
                                    m_hotkeys[i].mod = mod;
                                }
                                break;
                            }
                        }
                    }
                }
                *end = saved;
                p = nl + 1;
            }
        }
        CloseHandle(hFile);
    }

    
    // Settings persistence (.settings.txt)
    
    void SaveSettings() {
        SaveHotkeyBindings();
        std::wstring filePath = GetExeDirectory() + L"\\.settings.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        char buf[160];
        int len = sprintf(buf, "autoplay=%d\nremember_progress=%d\ntray_minimize=%d\nplay_mode=%d\n",
                          m_settingsAutoplay ? 1 : 0,
                          m_settingsRememberProgress ? 1 : 0,
                          m_settingsTray ? 1 : 0,
                          (int)m_audio.GetPlayMode());
        WriteFile(hFile, buf, (DWORD)len, &written, NULL);
        CloseHandle(hFile);
    }

    void LoadSettings() {
        LoadHotkeyBindings();

        // Now read just the plain settings
        std::wstring filePath = GetExeDirectory() + L"\\.settings.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        DWORD size = GetFileSize(hFile, NULL);
        int mode = 0;
        if (size > 0 && size < 2048) {
            char buf[2048] = {};
            DWORD read;
            ReadFile(hFile, buf, size, &read, NULL);
            char* p = buf;
            while (*p) {
                char* nl = strchr(p, '\n');
                if (!nl) nl = p + strlen(p);
                *nl = '\0';
                if (strncmp(p, "hk_", 3) != 0) {
                    if (sscanf(p, "autoplay=%d", &m_settingsAutoplay) == 1) {}
                    else if (sscanf(p, "remember_progress=%d", &m_settingsRememberProgress) == 1) {}
                    else if (sscanf(p, "tray_minimize=%d", &m_settingsTray) == 1) {}
                    else if (sscanf(p, "play_mode=%d", &mode) == 1) {
                        if (mode >= 0 && mode <= 2) {
                            m_audio.SetPlayMode(static_cast<PlayMode>(mode));
                        }
                    }
                }
                p = nl + 1;
            }
        }
        CloseHandle(hFile);
    }

    
    // Tray icon
    
    void AddTrayIcon() {
        if (m_trayIconAdded) return;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage = WM_APP_TRAY;
        nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        // Use current song info if available
        BuildTrayTipText(nid.szTip, 128);
        Shell_NotifyIconW(NIM_ADD, &nid);
        m_trayIconAdded = true;
    }

    void RemoveTrayIcon() {
        if (!m_trayIconAdded) return;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        m_trayIconAdded = false;
    }

    void BuildTrayTipText(wchar_t* buf, int bufLen) {
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
            std::wstring meta = m_audio.GetFormattedMetadata();
            std::wstring name;
            if (!meta.empty())
                name = meta;
            else
                name = GetDisplayName(m_playlist.GetFile(m_currentIndex));
            if (m_audio.IsLoaded() && m_audio.IsPlaying())
                swprintf(buf, bufLen, L"正在播放: %ls", name.c_str());
            else if (m_audio.IsLoaded())
                swprintf(buf, bufLen, L"已暂停: %ls", name.c_str());
            else
                wcsncpy(buf, WINDOW_TITLE, bufLen - 1);
        } else {
            wcsncpy(buf, WINDOW_TITLE, bufLen - 1);
        }
        buf[bufLen - 1] = L'\0';
    }

    void UpdateTrayTip() {
        if (!m_trayIconAdded) return;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_TIP;
        BuildTrayTipText(nid.szTip, 128);
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }

    void MinimizeToTray() {
        AddTrayIcon();
        ShowWindow(m_hwnd, SW_HIDE);
    }

    void HandleTrayMessage(WPARAM, LPARAM lp) {
        if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            ShowWindow(m_hwnd, SW_RESTORE);
            SetForegroundWindow(m_hwnd);
            RemoveTrayIcon();
        } else if (LOWORD(lp) == WM_RBUTTONDOWN) {
            HMENU popup = CreatePopupMenu();
            AppendMenuW(popup, MF_STRING, ID_TRAY_RESTORE, L"恢复");
            AppendMenuW(popup, MF_SEPARATOR, 0, NULL);
            AppendMenuW(popup, MF_STRING, ID_TRAY_EXIT, L"退出");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(m_hwnd);
            TrackPopupMenu(popup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, NULL);
            DestroyMenu(popup);
        }
    }

    
    // Last song progress
    void SaveLastSong() {
        if (m_currentIndex < 0 || !m_audio.IsLoaded()) return;
        std::wstring filePath = GetExeDirectory() + L"\\.lastsong.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        const WORD bom = 0xFEFF;
        WriteFile(hFile, &bom, 2, &written, NULL);
        wchar_t buf[64];
        swprintf(buf, 64, L"%d\n", m_currentIndex);
        WriteFile(hFile, buf, (DWORD)(wcslen(buf) * sizeof(wchar_t)), &written, NULL);
        double pos = m_audio.GetPosition();
        swprintf(buf, 64, L"%.3f\n", pos);
        WriteFile(hFile, buf, (DWORD)(wcslen(buf) * sizeof(wchar_t)), &written, NULL);
        const std::wstring& path = m_playlist.GetFile(m_currentIndex);
        std::wstring line = path + L"\n";
        WriteFile(hFile, line.c_str(), (DWORD)(line.size() * sizeof(wchar_t)), &written, NULL);
        CloseHandle(hFile);
    }

    bool LoadLastSong() {
        std::wstring filePath = GetExeDirectory() + L"\\.lastsong.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        DWORD size = GetFileSize(hFile, NULL);
        bool loaded = false;
        if (size > 2) {
            DWORD read = 0;
            std::wstring buf(size / 2 + 1, L'\0');
            ReadFile(hFile, &buf[0], size, &read, NULL);
            buf[read / 2] = L'\0';
            CloseHandle(hFile);
            const wchar_t* p = buf.c_str();
            if (*p == 0xFEFF) p++;
            const wchar_t* nl1 = wcschr(p, L'\n');
            if (!nl1) return false;
            int savedIndex = _wtoi(std::wstring(p, nl1 - p).c_str());
            p = nl1 + 1;
            const wchar_t* nl2 = wcschr(p, L'\n');
            if (!nl2) return false;
            double savedPos = _wtof(std::wstring(p, nl2 - p).c_str());
            p = nl2 + 1;
            const wchar_t* nl3 = wcschr(p, L'\n');
            size_t pathLen = nl3 ? (size_t)(nl3 - p) : wcslen(p);
            if (pathLen > 0 && p[pathLen-1] == L'\r') --pathLen;
            std::wstring savedPath(p, pathLen);
            if (!savedPath.empty()) {
                for (int i = 0; i < m_playlist.GetCount(); i++) {
                    if (m_playlist.GetFile(i) == savedPath) {
                        PlayFile(i);
                        if (savedPos > 0) m_audio.SetPosition(savedPos);
                        loaded = true;
                        break;
                    }
                }
            }
            if (!loaded && savedIndex >= 0 && savedIndex < m_playlist.GetCount()) {
                PlayFile(savedIndex);
                if (savedPos > 0) m_audio.SetPosition(savedPos);
                loaded = true;
            }
        } else { CloseHandle(hFile); }
        return loaded;
    }

    
    // Playlist persistence
    void SavePlaylist() {
        if (m_playlist.IsEmpty()) return;
        std::wstring filePath = GetExeDirectory() + L"\\.playlist.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        const WORD bom = 0xFEFF;
        WriteFile(hFile, &bom, 2, &written, NULL);
        for (int i = 0; i < m_playlist.GetCount(); i++) {
            std::wstring line = m_playlist.GetFile(i) + L"\n";
            WriteFile(hFile, line.c_str(), (DWORD)(line.size() * sizeof(wchar_t)), &written, NULL);
        }
        CloseHandle(hFile);
    }

    bool LoadPlaylist() {
        std::wstring filePath = GetExeDirectory() + L"\\.playlist.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        DWORD size = GetFileSize(hFile, NULL);
        bool loaded = false;
        if (size > 2) {
            DWORD read = 0;
            std::wstring buf(size / 2 + 1, L'\0');
            ReadFile(hFile, &buf[0], size, &read, NULL);
            buf[read / 2] = L'\0';
            const wchar_t* p = buf.c_str();
            if (*p == 0xFEFF) p++;
            m_playlist.Clear();
            while (*p) {
                const wchar_t* nl = wcschr(p, L'\n');
                size_t lineLen = nl ? (size_t)(nl - p) : wcslen(p);
                if (lineLen > 0 && p[lineLen-1] == L'\r') --lineLen;
                if (lineLen > 0) {
                    std::wstring line(p, lineLen);
                    if (GetFileAttributesW(line.c_str()) != INVALID_FILE_ATTRIBUTES)
                        m_playlist.AddFile(line);
                }
                p = nl ? nl + 1 : p + lineLen;
            }
            loaded = !m_playlist.IsEmpty();
        }
        if (loaded) {
            m_playlist.Sort(1, true);
            m_sortColumn = 1;
            m_sortAscending = true;
        }
        CloseHandle(hFile);
        RefreshPlaylistUI();
        if (loaded)
            SetWindowTextW(m_staticSong,
                (L"已加载 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
        return loaded;
    }

    
    // Last folder persistence
    void SaveLastFolder(const std::wstring& folderPath) {
        std::wstring filePath = GetExeDirectory() + L"\\.lastfolder.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        const WORD bom = 0xFEFF;
        WriteFile(hFile, &bom, 2, &written, NULL);
        std::wstring data = folderPath + L"\n";
        WriteFile(hFile, data.c_str(), (DWORD)(data.size() * sizeof(wchar_t)), &written, NULL);
        CloseHandle(hFile);
    }

    bool LoadFromLastFolder() {
        std::wstring filePath = GetExeDirectory() + L"\\.lastfolder.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        DWORD size = GetFileSize(hFile, NULL);
        bool loaded = false;
        if (size > 2) {
            DWORD read = 0;
            std::wstring buf(size / 2 + 1, L'\0');
            ReadFile(hFile, &buf[0], size, &read, NULL);
            buf[read / 2] = L'\0';
            const wchar_t* p = buf.c_str();
            if (*p == 0xFEFF) p++;
            size_t len = wcslen(p);
            while (len > 0 && (p[len-1] == L'\n' || p[len-1] == L'\r')) --len;
            if (len > 0) {
                std::wstring folder(p, len);
                if (GetFileAttributesW(folder.c_str()) != INVALID_FILE_ATTRIBUTES &&
                    (GetFileAttributesW(folder.c_str()) & FILE_ATTRIBUTE_DIRECTORY)) {
                    m_playlist.ScanFolder(folder);
                    m_sortColumn = 1;
                    m_sortAscending = true;
                    RefreshPlaylistUI();
                    if (!m_playlist.IsEmpty()) {
                        SetWindowTextW(m_staticSong,
                            (L"已加载 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
                        loaded = true;
                    }
                }
            }
        }
        CloseHandle(hFile);
        return loaded;
    }

    // Volume persistence
    void SaveVolume() {
        std::wstring filePath = GetExeDirectory() + L"\\.volume.txt";
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0);
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        char buf[16];
        int len = sprintf(buf, "%d\n", vol);
        WriteFile(hFile, buf, (DWORD)len, &written, NULL);
        CloseHandle(hFile);
    }

    void LoadVolume() {
        std::wstring filePath = GetExeDirectory() + L"\\.volume.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0 && size < 64) {
            DWORD read = 0;
            char buf[64] = {};
            ReadFile(hFile, buf, size, &read, NULL);
            int vol = atoi(buf);
            if (vol >= 0 && vol <= 100) {
                m_audio.SetVolume(vol);
                SendMessageW(m_sliderVol, TBM_SETPOS, TRUE, vol);
                UpdateVolLabel();
            }
        }
        CloseHandle(hFile);
    }
};

// Hotkey dialog procedure
static LRESULT CALLBACK HotkeyDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
    if (msg == WM_COMMAND) {
        int ctrlId = LOWORD(wp);
        HKDlgCtx* c = (HKDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (!c) return DefWindowProcW(hDlg, msg, wp, lp);

        if (ctrlId >= 200 && ctrlId < 200 + c->count) {
            int idx = ctrlId - 200;
            c->recording = idx;
            SetWindowTextW(GetDlgItem(hDlg, 100 + idx), L"[按下新按键...]");
            return 0;
        }
        if (ctrlId == 1 || ctrlId == IDOK) {
            c->result = 1;
            DestroyWindow(hDlg);
            return 0;
        }
        if (ctrlId == 2 || ctrlId == IDCANCEL) {
            DestroyWindow(hDlg);
            return 0;
        }
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// Export stats data to JSON file (for visualization tools)
static void ExportStatsToJson(const StatsDlgCtx* ctx) {
    std::wstring filePath = GetExeDirectory() + L"\\.stats_export.json";
    std::string json = ctx->win->ExportHistoryToJson();

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(hFile, json.c_str(), (DWORD)json.size(), &written, NULL);
    CloseHandle(hFile);
}

// Stats dialog procedure
static LRESULT CALLBACK StatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
    if (msg == WM_DESTROY) {
        StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        delete c;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
        return 0;
    }
    if (msg == WM_COMMAND) {
        StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (!c) return DefWindowProcW(hDlg, msg, wp, lp);
        int id = LOWORD(wp);

        if (id == IDCANCEL) {
            DestroyWindow(hDlg);
            return 0;
        }

        if (id == 304) { // Refresh button
            if (SendMessageW(c->hRadio7, BM_GETCHECK, 0, 0) == BST_CHECKED)
                c->rangeDays = 7;
            else if (SendMessageW(c->hRadio30, BM_GETCHECK, 0, 0) == BST_CHECKED)
                c->rangeDays = 30;
            else {
                wchar_t buf[16];
                GetWindowTextW(c->hEditCustom, buf, 16);
                int n = _wtoi(buf);
                if (n < 1) n = 1;
                c->rangeDays = n * 30;
            }
            c->win->RefreshStatsDisplay(c);
            return 0;
        }

        if (id == 300) { // Radio 7
            c->rangeDays = 7;
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 301) { // Radio 30
            c->rangeDays = 30;
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 302) { // Radio Custom
            wchar_t buf[16];
            GetWindowTextW(c->hEditCustom, buf, 16);
            int n = _wtoi(buf);
            if (n < 1) n = 1;
            c->rangeDays = n * 30;
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 305) { // Export button
            ExportStatsToJson(c);
            MessageBoxW(hDlg, L"统计数据已导出到 .stats_export.json", L"导出成功", MB_OK);
            return 0;
        }
        return 0;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    InitCommonControls();

    MainWindow win;
    if (!win.Create(hInst, nCmdShow)) {
        CoUninitialize();
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
