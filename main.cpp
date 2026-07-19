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

class MainWindow {
public:
    MainWindow()
        : m_hwnd(NULL), m_hInst(NULL)
        , m_playlistLV(NULL)
        , m_btnPrev(NULL), m_btnPlay(NULL), m_btnNext(NULL), m_btnMode(NULL)
        , m_trackSeek(NULL), m_sliderVol(NULL), m_staticVolPct(NULL)
        , m_staticTime(NULL), m_staticSong(NULL)
        , m_currentIndex(-1), m_userDraggingSeek(false)
        , m_sortColumn(-1), m_sortAscending(true)
        , m_shufflePos(0)
        , m_settingsAutoplay(true), m_settingsRememberProgress(true)
        , m_settingsTray(true), m_trayIconAdded(false)
    {
        srand((unsigned)time(NULL));
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

private:
    // ---- Controls ----
    HWND m_hwnd;
    HINSTANCE m_hInst;
    HWND m_playlistLV;
    HWND m_btnPrev, m_btnPlay, m_btnNext, m_btnMode;
    HWND m_trackSeek, m_sliderVol, m_staticVolPct;
    HWND m_staticTime, m_staticSong;

    // ---- State ----
    AudioEngine      m_audio;
    PlaylistManager  m_playlist;
    int              m_currentIndex;
    bool             m_userDraggingSeek;
    int              m_sortColumn;
    bool             m_sortAscending;

    // ---- Shuffle ----
    std::vector<int> m_shuffleOrder;
    int              m_shufflePos;

    // ---- Settings ----
    bool m_settingsAutoplay;
    bool m_settingsRememberProgress;
    bool m_settingsTray;
    bool m_trayIconAdded;

    HFONT m_hFont;

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
        // In-app keyboard shortcuts (Ctrl+key)
        if (msg == WM_KEYDOWN && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
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
            case WM_HOTKEY:            OnGlobalHotkey((int)wp);  return 0;
            case WM_NOTIFY:            return OnNotify(wp, lp);
            default:
                if (msg == WM_USER_SONG_END) { OnSongEnd(); return 0; }
                if (msg == WM_APP_TRAY) { HandleTrayMessage(wp, lp); return 0; }
                return DefWindowProcW(m_hwnd, msg, wp, lp);
        }
    }

    void OnCreate() {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&icc);
        CreateMenuBar();
        CreateControls();

        if (!m_audio.Initialize(m_hwnd)) {
            MessageBoxW(m_hwnd,
                L"无法初始化音频引擎 (bass.dll)。\n\n"
                L"请确保 bass.dll 位于程序目录或系统路径中。\n"
                L"下载地址: https://www.un4seen.com/bass.html",
                L"音频初始化失败", MB_OK | MB_ICONWARNING);
        }

        LoadSettings();
        UpdateSettingsMenu();

        if (!LoadFromLastFolder()) {
            LoadPlaylist();
        }

        LoadVolume();
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
        SavePlaylist();
        SaveVolume();
        SaveSettings();
        UnregisterHotKeys();
        RemoveTrayIcon();
        m_audio.Cleanup();
        DestroyWindow(m_hwnd);
    }

    // ==========================================
    // Menu bar
    // ==========================================
    void CreateMenuBar() {
        HMENU bar = CreateMenu();

        HMENU fileMenu = CreatePopupMenu();
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_OPENFOLDER, L"打开文件夹(&O)...");
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_ADDFILES, L"添加歌曲(&A)...");
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_EXIT, L"退出(&X)");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)fileMenu, L"文件(&F)");

        HMENU settingsMenu = CreatePopupMenu();
        AppendMenuW(settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_AUTOPLAY,
            L"启动后自动播放");
        AppendMenuW(settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_REMEMBER,
            L"记住播放进度");
        AppendMenuW(settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_TRAY,
            L"最小化到托盘");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)settingsMenu, L"设置(&S)");

        HMENU playMenu = CreatePopupMenu();
        AppendMenuW(playMenu, MF_STRING | MF_CHECKED, ID_PLAY_SEQUENTIAL, L"顺序播放(&S)");
        AppendMenuW(playMenu, MF_STRING, ID_PLAY_REPEATONE, L"单曲循环(&R)");
        AppendMenuW(playMenu, MF_STRING, ID_PLAY_SHUFFLE, L"随机播放(&H)");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)playMenu, L"播放(&P)");

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
        const int ctrlAreaH = 118;
        int listH = h - ctrlAreaH;
        if (listH < 30) listH = 30;

        SetWindowPos(m_playlistLV, NULL, M, M, w - 2 * M, listH - 2 * M, SWP_NOZORDER);

        int y = listH;
        const int BH = 28;

        SetWindowPos(m_btnMode, NULL, M, y + 2, 90, BH, SWP_NOZORDER);
        int bx = M + 96;
        SetWindowPos(m_btnPrev, NULL, bx, y + 2, 36, BH, SWP_NOZORDER);
        bx += 42;
        SetWindowPos(m_btnPlay, NULL, bx, y + 2, 36, BH, SWP_NOZORDER);
        bx += 42;
        SetWindowPos(m_btnNext, NULL, bx, y + 2, 36, BH, SWP_NOZORDER);

        int volPctW = 36;
        int volW = 130;
        int volX = w - M - volPctW - volW;
        SetWindowPos(m_staticVolPct, NULL, volX, y + 4, volPctW, 20, SWP_NOZORDER);
        SetWindowPos(m_sliderVol, NULL, volX + volPctW, y + 2, volW, BH, SWP_NOZORDER);

        y += BH + 6;
        SetWindowPos(m_trackSeek, NULL, M, y + 2, w - 2 * M, 24, SWP_NOZORDER);

        y += 28;
        int timeW = 170;
        SetWindowPos(m_staticTime, NULL, M, y + 2, timeW, 20, SWP_NOZORDER);
        SetWindowPos(m_staticSong, NULL, M + timeW + 8, y + 2,
                     w - M - timeW - 16, 20, SWP_NOZORDER);
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

    // ==========================================
    // WM_COMMAND
    // ==========================================
    void OnCommand(WPARAM wp, LPARAM lp) {
        WORD id = LOWORD(wp);
        HWND hCtrl = (HWND)lp;

        if (hCtrl == NULL) {
            switch (id) {
                case ID_FILE_OPENFOLDER: OpenFolder(); break;
                case ID_FILE_ADDFILES:   AddFiles();   break;
                case ID_FILE_EXIT:       OnRealClose(); break;
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
            }
        }
    }

    // ==========================================
    // WM_HSCROLL
    // ==========================================
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
                if (code == TB_ENDTRACK) m_userDraggingSeek = false;
            }
        } else if (hCtrl == m_sliderVol) {
            int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0);
            m_audio.SetVolume(vol);
            UpdateVolLabel();
        }
    }

    // ==========================================
    // WM_NOTIFY
    // ==========================================
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
                    if (ia->iItem >= 0) PlayFile(ia->iItem);
                    return 0;
                }
            }
        }
        return 0;
    }

    // ==========================================
    // Sort
    // ==========================================
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

        for (int i = 0; i < 4; i++) {
            std::wstring label = COL_LABELS[i];
            if (i == column)
                label += m_sortAscending ? L" ▲" : L" ▼";
            LVCOLUMNW lc = {};
            lc.mask = LVCF_TEXT;
            lc.pszText = &label[0];
            ListView_SetColumn(m_playlistLV, i, &lc);
        }

        RefreshPlaylistUI();

        if (!curPath.empty()) {
            for (int i = 0; i < m_playlist.GetCount(); i++) {
                if (m_playlist.GetFile(i) == curPath) {
                    m_currentIndex = i;
                    ListView_SetItemState(m_playlistLV, i, LVIS_SELECTED | LVIS_FOCUSED,
                                          LVIS_SELECTED | LVIS_FOCUSED);
                    break;
                }
            }
        }
    }

    // ==========================================
    // Timer
    // ==========================================
    void OnTimer() {
        if (m_audio.IsLoaded() && !m_userDraggingSeek) {
            UpdateSeekDisplay();
            UpdateTimeDisplay();
        }
    }

    // ==========================================
    // Fisher-Yates shuffle (non-repeating)
    // ==========================================
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

    // ==========================================
    // Song end
    // ==========================================
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

    // ==========================================
    // Open folder
    // ==========================================
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
            m_sortColumn = -1;

            SaveLastFolder(path);
            m_playlist.ScanFolder(path);
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

    // ==========================================
    // Playback controls
    // ==========================================
    // ==========================================
    // Add files from multi-file dialog
    // ==========================================
    void AddFiles() {
        wchar_t buf[65536];
        buf[0] = L'0';

        OPENFILENAMEW ofn = {};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = m_hwnd;
        ofn.lpstrFile       = buf;
        ofn.nMaxFile        = 65536;
        ofn.lpstrFilter     = L"音频文件 (*.mp3;*.flac;*.wav)0*.mp3;*.flac;*.wav0所有文件 (*.*)0*.*0";
        ofn.nFilterIndex    = 1;
        ofn.Flags           = OFN_ALLOWMULTISELECT | OFN_EXPLORER |
                              OFN_HIDEREADONLY | OFN_FILEMUSTEXIST |
                              OFN_LONGNAMES | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&ofn)) return;

        std::wstring dir = buf;
        size_t offset = dir.size() + 1;
        bool added = false;
        bool hadItems = !m_playlist.IsEmpty();

        if (buf[offset] == L'0') {
            std::wstring ext;
            const wchar_t* dot = wcsrchr(buf, L'.');
            if (dot) ext = dot;
            if (PlaylistManager::IsAudioExtension(ext)) {
                m_playlist.AddFile(buf);
                added = true;
            }
        } else {
            while (buf[offset] != L'0') {
                std::wstring fullPath = dir + L"\\" + (buf + offset);
                std::wstring ext;
                const wchar_t* dot = wcsrchr(buf + offset, L'.');
                if (dot) ext = dot;
                if (PlaylistManager::IsAudioExtension(ext)) {
                    bool dup = false;
                    for (int i = 0; i < m_playlist.GetCount(); i++) {
                        if (m_playlist.GetFile(i) == fullPath) { dup = true; break; }
                    }
                    if (!dup) {
                        m_playlist.AddFile(fullPath);
                        added = true;
                    }
                }
                offset += wcslen(buf + offset) + 1;
            }
        }

        if (added) {
            RefreshPlaylistUI();
            if (m_audio.GetPlayMode() == PlayMode::Shuffle)
                Reshuffle();
            if (!hadItems) {
                PlayFile(0);
            } else {
                SetWindowTextW(m_staticSong,
                    (L"已添加 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
                UpdateUI();
            }
        }
    }

    void OnPlayPause() {
        if (!m_audio.IsLoaded()) return;
        if (m_audio.IsPlaying()) {
            m_audio.Pause();
            SetWindowTextW(m_staticSong, L"已暂停");
            std::wstring tipText;
            if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
                std::wstring meta = m_audio.GetFormattedMetadata();
                tipText = meta.empty() ? GetDisplayName(m_playlist.GetFile(m_currentIndex)) : meta;
            }
            UpdateTrayTip(L"已暂停: " + tipText);
        } else {
            m_audio.Play();
            if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
                std::wstring meta = m_audio.GetFormattedMetadata();
                const auto& path = m_playlist.GetFile(m_currentIndex);
                if (!meta.empty()) {
                    SetWindowTextW(m_staticSong, (L"正在播放: " + meta).c_str());
                    UpdateTrayTip(L"正在播放: " + meta);
                } else {
                    SetWindowTextW(m_staticSong, (L"正在播放: " + GetDisplayName(path)).c_str());
                    UpdateTrayTip(L"正在播放: " + GetDisplayName(path));
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
            if (m_shufflePos >= (int)m_shuffleOrder.size())
                Reshuffle();
            idx = m_shuffleOrder[m_shufflePos];
        } else {
            idx = (m_currentIndex + 1) % count;
        }
        PlayFile(idx);
    }

    // ==========================================
    // Play mode
    // ==========================================
    void OnCycleMode() {
        PlayMode old = m_audio.GetPlayMode();
        m_audio.CyclePlayMode();
        if (m_audio.GetPlayMode() == PlayMode::Shuffle && old != PlayMode::Shuffle)
            Reshuffle();
        UpdateModeUI();
    }

    void SetPlayMode(PlayMode mode) {
        m_audio.SetPlayMode(mode);
        if (mode == PlayMode::Shuffle)
            Reshuffle();
        UpdateModeUI();
    }

    void UpdateModeUI() {
        PlayMode pm = m_audio.GetPlayMode();
        const wchar_t* labels[] = { L"顺序播放", L"单曲循环", L"随机播放" };
        SetWindowTextW(m_btnMode, labels[(int)pm]);

        HMENU bar = GetMenu(m_hwnd);
        HMENU pmMenu = GetSubMenu(bar, 2);
        CheckMenuItem(pmMenu, ID_PLAY_SEQUENTIAL,
            MF_BYCOMMAND | (pm == PlayMode::Sequential ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(pmMenu, ID_PLAY_REPEATONE,
            MF_BYCOMMAND | (pm == PlayMode::RepeatOne ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(pmMenu, ID_PLAY_SHUFFLE,
            MF_BYCOMMAND | (pm == PlayMode::Shuffle ? MF_CHECKED : MF_UNCHECKED));
    }

    void UpdateSettingsMenu() {
        HMENU bar = GetMenu(m_hwnd);
        HMENU settingsMenu = GetSubMenu(bar, 1);
        CheckMenuItem(settingsMenu, ID_SETTINGS_AUTOPLAY,
            MF_BYCOMMAND | (m_settingsAutoplay ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(settingsMenu, ID_SETTINGS_REMEMBER,
            MF_BYCOMMAND | (m_settingsRememberProgress ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(settingsMenu, ID_SETTINGS_TRAY,
            MF_BYCOMMAND | (m_settingsTray ? MF_CHECKED : MF_UNCHECKED));
    }

    // ==========================================
    // Keyboard shortcuts
    // ==========================================
    bool HandleAccelerator(int vk) {
        switch (vk) {
            case 'P': OnPlayPause();       return true;
            case VK_LEFT:  OnPrev();       return true;
            case VK_RIGHT: OnNext();       return true;
            case VK_UP:    AdjustVolume(5);   return true;
            case VK_DOWN:  AdjustVolume(-5);  return true;
            case 'O': ShowWindow(m_hwnd, SW_RESTORE); SetForegroundWindow(m_hwnd); RemoveTrayIcon(); return true;
            case 'K': MinimizeToTray();    return true;
        }
        return false;
    }

    void OnGlobalHotkey(int id) {
        switch (id) {
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
        RegisterHotKey(h, HKID_PLAYPAUSE, MOD_CONTROL | MOD_ALT, 'P');
        RegisterHotKey(h, HKID_PREV,      MOD_CONTROL | MOD_ALT, VK_LEFT);
        RegisterHotKey(h, HKID_NEXT,      MOD_CONTROL | MOD_ALT, VK_RIGHT);
        RegisterHotKey(h, HKID_VOLUP,     MOD_CONTROL | MOD_ALT, VK_UP);
        RegisterHotKey(h, HKID_VOLDN,     MOD_CONTROL | MOD_ALT, VK_DOWN);
        RegisterHotKey(h, HKID_RESTORE,   MOD_CONTROL | MOD_ALT, 'O');
        RegisterHotKey(h, HKID_MINIMIZE,  MOD_CONTROL | MOD_ALT, 'K');
    }

    void UnregisterHotKeys() {
        HWND h = m_hwnd;
        UnregisterHotKey(h, HKID_PLAYPAUSE);
        UnregisterHotKey(h, HKID_PREV);
        UnregisterHotKey(h, HKID_NEXT);
        UnregisterHotKey(h, HKID_VOLUP);
        UnregisterHotKey(h, HKID_VOLDN);
        UnregisterHotKey(h, HKID_RESTORE);
        UnregisterHotKey(h, HKID_MINIMIZE);
    }

    void AdjustVolume(int delta) {
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0) + delta;
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        m_audio.SetVolume(vol);
        SendMessageW(m_sliderVol, TBM_SETPOS, TRUE, vol);
        UpdateVolLabel();
    }

    // ==========================================
    // Play file
    // ==========================================
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
        UpdateLVItem(index);

        if (!meta.empty())
            SetWindowTextW(m_staticSong, (L"正在播放: " + meta).c_str());
        else
            SetWindowTextW(m_staticSong, (L"正在播放: " + GetDisplayName(path)).c_str());

        SetTimer(m_hwnd, TIMER_ID_SEEK, 500, NULL);

        // Update tray tooltip
        std::wstring tipText = meta.empty() ? GetDisplayName(path) : meta;
        UpdateTrayTip(L"正在播放: " + tipText);

        if (m_settingsRememberProgress) {
            SaveLastSong();
        }
    }

    // ==========================================
    // UI helpers
    // ==========================================
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
        ListView_SetItemState(m_playlistLV, m_currentIndex,
            LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(m_playlistLV, m_currentIndex, FALSE);
    }

    void UpdateLVItem(int index) {
        if (index < 0 || index >= m_playlist.GetCount()) return;
        const auto& song = m_playlist.GetSong(index);

        wchar_t num[16];
        swprintf(num, 16, L"%d", index + 1);
        ListView_SetItemText(m_playlistLV, index, 0, num);

        std::wstring display = song.title;
        if (!song.artist.empty())
            display = song.title + L" - " + song.artist;
        ListView_SetItemText(m_playlistLV, index, 1, &display[0]);

        std::wstring alb = song.album.empty() ? L"" : song.album;
        ListView_SetItemText(m_playlistLV, index, 2, &alb[0]);

        std::wstring dur = FormatDuration(song.duration);
        ListView_SetItemText(m_playlistLV, index, 3, &dur[0]);
    }

    void RefreshPlaylistUI() {
        SendMessageW(m_playlistLV, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(m_playlistLV);

        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        for (int i = 0; i < m_playlist.GetCount(); i++) {
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

    // ==========================================
    // Settings persistence (.settings.txt)
    // ==========================================
    void SaveSettings() {
        std::wstring filePath = GetExeDirectory() + L"\\.settings.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        char buf[128];
        int len = sprintf(buf, "autoplay=%d\nremember_progress=%d\ntray_minimize=%d\n",
                          m_settingsAutoplay ? 1 : 0,
                          m_settingsRememberProgress ? 1 : 0,
                          m_settingsTray ? 1 : 0);
        WriteFile(hFile, buf, len, &written, NULL);
        CloseHandle(hFile);
    }

    void LoadSettings() {
        std::wstring filePath = GetExeDirectory() + L"\\.settings.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0 && size < 256) {
            DWORD read = 0;
            char buf[256] = {};
            ReadFile(hFile, buf, size, &read, NULL);
            char* p = buf;
            while (*p) {
                char* nl = strchr(p, '\n');
                if (!nl) nl = p + strlen(p);
                *nl = '\0';
                if (sscanf(p, "autoplay=%d", &m_settingsAutoplay) == 1) {}
                else if (sscanf(p, "remember_progress=%d", &m_settingsRememberProgress) == 1) {}
                else if (sscanf(p, "tray_minimize=%d", &m_settingsTray) == 1) {}
                p = nl + 1;
            }
        }
        CloseHandle(hFile);
    }

    // ==========================================
    // Tray icon
    // ==========================================
    void AddTrayIcon() {
        if (m_trayIconAdded) return;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage = WM_APP_TRAY;
        nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        wcscpy(nid.szTip, WINDOW_TITLE);
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

    void UpdateTrayTip(const std::wstring& text) {
        if (!m_trayIconAdded) return;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_TIP;
        wcsncpy(nid.szTip, text.c_str(), 127);
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

    // ==========================================
    // Last song progress (.lastsong.txt)
    // ==========================================
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
                        if (savedPos > 0) {
                            m_audio.SetPosition(savedPos);
                        }
                        loaded = true;
                        break;
                    }
                }
            }

            if (!loaded && savedIndex >= 0 && savedIndex < m_playlist.GetCount()) {
                PlayFile(savedIndex);
                if (savedPos > 0) {
                    m_audio.SetPosition(savedPos);
                }
                loaded = true;
            }
        } else {
            CloseHandle(hFile);
        }
        return loaded;
    }

    // ==========================================
    // Playlist persistence
    // ==========================================
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
            WriteFile(hFile, line.c_str(),
                (DWORD)(line.size() * sizeof(wchar_t)), &written, NULL);
        }
        CloseHandle(hFile);
    }

    void LoadPlaylist() {
        std::wstring filePath = GetExeDirectory() + L"\\.playlist.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        DWORD size = GetFileSize(hFile, NULL);
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
        }
        CloseHandle(hFile);
        RefreshPlaylistUI();
        if (!m_playlist.IsEmpty())
            SetWindowTextW(m_staticSong,
                (L"已加载 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
    }

    // ==========================================
    // Last folder persistence
    // ==========================================
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

    // ==========================================
    // Volume persistence
    // ==========================================
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
