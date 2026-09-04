#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>

// MusicPlayer version
static const wchar_t* APP_VERSION = L"1.4.6";

// Changelog — shown in the About dialog
static const wchar_t* CHANGELOG =
    L"v1.4.6\r\n"
    L"  - 修复: 重启 Windows 资源管理器后托盘图标消失且无法自动恢复 (根因: Win11/第三方任务栏不广播 TaskbarCreated, 依赖该消息的重加从未触发; 改为 5 秒周期性心跳, 用 NIM_MODIFY 重新断言图标、失败即 NIM_ADD, 并清除 NIS_HIDDEN 隐藏位强制图标可见)\r\n"
    L"  - 修复 .error.log 中\"WriteLog 的落盘格式跟项目其它文件不一致\"的问题。现在空文件先写 UTF-8 BOM，消息用已有的 WideToUtf8 转成 UTF-8 再写盘\r\n"
    L"\r\n"
    L"v1.4.5\r\n"
    L"  - 新增: 设置 → 重新统计歌曲时长, 全量重扫歌单时长; 对比 .durations.txt 缓存自动跳过未变化歌曲, 扫描期间防重复触发\r\n"
    L"\r\n"
    L"v1.4.4\r\n"
    L"  - 修复: 重启 Windows 资源管理器后系统托盘图标消失、窗口无法从托盘恢复 (改为监听 TaskbarCreated 消息并在资源管理器重建后自动重新添加托盘图标)\r\n"
    L"  - 调整播放列表列宽: 标题列改为随窗口宽度自适应伸缩填满剩余空间, # / 专辑 / 时长保持固定\r\n"
    L"  - 统计窗口: 「统计周期」更名为「统计范围」, 新增「所有」选项 (统计全部历史)\r\n"
    L"\r\n"
    L"v1.4.3\r\n"
    L"  - 修复: 合盖休眠期间仍被计入听歌时长, 导致一天累计虚增至 23 小时 59 分 (改为休眠时结束当前计时段并落盘, 唤醒后若仍在播放则重新计时)\r\n"
    L"\r\n"
    L"v1.4.2\r\n"
    L"  - 计时区间现在严格对应\"音频正在播放\"：StartListening 只在 Play() 成功后、以及暂停恢复（淡入）时开启；所有停止播放的路径（换歌、播完、暂停淡出、删除当前歌、换文件夹、关程序）都会 StopListening\r\n"
    L"  - 一天的累计时长不会再虚增超过 24 小时\r\n"
    L"  - 墙钟计时的既有语义不变（倍速、静音仍按实际经过时间计）\r\n"
    L"  - 修复跨午夜听歌时长整段计入结束日的问题：改为按本地午夜切分，分别计入前后两天\r\n"
    L"\r\n"
    L"v1.4.1\r\n"
    L"  - 修复: 启动时缓存命中的歌曲时长仍显示 --:-- (改为先应用缓存再渲染列表)\r\n"
    L"\r\n"
    L"v1.4.0\r\n"
    L"  - 歌曲时长缓存: 启动时先读本地 .durations.txt, 命中则直接显示具体时长, 否则仍为 --:--\r\n"
    L"  - 播放中或空闲时后台逐步扫描未知时长歌曲 (每 0.5 秒一首), 扫描结果即时刷新并写入缓存\r\n"
    L"  - 缓存记录文件大小与修改时间, 歌曲被替换后自动重新测量\r\n"
    L"\r\n"
    L"v1.3.0\r\n"
    L"  - 统计数据导出升级: 新增 CSV 格式, 可直接用 Excel 打开并可视化\r\n"
    L"  - 导出前弹出选择窗口, 可勾选导出内容 (每日听歌记录/每周统计/常听歌曲排行/总计)\r\n"
    L"  - 导出范围跟随统计窗口当前选中的日期区间\r\n"
    L"  - 改为\"另存为\"对话框选择保存位置, 不再固定写到程序目录\r\n"
    L"\r\n"
    L"v1.2.1\r\n"
    L"  - 「关于 MusicPlayer」窗口现在支持自由拉伸,并限定了最大尺寸\r\n"
    L"\r\n"
    L"v1.2.0\r\n"
    L"  - 新增“音量平衡”功能（设置 → 音量平衡）：自动分析每首歌曲响度并统一音量，无需手动调节\r\n"
    L"  - 响度分析结果缓存至 .loudness.txt，分析过的歌曲再次播放即时生效\r\n"
    L"  - 修正音量刻度（消除全局音量与单曲音量的双重衰减，显示 80% 即为真实 80%）\r\n"
    L"\r\n"
    L"v1.1.0\r\n"
    L"  - 音量条左侧添加喇叭静音按钮（点击静音，再次点击恢复原音量）\r\n"
    L"\r\n"
    L"v1.0.1\r\n"
    L"  - 修复关于对话框文本底部裁剪（指定 Microsoft YaHei 字体消除字体链接偏差）\r\n"
    L"  - 修复关于对话框更新日志文本换行（\\n → \\r\\n）\r\n"
    L"  - 修复关闭关于对话框后播放器窗口失焦\r\n"
    L"\r\n"
    L"v1.0.0\r\n"
    L"  - 添加定时刷写听歌历史，防止直接关机丢失数据\r\n"
    L"  - 添加关于对话框（版本号 + 更新历史）\r\n"
    L"\r\n"
    L"v0.9.0 (previous)\r\n"
    L"  - 添加 ID3 标签读取、搜索、撤销删除\r\n"
    L"  - 修复托盘图标显示\r\n"
    L"  - 添加日历日期范围选择\r\n"
    L"  - 修复暂停淡出效果的 bug\r\n"
    L"  - 添加单实例检查\r\n"
    L"  - 暂停时添加淡出效果 (BASS_ChannelSlideAttribute)\r\n"
    L"  - 变速播放 (0.1x - 10x) 支持\r\n"
    L"  - 添加 \"我常听的\" 播放次数统计\r\n"
    L"  - 鼠标滚轮调节音量\r\n"
    L"  - 播放模式支持顺序播放、单曲循环、随机播放\r\n"
    L"  - 窗口比例缩放自适应\r\n";

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <utility>

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

    // 关于对话框尺寸: 默认/最小 = 420x380, 最大拉伸上限 = 800x640
    constexpr int ABOUT_MIN_W = 420;
    constexpr int ABOUT_MIN_H = 380;
    constexpr int ABOUT_MAX_W = 800;
    constexpr int ABOUT_MAX_H = 640;
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

static std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return {};
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &out[0], n, NULL, NULL);
    return out;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    if (len <= 0) return L"";
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
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

static void WriteLog(const wchar_t* format, ...) {
    std::wstring filePath = GetExeDirectory() + L"\\.error.log";
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    // 空文件先写 UTF-8 BOM, 便于文本编辑器正确识别编码
    if (GetFileSize(hFile, NULL) == 0) {
        const BYTE bomUtf8[] = { 0xEF, 0xBB, 0xBF };
        DWORD written = 0;
        WriteFile(hFile, bomUtf8, 3, &written, NULL);
    }
    SetFilePointer(hFile, 0, NULL, FILE_END);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    wchar_t ts[64];
    wcsftime(ts, 64, L"[%Y-%m-%d %H:%M:%S] ", t);

    wchar_t buf[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buf, 1024, format, args);
    va_end(args);

    // 与项目其它文件一致, 用 UTF-8 落盘, 避免裸写 UTF-16LE 导致日志被读成乱码
    std::string line = WideToUtf8(std::wstring(ts) + buf + L"\n");
    DWORD written = 0;
    WriteFile(hFile, line.data(), (DWORD)line.size(), &written, NULL);
    CloseHandle(hFile);
}

static void Log(const wchar_t* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    wchar_t msg[512];
    vswprintf(msg, 512, fmt, a);
    va_end(a);
    WriteLog(msg);
}

static const wchar_t* COL_LABELS[4] = { L"#", L"标题", L"专辑", L"时长" };
// 列宽: # / 标题 / 专辑 / 时长; 标题列为自适应列(在 LayoutControls 中随窗口宽度伸缩), 其余为固定值
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

// Pure search logic: check if song text fields match a query
static bool SearchMatchesText(const std::wstring& query,
                               const std::wstring& title,
                               const std::wstring& artist,
                               const std::wstring& album) {
    if (title.empty() && artist.empty() && album.empty())
        return true;
    if (query.empty()) return true;

    std::wstring q = query;
    for (auto& c : q) c = towlower(c);

    auto contains = [&](const std::wstring& s) -> bool {
        std::wstring ls = s;
        for (auto& c : ls) c = towlower(c);
        return ls.find(q) != std::wstring::npos;
    };
    return contains(title) || contains(artist) || contains(album);
}

// Forward declaration
class MainWindow;

// DlgCtx - Hotkey dialog context (needed by both MainWindow and HotkeyDlgProc)
struct HKDlgCtx { HotkeyBinding* bindings; int recording; int count; MainWindow* win; int result; };

static LRESULT CALLBACK HotkeyDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

struct StatsDlgCtx;
struct StatsDlgCtx {
    MainWindow* win;
    int rangeDays;
    bool useCalendarRange;
    bool useAllRange;
    SYSTEMTIME calStart, calEnd;

    int baseW, baseH;
    int yDayList, yWeekList, yPlayList, yTotal, yButtons;
    int hDay, hWeek, hPlay;

    HWND hRadio7, hRadio30, hRadioAll, hRadioCustom;
    HWND hDtpStart, hDtpEnd;
    HWND hDayList, hWeekList, hPlayCountList, hTotalText;
    HWND hLabelDay, hLabelWeek, hLabelPlay;
    HWND hDlg;
    bool dtpGuard; // guard against re-entrancy from DTM_SETSYSTEMTIME
};
// 统计数据导出对话框上下文
struct ExportCtx {
    HWND hDlg;
    HWND hRadioCsv, hRadioJson;
    HWND hChkDaily, hChkWeekly, hChkTop, hChkTotal;
    bool asCsv;
    bool includeDaily, includeWeekly, includeTopSongs, includeTotal;
    int result;
};
static LRESULT CALLBACK ExportStatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

// 时长缓存条目: 记录文件大小与修改时间用于缓存失效判断
struct DurationCacheEntry {
    double    duration;   // 秒
    ULONGLONG size;
    FILETIME  mtime;
};

static LRESULT CALLBACK StatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK SpeedInputDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
static void LayoutStatsControls(StatsDlgCtx* c, int clientW, int clientH);
static void ComputeStatsRange(const StatsDlgCtx* ctx, std::string& from, std::string& to);

// 关于对话框子控件与上下文
struct AboutCtx {
    HWND hDlg;
    HFONT hGuiFont;     // 正文/编辑框字体
    HFONT hTitleFont;   // 标题字体
    HWND hTitle;
    HWND hVersion;
    HWND hChangelogLabel;
    HWND hChangelog;
    HWND hClose;
    int lineH;          // 编辑框行高, 用于格式矩形对齐
};

// 根据当前客户区大小布局关于对话框子控件 (创建时与 WM_SIZE 时调用)
static void LayoutAboutControls(AboutCtx* ctx) {
    if (!ctx->hChangelog) return;
    RECT cr;
    GetClientRect(ctx->hDlg, &cr);
    int clientW = cr.right, clientH = cr.bottom;

    const int marginX = 20, gap = 12;
    const int btnW = 80, btnH = 28;
    int btnY = clientH - btnH - gap;
    int editY = 95;
    int editH = btnY - gap - editY;

    MoveWindow(ctx->hTitle, marginX, 15, clientW - marginX * 2, 28, TRUE);
    MoveWindow(ctx->hVersion, marginX, 48, clientW - marginX * 2, 20, TRUE);
    MoveWindow(ctx->hChangelogLabel, marginX, 75, 100, 16, TRUE);
    MoveWindow(ctx->hChangelog, marginX, editY, clientW - marginX * 2, editH, TRUE);
    MoveWindow(ctx->hClose, clientW / 2 - btnW / 2, btnY, btnW, btnH, TRUE);

    // 将编辑框内部格式矩形对齐到 lineH 整数倍, 避免底部出现被裁剪的半行
    RECT fmt;
    SendMessageW(ctx->hChangelog, EM_GETRECT, 0, (LPARAM)&fmt);
    int fmtH = fmt.bottom - fmt.top;
    fmt.bottom = fmt.top + (fmtH / ctx->lineH) * ctx->lineH;
    SendMessageW(ctx->hChangelog, EM_SETRECT, 0, (LPARAM)&fmt);
    InvalidateRect(ctx->hChangelog, NULL, TRUE);
}

// MainWindow
class MainWindow {
public:
    MainWindow()
        : m_hwnd(NULL), m_hInst(NULL)
        , m_playlistLV(NULL), m_searchEdit(NULL)
        , m_btnPrev(NULL), m_btnPlay(NULL), m_btnNext(NULL), m_btnMode(NULL), m_btnLocate(NULL)
        , m_btnMute(NULL)
        , m_trackSeek(NULL), m_sliderVol(NULL), m_staticVolPct(NULL)
        , m_lastVol(80)
        , m_staticTime(NULL), m_staticSong(NULL)
        , m_currentIndex(-1), m_userDraggingSeek(false)
        , m_sortColumn(-1), m_sortAscending(true)
        , m_shufflePos(0)
        , m_settingsAutoplay(1), m_settingsRememberProgress(true)
        , m_settingsTray(true), m_trayIconAdded(false), m_taskbarCreatedMsg(0)
        , m_trayReaddAttempts(0), m_trayReaddActive(false)
        , m_balanceEnabled(true)
        , m_ctrlPanel(NULL)
        , m_listening(false), m_listenStartWall(0), m_saveTick(0), m_nextScheduled(-1)
        , m_undoValid(false)
    {
        srand((unsigned)time(NULL));
        InitDefaultHotkeys();
        m_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    }

    bool Create(HINSTANCE hInst, int nCmdShow) {
        m_hInst = hInst;

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = StaticWndProc;
        wc.hInstance     = hInst;
        wc.hIcon         = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON),
                                              IMAGE_ICON,
                                              GetSystemMetrics(SM_CXICON),
                                              GetSystemMetrics(SM_CYICON),
                                              LR_DEFAULTCOLOR);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLASS_NAME;
        wc.hIconSm       = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON),
                                              IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON),
                                              GetSystemMetrics(SM_CYSMICON),
                                              LR_DEFAULTCOLOR);

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

    // 最早有听歌记录的那天 (YYYY-MM-DD); 无记录返回空串. 供"统计范围=所有"使用
    std::string GetHistoryEarliestDate() const { return m_history.GetEarliestDate(); }

private:
    // ---- Controls ----
    HWND m_hwnd;
    HINSTANCE m_hInst;
    HWND m_playlistLV;
    HWND m_searchEdit;
    HWND m_btnPrev, m_btnPlay, m_btnNext, m_btnMode, m_btnLocate;
    HWND m_btnMute;
    HWND m_trackSeek, m_sliderVol, m_staticVolPct;
    HWND m_staticTime, m_staticSong;
    HWND m_ctrlPanel;

    // ---- State ----
    AudioEngine      m_audio;
    PlaylistManager  m_playlist;
    int              m_currentIndex;
    int              m_lastVol;   // volume restored on unmute
    bool             m_userDraggingSeek;
    int              m_sortColumn;
    bool             m_sortAscending;
    std::vector<int> m_filterMap;  // display row → playlist index

    // ---- Shuffle ----
    std::vector<int> m_shuffleOrder;
    int              m_shufflePos;

    // ---- Settings ----
    int m_settingsAutoplay;  // 0=不进行操作, 1=自动播放
    bool m_settingsRememberProgress;
    bool m_settingsTray;
    bool m_trayIconAdded;
    UINT m_taskbarCreatedMsg; // "TaskbarCreated" 注册消息, 用于探测资源管理器重启
    UINT m_trayReaddAttempts; // 资源管理器重启后延迟重加托盘图标的尝试次数
    bool m_trayReaddActive;   // 重试定时器当前是否在运行
    bool m_balanceEnabled;   // 音量平衡

    // ---- Hotkeys ----
    HotkeyBinding m_hotkeys[7];
    int m_hotkeyCount;

    // ---- Menu handles (for nested submenus) ----
    HMENU m_settingsMenu;
    HMENU m_playSubMenu;
    HMENU m_speedSubMenu;
    HMENU m_startupSubMenu;

    // ---- Listening history ----
    ListeningHistory m_history;
    bool m_listening;
    std::chrono::steady_clock::time_point m_listenStart;
    time_t m_listenStartWall;
    int m_saveTick;
    int m_nextScheduled;  // index to play after current song ends, -1 = none

    // ---- Play count tracking ----
    std::map<std::wstring, int> m_playCount;

    // ---- Duration cache & progressive scan ----
    std::map<std::wstring, DurationCacheEntry> m_durationCache;
    bool m_durationCacheLoaded = false;
    std::vector<int> m_pendingDurationScan;  // playlist indexes to scan (unknown durations, or all during full rescan)
    bool m_fullDurationScan = false;         // 全量重扫进行中 (用于防重复执行)

    // ---- Undo remove ----
    SongInfo m_undoSong;
    int m_undoIndex;
    bool m_undoValid;

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
            case WM_MOUSEWHEEL:        OnMouseWheel(wp);             return 0;
            case WM_TIMER:
                if (wp == TIMER_ID_SEEK) OnTimer();
                else if (wp == TIMER_ID_DURATION_SCAN) OnTimerDurationScan();
                else if (wp == TIMER_ID_TRAY_READD) OnTrayReaddTimer();
                else if (wp == TIMER_ID_TRAY_WATCHDOG) OnTrayWatchdog();
                return 0;
            case WM_HOTKEY:            OnGlobalHotkey((int)wp);     return 0;
            case WM_POWERBROADCAST:
                // 合盖休眠时结束当前计时段, 避免休眠期间被计入听歌时长;
                // 唤醒后若音频仍在播放则重新开始计时
                if (wp == PBT_APMSUSPEND) {
                    StopListening();
                    m_history.Save(GetExeDirectory() + L"\\.history.txt");
                } else if (wp == PBT_APMRESUMESUSPEND || wp == PBT_APMRESUMEAUTOMATIC) {
                    if (m_audio.IsPlaying())
                        StartListening();
                }
                return TRUE;
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
                if (msg == WM_APP_FADE_DONE) { OnFadeDone(); return 0; }
                if (msg == WM_APP_BRING_TO_TOP) {
                    SetForegroundWindow(m_hwnd);
                    return 0;
                }
                if (msg == m_taskbarCreatedMsg && m_taskbarCreatedMsg != 0) {
                    // 资源管理器重启后托盘图标被系统清除。此时通知区往往尚未就绪,
                    // 立即 NIM_ADD 容易失败或图标随后被再次清除, 故改为延迟重试。
                    m_trayIconAdded = false;
                    // 同一时间点的多次 TaskbarCreated 广播只启动一次重试定时器,
                    // 避免反复重置定时器导致重加被无限推迟。
                    if (!m_trayReaddActive) {
                        m_trayReaddActive = true;
                        m_trayReaddAttempts = 0;
                        WriteLog(L"TaskbarCreated 消息已收到, 开始延迟重加托盘图标");
                        SetTimer(m_hwnd, TIMER_ID_TRAY_READD, 1500, NULL);
                    }
                    return 0;
                }
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
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_DATE_CLASSES };
        InitCommonControlsEx(&icc);
        CreateMenuBar();
        CreateControls();

        RegisterStatsWindowClass();

        if (!m_audio.Initialize(m_hwnd)) {
            WriteLog(L"初始化音频引擎失败: %ls", m_audio.GetErrorMessage().c_str());
            MessageBoxW(m_hwnd,
                (L"无法初始化音频引擎 (bass.dll)。\n\n"
                 L"请确保 bass.dll 位于程序目录或系统路径中。\n"
                 L"下载地址: https://www.un4seen.com/bass.html\n\n"
                 L"错误详情: " + m_audio.GetErrorMessage()).c_str(),
                L"音频初始化失败", MB_OK | MB_ICONWARNING);
        }

        LoadPlayCount();
        LoadSettings();
        // 同步音量平衡开关: 老版本 .settings.txt 无此字段时, 以 MainWindow 默认值 (开启) 为准
        m_audio.SetBalanceEnabled(m_balanceEnabled);
        UpdateSettingsMenu();
        UpdateModeUI();
        UpdateSpeedMenu();
        UpdateUndoMenuState();

        if (!LoadPlaylist()) {
            WriteLog(L"播放列表文件不存在或为空，尝试加载上次打开的文件夹");
            LoadFromLastFolder();
        }

        LoadVolume();
        m_history.Load(GetExeDirectory() + L"\\.history.txt");
        m_audio.SetNotifyWindow(m_hwnd, WM_USER_SONG_END);
        m_audio.SetFadeNotify(m_hwnd, WM_APP_FADE_DONE);

        bool startPlay = (m_settingsAutoplay == 1);
        if (m_settingsRememberProgress && !m_playlist.IsEmpty()) {
            if (LoadLastSong()) {
                if (m_audio.IsBalanceEnabled()) {
                    m_audio.ApplyBalance();
                }
                // Stream loaded and seeked, now start playback if configured.
                if (startPlay) {
                    m_audio.Play();
                    StartListening();
                    if (m_currentIndex >= 0) {
                        const std::wstring& path = m_playlist.GetFile(m_currentIndex);
                        std::wstring meta = m_audio.GetFormattedMetadata();
                        std::wstring text;
                        if (!meta.empty())
                            text = L"正在播放: " + meta;
                        else
                            text = L"正在播放: " + GetDisplayName(path);
                        double speed = m_audio.GetSpeed();
                        if (speed != 1.0) {
                            wchar_t sb[16];
                            swprintf(sb, 16, L" (%.2gx)", speed);
                            text += sb;
                        }
                        SetWindowTextW(m_staticSong, text.c_str());
                    }
                    UpdateUI();
                    UpdatePlaylistSelection();
                    UpdateTrayTip();
                    SetTimer(m_hwnd, TIMER_ID_SEEK, 500, NULL);
                    m_saveTick = 0;
                } else {
                    UpdateUI();
                    UpdatePlaylistSelection();
                    SetWindowTextW(m_staticSong, L"已暂停");
                    UpdateTrayTip();
                }
            } else if (startPlay) {
                PlayFile(0);
            }
        } else if (startPlay) {
            PlayFile(0);
        }

        RegisterHotKeys();
        AddTrayIcon();
        SetTimer(m_hwnd, TIMER_ID_TRAY_WATCHDOG, 5000, NULL);  // 周期性心跳, 兜底恢复托盘图标
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
        SavePlayCount();
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
        AppendMenuW(fileMenu, MF_STRING | MF_GRAYED, ID_UNDO_REMOVE, L"撤销移除(&U)");
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_EXIT, L"退出(&X)");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)fileMenu, L"文件(&F)");

        m_settingsMenu = CreatePopupMenu();
        m_startupSubMenu = CreatePopupMenu();
        AppendMenuW(m_startupSubMenu, MF_STRING, ID_STARTUP_NOTHING, L"不进行操作");
        AppendMenuW(m_startupSubMenu, MF_STRING | MF_CHECKED, ID_STARTUP_AUTOPLAY, L"自动播放");
        AppendMenuW(m_settingsMenu, MF_POPUP, (UINT_PTR)m_startupSubMenu, L"启动后...");
        AppendMenuW(m_settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_REMEMBER,
            L"记住播放进度");
        AppendMenuW(m_settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_TRAY,
            L"最小化到托盘");
        AppendMenuW(m_settingsMenu, MF_STRING | MF_CHECKED, ID_SETTINGS_BALANCE,
            L"音量平衡(&B)");
        AppendMenuW(m_settingsMenu, MF_SEPARATOR, 0, NULL);

        m_playSubMenu = CreatePopupMenu();
        AppendMenuW(m_playSubMenu, MF_STRING | MF_CHECKED, ID_PLAY_SEQUENTIAL, L"顺序播放(&S)");
        AppendMenuW(m_playSubMenu, MF_STRING, ID_PLAY_REPEATONE, L"单曲循环(&R)");
        AppendMenuW(m_playSubMenu, MF_STRING, ID_PLAY_SHUFFLE, L"随机播放(&H)");
        AppendMenuW(m_settingsMenu, MF_POPUP, (UINT_PTR)m_playSubMenu, L"播放模式");

        // 倍速 submenu inside 设置
        m_speedSubMenu = CreatePopupMenu();
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_025, L"0.25x");
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_050, L"0.5x");
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_075, L"0.75x");
        AppendMenuW(m_speedSubMenu, MF_STRING | MF_CHECKED, ID_SPEED_100, L"1x");
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_125, L"1.25x");
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_150, L"1.5x");
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_200, L"2x");
        AppendMenuW(m_speedSubMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m_speedSubMenu, MF_STRING, ID_SPEED_CUSTOM, L"自定义...");
        AppendMenuW(m_settingsMenu, MF_POPUP, (UINT_PTR)m_speedSubMenu, L"倍速");

        AppendMenuW(m_settingsMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_HOTKEYS,
            L"配置快捷键...");
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_STATS, L"统计");
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_RESCAN_DURATIONS,
            L"重新统计歌曲时长");
        AppendMenuW(m_settingsMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_ABOUT, L"关于...");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)m_settingsMenu, L"设置(&S)");

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
        m_searchEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_LEFT,
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

        m_btnLocate = CreateWindowExW(0, L"BUTTON", L"📍",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_LOCATE, m_hInst, NULL);

        m_btnMute = CreateWindowExW(0, L"BUTTON", L"🔊",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_MUTE, m_hInst, NULL);

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

        HWND ctls[] = { m_btnMode, m_btnPrev, m_btnPlay, m_btnNext, m_btnLocate, m_btnMute,
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
        const int searchTop = 14;   // 搜索框距菜单栏的顶部间距
        const int ctrlPanelH = 96;  // bottom panel height: controls + padding
        const int lvY = searchTop + searchH + M;
        int panelY = h - ctrlPanelH;
        int listH = panelY - lvY - M;  // M gap between listview and panel
        if (listH < 30) listH = 30;

        // Search label + edit at top
        HWND hSearchLabel = FindWindowExW(m_hwnd, NULL, L"STATIC", L"搜索:");
        if (hSearchLabel) {
            SetWindowPos(hSearchLabel, NULL, M, searchTop, 40, searchH, SWP_NOZORDER);
            SendMessageW(hSearchLabel, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        }
        if (m_searchEdit) {
            SetWindowPos(m_searchEdit, NULL, M + 42, searchTop, w - 2 * M - 42, searchH, SWP_NOZORDER);
            SendMessageW(m_searchEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        }

        // Listview
        SetWindowPos(m_playlistLV, NULL, M, lvY, w - 2 * M, listH, SWP_NOZORDER);

        // 标题列(索引 1)自适应填满剩余宽度, # / 专辑 / 时长保持固定
        {
            RECT lvrc;
            GetClientRect(m_playlistLV, &lvrc);
            int titleW = (lvrc.right - lvrc.left) - (COL_WIDTHS[0] + COL_WIDTHS[2] + COL_WIDTHS[3]);
            if (titleW < 120) titleW = 120;
            ListView_SetColumnWidth(m_playlistLV, 1, titleW);
        }

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
        bx += 42;
        SetWindowPos(m_btnLocate, NULL, bx, y + 4, 36, BH, SWP_NOZORDER);

        int muteW = 26;
        int volPctW = 36;
        int volW = 130;
        int volX = w - M - volPctW - volW - muteW - 4;
        SetWindowPos(m_btnMute, NULL, volX, y + 4, muteW, BH, SWP_NOZORDER);
        SetWindowPos(m_staticVolPct, NULL, volX + muteW + 4, y + 6, volPctW, 20, SWP_NOZORDER);
        SetWindowPos(m_sliderVol, NULL, volX + muteW + 4 + volPctW, y + 4, volW, BH, SWP_NOZORDER);

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
                case ID_UNDO_REMOVE:          UndoRemove();     break;
                case ID_FILE_EXIT:            OnRealClose(); break;
                case ID_STARTUP_NOTHING:
                    m_settingsAutoplay = 0;
                    UpdateSettingsMenu();
                    SaveSettings();
                    break;
                case ID_STARTUP_AUTOPLAY:
                    m_settingsAutoplay = 1;
                    UpdateSettingsMenu();
                    SaveSettings();
                    break;
                case ID_SETTINGS_REMEMBER:
                    m_settingsRememberProgress = !m_settingsRememberProgress;
                    UpdateSettingsMenu();
                    break;
                case ID_SETTINGS_TRAY:
                    m_settingsTray = !m_settingsTray;
                    UpdateSettingsMenu();
                    break;
                case ID_SETTINGS_BALANCE:
                    m_balanceEnabled = !m_balanceEnabled;
                    m_audio.SetBalanceEnabled(m_balanceEnabled);
                    UpdateSettingsMenu();
                    SaveSettings();
                    break;
                case ID_SETTINGS_HOTKEYS:
                    ShowHotkeyDialog();
                    break;
                case ID_SETTINGS_STATS:
                    ShowStatsWindow();
                    break;
                case ID_SETTINGS_RESCAN_DURATIONS:
                    OnRescanDurations();
                    break;
                case ID_SETTINGS_ABOUT:
                    ShowAboutWindow();
                    break;
                case ID_PLAY_SEQUENTIAL: SetPlayMode(PlayMode::Sequential); break;
                case ID_PLAY_REPEATONE:  SetPlayMode(PlayMode::RepeatOne);  break;
                case ID_PLAY_SHUFFLE:
                    SetPlayMode(PlayMode::Shuffle);
                    Reshuffle();
                    break;
                case ID_SPEED_025: ApplySpeed(0.25); break;
                case ID_SPEED_050: ApplySpeed(0.5);  break;
                case ID_SPEED_075: ApplySpeed(0.75); break;
                case ID_SPEED_100: ApplySpeed(1.0);  break;
                case ID_SPEED_125: ApplySpeed(1.25); break;
                case ID_SPEED_150: ApplySpeed(1.5);  break;
                case ID_SPEED_200: ApplySpeed(2.0);  break;
                case ID_SPEED_CUSTOM:
                    ShowSpeedInputDialog();
                    break;
                case ID_TRAY_PLAYPAUSE: OnPlayPause(); break;
                case ID_TRAY_PREV:      OnPrev();      break;
                case ID_TRAY_NEXT:      OnNext();      break;
                case ID_TRAY_RESTORE:
                    ShowWindow(m_hwnd, SW_RESTORE);
                    SetForegroundWindow(m_hwnd);
                    break;
                case ID_TRAY_MINIMIZE:
                    MinimizeToTray();
                    break;
                case ID_TRAY_EXIT:
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
                else if (hCtrl == m_btnLocate) LocateCurrentSong();
                else if (hCtrl == m_btnMute) ToggleMute();
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

    // WM_MOUSEWHEEL
    void OnMouseWheel(WPARAM wp) {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int step = (delta > 0) ? 5 : -5;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) step *= 2;
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0) + step;
        vol = (vol < 0) ? 0 : (vol > 100 ? 100 : vol);
        m_audio.SetVolume(vol);
        SendMessageW(m_sliderVol, TBM_SETPOS, TRUE, vol);
        UpdateVolLabel();
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
                case NM_RCLICK: {
                    LPNMITEMACTIVATE ia = (LPNMITEMACTIVATE)lp;
                    int row = ia->iItem;
                    if (row < 0 || row >= (int)m_filterMap.size()) return 0;
                    int songIdx = m_filterMap[row];

                    ListView_SetItemState(m_playlistLV, row,
                        LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

                    POINT pt;
                    GetCursorPos(&pt);

                    const std::wstring& path = m_playlist.GetFile(songIdx);

                    HMENU hMenu = CreatePopupMenu();
                    HMENU hSpeedMenu = CreatePopupMenu();

                    AppendMenuW(hMenu, MF_STRING, 3101, L"播放(&P)");
                    AppendMenuW(hMenu, MF_STRING, 3102, L"下一首播放(&N)");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

                    // Speed submenu
                    double curSpeed = m_audio.GetSpeed();
                    struct { int id; const wchar_t* label; double val; } speedItems[] = {
                        { ID_SPEED_025, L"0.25x", 0.25 },
                        { ID_SPEED_050, L"0.5x", 0.5 },
                        { ID_SPEED_075, L"0.75x", 0.75 },
                        { ID_SPEED_100, L"1x", 1.0 },
                        { ID_SPEED_125, L"1.25x", 1.25 },
                        { ID_SPEED_150, L"1.5x", 1.5 },
                        { ID_SPEED_200, L"2x", 2.0 },
                    };
                    for (auto& si : speedItems) {
                        UINT flags = MF_STRING;
                        if (curSpeed == si.val) flags |= MF_CHECKED;
                        AppendMenuW(hSpeedMenu, flags, si.id, si.label);
                    }
                    AppendMenuW(hSpeedMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuW(hSpeedMenu, MF_STRING, ID_SPEED_CUSTOM, L"自定义...");
                    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSpeedMenu, L"倍速播放");

                    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuW(hMenu, MF_STRING, 3103, L"在文件管理器中定位(&L)");
                    AppendMenuW(hMenu, MF_STRING, 3104, L"从列表中移除(&R)");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuW(hMenu, MF_STRING, 3105, L"属性(&T)");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuW(hMenu, MF_STRING | (m_undoValid ? MF_ENABLED : MF_GRAYED),
                        ID_UNDO_REMOVE, L"撤销移除(&U)");

                    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, m_hwnd, NULL);
                    DestroyMenu(hMenu);

                    if (cmd > 0) {
                        switch (cmd) {
                            case 3101: PlayFile(songIdx); break;
                            case 3102:
                                if (!m_audio.IsLoaded() || !m_audio.IsPlaying())
                                    PlayFile(songIdx);
                                else
                                    m_nextScheduled = songIdx;
                                break;
                            case 3103: {
                                std::wstring param = L"/select,\"" + path + L"\"";
                                ShellExecuteW(NULL, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
                                break;
                            }
                            case 3104: {
                                if (m_nextScheduled == songIdx)
                                    m_nextScheduled = -1;
                                else if (m_nextScheduled > songIdx)
                                    m_nextScheduled--;

                                if (m_currentIndex == songIdx) {
                                    KillTimer(m_hwnd, TIMER_ID_SEEK);
                                    m_audio.Unload();
                                    m_currentIndex = -1;
                                    StopListening();
                                } else if (m_currentIndex > songIdx) {
                                    m_currentIndex--;
                                }
                                // Save undo info before removal
                                m_undoSong = m_playlist.GetSong(songIdx);
                                m_undoIndex = songIdx;
                                m_undoValid = true;
                                m_playlist.RemoveAt(songIdx);
                                if (m_audio.GetPlayMode() == PlayMode::Shuffle || !m_shuffleOrder.empty())
                                    Reshuffle();
                                RebuildFilter();
                                RefreshPlaylistUI();
                                UpdatePlaylistSelection();
                                UpdateUI();
                                UpdateUndoMenuState();
                                break;
                            }
                            case 3105: {
                                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                                sei.lpVerb = L"properties";
                                sei.lpFile = path.c_str();
                                sei.nShow = SW_SHOW;
                                sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_NOCLOSEPROCESS;
                                ShellExecuteExW(&sei);
                                break;
                            }
                            case ID_UNDO_REMOVE: UndoRemove(); break;
                            default:
                                if (cmd == ID_SPEED_CUSTOM)
                                    ShowSpeedInputDialog();
                                else if (cmd >= ID_SPEED_025 && cmd <= ID_SPEED_200) {
                                    double speeds[] = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
                                    int speedIds[] = {ID_SPEED_025, ID_SPEED_050, ID_SPEED_075, ID_SPEED_100, ID_SPEED_125, ID_SPEED_150, ID_SPEED_200};
                                    double chosenSpeed = 1.0;
                                    for (int i = 0; i < 7; i++) {
                                        if (cmd == speedIds[i]) {
                                            chosenSpeed = speeds[i];
                                            break;
                                        }
                                    }
                                    PlayFile(songIdx);
                                    ApplySpeed(chosenSpeed);
                                }
                                break;
                        }
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
            if (++m_saveTick >= 20) {
                m_saveTick = 0;
                if (m_settingsRememberProgress)
                    SaveLastSong();
                // Periodically flush session time to disk so forced shutdown
                // loses at most one interval (~10 s) of listening history
                if (m_listening) {
                    StopListening();
                    StartListening();
                }
                m_history.Save(GetExeDirectory() + L"\\.history.txt");
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

        // Override with next scheduled if set
        if (m_nextScheduled >= 0 && m_nextScheduled < count) {
            next = m_nextScheduled;
            m_nextScheduled = -1;
        }

        if (next >= 0 && next < count) {
            PlayFile(next);
        } else {
            StopListening();  // 播放完毕，关闭计时，避免空闲时间被计入
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
            StopListening();  // 卸载当前歌曲后停止计时，避免扫描新文件夹期间被计入
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
        if (m_audio.IsPlaying() && !m_audio.IsFading()) {
            m_audio.PauseFade(500);
            SetWindowTextW(m_staticSong, L"已暂停");
            UpdateTrayTip();
        } else {
            m_audio.PlayFade();
            StartListening();
            if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
                std::wstring meta = m_audio.GetFormattedMetadata();
                const auto& path = m_playlist.GetFile(m_currentIndex);
                std::wstring text;
                if (!meta.empty())
                    text = L"正在播放: " + meta;
                else
                    text = L"正在播放: " + GetDisplayName(path);
                double speed = m_audio.GetSpeed();
                if (speed != 1.0) {
                    wchar_t sb[16];
                    swprintf(sb, 16, L" (%.2gx)", speed);
                    text += sb;
                }
                SetWindowTextW(m_staticSong, text.c_str());
                UpdateTrayTip();
            }
            SetTimer(m_hwnd, TIMER_ID_SEEK, 500, NULL);
        }
        UpdateUI();
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

    void OnFadeDone() {
        m_audio.OnFadeComplete();
        StopListening();
        UpdateUI();
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

    void UpdateSpeedMenu() {
        double s = m_audio.GetSpeed();
        int checkId = ID_SPEED_CUSTOM;
        if (s == 0.25) checkId = ID_SPEED_025;
        else if (s == 0.5)  checkId = ID_SPEED_050;
        else if (s == 0.75) checkId = ID_SPEED_075;
        else if (s == 1.0)  checkId = ID_SPEED_100;
        else if (s == 1.25) checkId = ID_SPEED_125;
        else if (s == 1.5)  checkId = ID_SPEED_150;
        else if (s == 2.0)  checkId = ID_SPEED_200;

        int ids[] = { ID_SPEED_025, ID_SPEED_050, ID_SPEED_075, ID_SPEED_100,
                      ID_SPEED_125, ID_SPEED_150, ID_SPEED_200 };
        for (int id : ids)
            CheckMenuItem(m_speedSubMenu, id, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(m_speedSubMenu, checkId, MF_BYCOMMAND | MF_CHECKED);
    }

    void ApplySpeed(double speed) {
        m_audio.SetSpeed(speed);
        UpdateSpeedMenu();
        // Update status bar if currently playing
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
            const std::wstring& path = m_playlist.GetFile(m_currentIndex);
            std::wstring meta = m_audio.GetFormattedMetadata();
            std::wstring text;
            if (!meta.empty())
                text = L"正在播放: " + meta;
            else
                text = L"正在播放: " + GetDisplayName(path);
            if (speed != 1.0) {
                wchar_t speedBuf[16];
                swprintf(speedBuf, 16, L" (%.2gx)", speed);
                text += speedBuf;
            }
            SetWindowTextW(m_staticSong, text.c_str());
        }
        SaveSettings();
    }

    void UpdateUndoMenuState() {
        EnableMenuItem(GetMenu(m_hwnd), ID_UNDO_REMOVE,
            MF_BYCOMMAND | (m_undoValid ? MF_ENABLED : MF_GRAYED));
    }

    void UndoRemove() {
        if (!m_undoValid) return;
        m_playlist.InsertAt(m_undoIndex, m_undoSong);
        if (m_currentIndex >= m_undoIndex)
            m_currentIndex++;
        if (m_nextScheduled >= m_undoIndex)
            m_nextScheduled++;
        if (m_audio.GetPlayMode() == PlayMode::Shuffle || !m_shuffleOrder.empty())
            Reshuffle();
        m_undoValid = false;
        RebuildFilter();
        RefreshPlaylistUI();
        UpdatePlaylistSelection();
        UpdateUndoMenuState();
    }

    void UpdateSettingsMenu() {
        CheckMenuItem(m_startupSubMenu, ID_STARTUP_NOTHING,
            MF_BYCOMMAND | (m_settingsAutoplay == 0 ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_startupSubMenu, ID_STARTUP_AUTOPLAY,
            MF_BYCOMMAND | (m_settingsAutoplay == 1 ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_REMEMBER,
            MF_BYCOMMAND | (m_settingsRememberProgress ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_TRAY,
            MF_BYCOMMAND | (m_settingsTray ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_BALANCE,
            MF_BYCOMMAND | (m_balanceEnabled ? MF_CHECKED : MF_UNCHECKED));
    }


    // Speed input dialog

    void ShowSpeedInputDialog() {
        const wchar_t DLG_CLASS[] = L"SpeedInputDlg";
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = SpeedInputDlgProc;
        wc.hInstance     = m_hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = DLG_CLASS;
        if (!RegisterClassExW(&wc)) return;

        int dlgW = 280, dlgH = 140;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

        HWND hDlg = CreateWindowExW(0, DLG_CLASS, L"自定义倍速",
            WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            x, y, dlgW, dlgH, m_hwnd, NULL, m_hInst, NULL);
        if (!hDlg) return;

        struct SpeedCtx { MainWindow* win; double result; };
        SpeedCtx* ctx = new SpeedCtx{ this, -1.0 };
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

        CreateWindowExW(0, L"STATIC", L"输入倍速 (0.1 ~ 10.0):",
            WS_CHILD | WS_VISIBLE, 15, 15, 250, 20, hDlg, NULL, m_hInst, NULL);
        wchar_t cur[16];
        swprintf(cur, 16, L"%.2g", m_audio.GetSpeed());
        CreateWindowExW(0, L"EDIT", cur,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
            15, 40, 250, 26, hDlg, (HMENU)500, m_hInst, NULL);
        SendMessageW(GetDlgItem(hDlg, 500), EM_SETLIMITTEXT, 6, 0);

        CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            dlgW / 2 - 95, 80, 80, 28, hDlg, (HMENU)IDOK, m_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            dlgW / 2 + 15, 80, 80, 28, hDlg, (HMENU)IDCANCEL, m_hInst, NULL);

        EnableWindow(m_hwnd, FALSE);
        MSG msg;
        while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        EnableWindow(m_hwnd, TRUE);
        SetForegroundWindow(m_hwnd);

        if (ctx->result > 0) {
            ApplySpeed(ctx->result);
        }
        delete ctx;
        UnregisterClassW(DLG_CLASS, m_hInst);
    }


    // Search / Filter
    
    bool SearchMatches(int playlistIdx) {
        const auto& song = m_playlist.GetSong(playlistIdx);
        if (!m_searchEdit) return true;
        wchar_t searchBuf[256] = {};
        GetWindowTextW(m_searchEdit, searchBuf, 256);
        return SearchMatchesText(searchBuf, song.title, song.artist, song.album);
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
            case HKID_RESTORE:   ShowWindow(m_hwnd, SW_RESTORE); SetForegroundWindow(m_hwnd); break;
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
                    ReleaseCapture();
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
        int dlgW = 660, dlgH = 620;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

        HWND hDlg = CreateWindowExW(0, STATS_CLASS, L"听歌时长统计",
            WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_VISIBLE,
            x, y, dlgW, dlgH, m_hwnd, NULL, m_hInst, NULL);
        if (!hDlg) return;

        StatsDlgCtx* ctx = new StatsDlgCtx();
        ctx->win = this;
        ctx->rangeDays = 30;
        ctx->useCalendarRange = false;
        ctx->useAllRange = false;
        ctx->dtpGuard = false;
        memset(&ctx->calStart, 0, sizeof(SYSTEMTIME));
        memset(&ctx->calEnd, 0, sizeof(SYSTEMTIME));
        ctx->hRadio7 = NULL;
        ctx->hRadio30 = NULL;
        ctx->hRadioAll = NULL;
        ctx->hRadioCustom = NULL;
        ctx->hDtpStart = NULL;
        ctx->hDtpEnd = NULL;
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
        int rY = yPos + 18;

        // --- Group box: 统计范围 ---
        CreateWindowExW(0, L"BUTTON", L"统计范围",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, yPos, dlgW - 20, 50, hDlg, NULL, m_hInst, NULL);

        ctx->hRadioAll = CreateWindowExW(0, L"BUTTON", L"所有",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            25, rY, 55, 22, hDlg, (HMENU)303, m_hInst, NULL);
        SendMessageW(ctx->hRadioAll, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        ctx->hRadio7 = CreateWindowExW(0, L"BUTTON", L"7天",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            85, rY, 55, 22, hDlg, (HMENU)300, m_hInst, NULL);
        SendMessageW(ctx->hRadio7, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        ctx->hRadio30 = CreateWindowExW(0, L"BUTTON", L"30天",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            145, rY, 60, 22, hDlg, (HMENU)301, m_hInst, NULL);
        SendMessageW(ctx->hRadio30, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        SendMessageW(ctx->hRadio30, BM_SETCHECK, BST_CHECKED, 0);

        ctx->hRadioCustom = CreateWindowExW(0, L"BUTTON", L"自定义",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            210, rY, 65, 22, hDlg, (HMENU)302, m_hInst, NULL);
        SendMessageW(ctx->hRadioCustom, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        // --- DTP row + buttons ---
        yPos += 62;
        int dtpY = yPos;

        CreateWindowExW(0, L"STATIC", L"起始:",
            WS_CHILD | WS_VISIBLE, 15, dtpY + 3, 35, 20, hDlg, (HMENU)312, m_hInst, NULL);
        ctx->hDtpStart = CreateWindowExW(0, DATETIMEPICK_CLASSW, NULL,
            WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
            50, dtpY, 130, 26, hDlg, (HMENU)310, m_hInst, NULL);
        SendMessageW(ctx->hDtpStart, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        CreateWindowExW(0, L"STATIC", L"终止:",
            WS_CHILD | WS_VISIBLE, 200, dtpY + 3, 35, 20, hDlg, (HMENU)313, m_hInst, NULL);
        ctx->hDtpEnd = CreateWindowExW(0, DATETIMEPICK_CLASSW, NULL,
            WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
            235, dtpY, 130, 26, hDlg, (HMENU)311, m_hInst, NULL);
        SendMessageW(ctx->hDtpEnd, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        CreateWindowExW(0, L"BUTTON", L"刷新",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            dlgW - 100, dtpY - 2, 80, 28, hDlg, (HMENU)304, m_hInst, NULL);
        SendMessageW(GetDlgItem(hDlg, 304), WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        SYSTEMTIME todayDtp;
        GetLocalTime(&todayDtp);
        SendMessageW(ctx->hDtpStart, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&todayDtp);
        SendMessageW(ctx->hDtpEnd, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&todayDtp);
        ctx->calStart = todayDtp;
        ctx->calEnd = todayDtp;

        yPos = dtpY + 34;

        ctx->hLabelDay = CreateWindowExW(0, L"STATIC", L"每日详情",
            WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, m_hInst, NULL);

        yPos += 20;
        ctx->hDayList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
            15, yPos, dlgW - 30, 130, hDlg, NULL, m_hInst, NULL);
        ListView_SetExtendedListViewStyle(ctx->hDayList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

        LVCOLUMNW lc = {};
        lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lc.fmt  = LVCFMT_LEFT;
        lc.cx = 120; lc.pszText = (LPWSTR)L"日期"; ListView_InsertColumn(ctx->hDayList, 0, &lc);
        lc.cx = 70;  lc.pszText = (LPWSTR)L"星期"; ListView_InsertColumn(ctx->hDayList, 1, &lc);
        lc.cx = 150; lc.pszText = (LPWSTR)L"听歌时长"; ListView_InsertColumn(ctx->hDayList, 2, &lc);

        yPos += 138;

        ctx->hLabelWeek = CreateWindowExW(0, L"STATIC", L"每周统计",
            WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, m_hInst, NULL);

        yPos += 20;
        ctx->hWeekList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
            15, yPos, dlgW - 30, 70, hDlg, NULL, m_hInst, NULL);
        ListView_SetExtendedListViewStyle(ctx->hWeekList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

        lc.cx = 100; lc.pszText = (LPWSTR)L"周"; ListView_InsertColumn(ctx->hWeekList, 0, &lc);
        lc.cx = 140; lc.pszText = (LPWSTR)L"日期范围"; ListView_InsertColumn(ctx->hWeekList, 1, &lc);
        lc.cx = 150; lc.pszText = (LPWSTR)L"累计时长"; ListView_InsertColumn(ctx->hWeekList, 2, &lc);

        yPos += 78;

        ctx->hLabelPlay = CreateWindowExW(0, L"STATIC", L"我常听的",
            WS_CHILD | WS_VISIBLE, 15, yPos, 180, 20, hDlg, NULL, m_hInst, NULL);

        yPos += 20;
        ctx->hPlayCountList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
            15, yPos, dlgW - 30, 90, hDlg, NULL, m_hInst, NULL);
        ListView_SetExtendedListViewStyle(ctx->hPlayCountList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

        lc.cx = 40;  lc.pszText = (LPWSTR)L"#";   ListView_InsertColumn(ctx->hPlayCountList, 0, &lc);
        lc.cx = 250; lc.pszText = (LPWSTR)L"歌曲"; ListView_InsertColumn(ctx->hPlayCountList, 1, &lc);
        lc.cx = 80;  lc.pszText = (LPWSTR)L"次数"; ListView_InsertColumn(ctx->hPlayCountList, 2, &lc);

        yPos += 98;

        ctx->hTotalText = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            15, yPos, dlgW - 30, 22, hDlg, (HMENU)400, m_hInst, NULL);
        SendMessageW(ctx->hTotalText, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

        yPos += 28;
        CreateWindowExW(0, L"BUTTON", L"导出数据...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            dlgW / 2 - 120, yPos, 120, 28, hDlg, (HMENU)305, m_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"关闭",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            dlgW / 2 + 20, yPos, 90, 28, hDlg, (HMENU)IDCANCEL, m_hInst, NULL);

        RefreshStatsDisplay(ctx);

        // Save base layout values for proportional resize
        {
            RECT cr;
            GetClientRect(hDlg, &cr);
            ctx->baseW = cr.right;
            ctx->baseH = cr.bottom;
            int yp = 15;
            yp += 62;
            yp += 34;
            yp += 20;
            ctx->yDayList = yp;
            ctx->hDay = 130;
            yp += 138;
            yp += 20;
            ctx->yWeekList = yp;
            ctx->hWeek = 70;
            yp += 78;
            yp += 20;
            ctx->yPlayList = yp;
            ctx->hPlay = 90;
            yp += 98;
            ctx->yTotal = yp;
            yp += 28;
            ctx->yButtons = yp;
        }

        // Force initial layout pass: WM_SIZE during CreateWindowEx fired
        // before baseH was set, so LayoutStatsControls skipped.
        {
            RECT cr;
            GetClientRect(hDlg, &cr);
            LayoutStatsControls(ctx, cr.right, cr.bottom);
        }

        DeleteObject(hBoldFont);
        DeleteObject(hGuiFont);
    }

    // 统计导出：选择窗口 (格式 + 数据项) → 另存为 → 写入
    void ShowExportStatsDialog(StatsDlgCtx* statsCtx) {
        const wchar_t DLG_CLASS[] = L"ExportStatsDlg";
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = ExportStatsDlgProc;
        wc.hInstance     = m_hInst;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = DLG_CLASS;
        if (!RegisterClassExW(&wc)) return;

        int dlgW = 380, dlgH = 300;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

        HWND hDlg = CreateWindowExW(0, DLG_CLASS, L"导出统计数据",
            WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            x, y, dlgW, dlgH, statsCtx->hDlg, NULL, m_hInst, NULL);
        if (!hDlg) { UnregisterClassW(DLG_CLASS, m_hInst); return; }

        ExportCtx* ctx = new ExportCtx();
        ctx->hDlg = hDlg;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

        // 导出格式
        CreateWindowExW(0, L"BUTTON", L"导出格式",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            15, 12, dlgW - 30, 52, hDlg, NULL, m_hInst, NULL);
        ctx->hRadioCsv = CreateWindowExW(0, L"BUTTON", L"CSV (Excel)",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
            30, 32, 130, 22, hDlg, (HMENU)600, m_hInst, NULL);
        ctx->hRadioJson = CreateWindowExW(0, L"BUTTON", L"JSON",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP,
            170, 32, 100, 22, hDlg, (HMENU)601, m_hInst, NULL);
        SendMessageW(ctx->hRadioCsv, BM_SETCHECK, BST_CHECKED, 0);

        // 导出内容
        CreateWindowExW(0, L"BUTTON", L"导出内容",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            15, 72, dlgW - 30, 140, hDlg, NULL, m_hInst, NULL);
        ctx->hChkDaily = CreateWindowExW(0, L"BUTTON", L"每日听歌记录 (日期/星期/时长)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            30, 92, dlgW - 60, 22, hDlg, (HMENU)602, m_hInst, NULL);
        ctx->hChkWeekly = CreateWindowExW(0, L"BUTTON", L"每周统计 (周次/日期范围/时长)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            30, 118, dlgW - 60, 22, hDlg, (HMENU)603, m_hInst, NULL);
        ctx->hChkTop = CreateWindowExW(0, L"BUTTON", L"常听歌曲排行 (前 20)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            30, 144, dlgW - 60, 22, hDlg, (HMENU)604, m_hInst, NULL);
        ctx->hChkTotal = CreateWindowExW(0, L"BUTTON", L"总计",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            30, 170, dlgW - 60, 22, hDlg, (HMENU)605, m_hInst, NULL);
        SendMessageW(ctx->hChkDaily, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(ctx->hChkWeekly, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(ctx->hChkTop, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(ctx->hChkTotal, BM_SETCHECK, BST_CHECKED, 0);

        // 按钮
        CreateWindowExW(0, L"BUTTON", L"导出",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            dlgW / 2 - 115, 224, 90, 30, hDlg, (HMENU)IDOK, m_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            dlgW / 2 + 25, 224, 90, 30, hDlg, (HMENU)IDCANCEL, m_hInst, NULL);

        EnableWindow(m_hwnd, FALSE);
        EnableWindow(statsCtx->hDlg, FALSE);
        MSG msg;
        while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        EnableWindow(statsCtx->hDlg, TRUE);
        EnableWindow(m_hwnd, TRUE);
        SetForegroundWindow(statsCtx->hDlg);

        if (ctx->result == 1) {
            DoExportStats(statsCtx, ctx->asCsv,
                ctx->includeDaily, ctx->includeWeekly, ctx->includeTopSongs, ctx->includeTotal);
        }
        delete ctx;
        UnregisterClassW(DLG_CLASS, m_hInst);
    }

    // 按所选格式与勾选项导出统计数据到用户指定文件
    void DoExportStats(StatsDlgCtx* statsCtx, bool asCsv,
                       bool includeDaily, bool includeWeekly,
                       bool includeTopSongs, bool includeTotal) {
        std::string fromDate, toDate;
        ComputeStatsRange(statsCtx, fromDate, toDate);

        StatsExportData data;
        data.dailyRecords = m_history.GetDailyRecords(fromDate, toDate);
        data.weeklyRecords = m_history.GetWeeklySummaries(fromDate, toDate);
        data.totalSeconds = m_history.GetTotalSeconds(fromDate, toDate);

        std::vector<std::pair<std::wstring, int>> sorted(m_playCount.begin(), m_playCount.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        int limit = (int)sorted.size() < 20 ? (int)sorted.size() : 20;
        for (int i = 0; i < limit; i++) {
            data.topSongs.push_back({ GetDisplayName(sorted[i].first), sorted[i].second });
        }

        ExportOptions opt;
        opt.asCsv = asCsv;
        opt.includeDaily = includeDaily;
        opt.includeWeekly = includeWeekly;
        opt.includeTopSongs = includeTopSongs;
        opt.includeTotal = includeTotal;
        std::string content = asCsv ? data.ToCsv(opt) : data.ToJson(opt);

        wchar_t filePath[MAX_PATH] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = statsCtx->hDlg;
        ofn.lpstrFile    = filePath;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = L"CSV 文件 (*.csv)\0*.csv\0JSON 文件 (*.json)\0*.json\0所有文件 (*.*)\0*.*\0";
        ofn.nFilterIndex = asCsv ? 1 : 2;
        ofn.lpstrDefExt  = asCsv ? L"csv" : L"json";
        ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&ofn)) return;

        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            MessageBoxW(statsCtx->hDlg, L"无法写入文件。", L"导出失败", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD written;
        const BYTE bomUtf8[] = { 0xEF, 0xBB, 0xBF }; // UTF-8 BOM, Excel 依赖它识别编码
        WriteFile(hFile, bomUtf8, 3, &written, NULL);
        WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, NULL);
        CloseHandle(hFile);

        std::wstring msg = L"统计数据已导出到:\n" + std::wstring(filePath);
        MessageBoxW(statsCtx->hDlg, msg.c_str(), L"导出完成", MB_OK | MB_ICONINFORMATION);
    }

    void ShowAboutWindow() {
        const wchar_t ABOUT_CLASS[] = L"AboutWindow";
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = AboutDlgProc;
        wc.hInstance     = m_hInst;
        wc.hIcon         = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = ABOUT_CLASS;
        RegisterClassExW(&wc);

        int dlgW = ABOUT_MIN_W, dlgH = ABOUT_MIN_H;
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

        // 支持用户拉伸 (WS_THICKFRAME), 尺寸上下限由 WM_GETMINMAXINFO 限制
        HWND hDlg = CreateWindowExW(0, ABOUT_CLASS, L"关于 MusicPlayer",
            WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX,
            x, y, dlgW, dlgH, m_hwnd, NULL, m_hInst, NULL);
        if (!hDlg) return;

        AboutCtx* ctx = new AboutCtx{};
        ctx->hDlg = hDlg;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

        // Use Microsoft YaHei explicitly so the EDIT control has accurate
        // metrics for CJK text.  Using NULL as typeface lets the font mapper
        // pick a default that lacks CJK glyphs, forcing font linking — the
        // linked CJK font's real glyph extent can exceed tmHeight reported
        // by the base font, clipping character bottoms on every visible row.
        ctx->hGuiFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        ctx->hTitleFont = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

        // Query font line height (ascent + descent in logical units)
        HDC hdc = GetDC(hDlg);
        HFONT hOldFont = (HFONT)SelectObject(hdc, ctx->hGuiFont);
        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, hOldFont);
        ReleaseDC(hDlg, hdc);
        ctx->lineH = tm.tmHeight;

        // 子控件位置统一由 LayoutAboutControls 计算
        ctx->hTitle = CreateWindowExW(0, L"STATIC", L"MusicPlayer",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hDlg, NULL, m_hInst, NULL);
        SendMessageW(ctx->hTitle, WM_SETFONT, (WPARAM)ctx->hTitleFont, TRUE);

        wchar_t verBuf[64];
        swprintf(verBuf, 64, L"版本: %ls", APP_VERSION);
        ctx->hVersion = CreateWindowExW(0, L"STATIC", verBuf,
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hDlg, NULL, m_hInst, NULL);
        SendMessageW(ctx->hVersion, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);

        ctx->hChangelogLabel = CreateWindowExW(0, L"STATIC", L"更新历史:",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hDlg, NULL, m_hInst, NULL);
        SendMessageW(ctx->hChangelogLabel, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);

        ctx->hChangelog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", CHANGELOG,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
            ES_AUTOVSCROLL | WS_VSCROLL | ES_LEFT,
            0, 0, 0, 0, hDlg, NULL, m_hInst, NULL);
        SendMessageW(ctx->hChangelog, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);
        SendMessageW(ctx->hChangelog, EM_SETSEL, 0, 0);

        ctx->hClose = CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, hDlg, (HMENU)IDOK, m_hInst, NULL);
        SendMessageW(ctx->hClose, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);

        LayoutAboutControls(ctx);
        ShowWindow(hDlg, SW_SHOW);
    }

    void RefreshStatsDisplay(StatsDlgCtx* ctx) {
        std::string fromDate, toDate;
        ComputeStatsRange(ctx, fromDate, toDate);

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

        // Play count
        ListView_DeleteAllItems(ctx->hPlayCountList);
        // Sort by count descending, take top 20
        std::vector<std::pair<std::wstring, int>> sorted(m_playCount.begin(), m_playCount.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        int limit = (int)sorted.size() < 20 ? (int)sorted.size() : 20;
        for (int i = 0; i < limit; i++) {
            wchar_t rank[8];
            swprintf(rank, 8, L"%d", i + 1);
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            item.pszText = rank;
            ListView_InsertItem(ctx->hPlayCountList, &item);
            ListView_SetItemText(ctx->hPlayCountList, i, 1, (LPWSTR)GetDisplayName(sorted[i].first).c_str());
            wchar_t cnt[16];
            swprintf(cnt, 16, L"%d", sorted[i].second);
            ListView_SetItemText(ctx->hPlayCountList, i, 2, cnt);
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
            m_listenStartWall = time(NULL);
        }
    }

    void StopListening() {
        if (m_listening) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - m_listenStart).count();
            if (elapsed > 0.5) // ignore sub-second glitches
                m_history.AddSession(elapsed, m_listenStartWall);
            m_listening = false;
        }
    }


    // Play file
    void PlayFile(int index) {
        if (index < 0 || index >= m_playlist.GetCount()) return;

        KillTimer(m_hwnd, TIMER_ID_SEEK);
        StopListening();  // 关闭上一段计时，避免换歌/加载间隙被计入听歌时长
        const std::wstring& path = m_playlist.GetFile(index);

        if (!m_audio.Load(path)) {
            std::wstring errMsg = m_audio.GetErrorMessage();
            std::wstring displayName = GetDisplayName(path);
            WriteLog(L"加载文件失败 [%ls]: %ls", errMsg.c_str(), path.c_str());
            SetWindowTextW(m_staticSong,
                (L"无法加载: " + displayName + L" (" + errMsg + L")").c_str());
            MessageBoxW(m_hwnd,
                (L"无法加载文件:\n" + path + L"\n\n错误原因: " + errMsg).c_str(),
                L"播放失败", MB_OK | MB_ICONWARNING);
            return;
        }

        m_currentIndex = index;
        if (m_audio.IsBalanceEnabled()) {
            m_audio.ApplyBalance();   // 首次播放该歌曲时测量响度, 之后命中缓存
        }
        m_audio.Play();
        StartListening();
        m_playCount[path]++;
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
        // 播放已知时长, 写入缓存供下次启动直接恢复
        if (len > 0)
            WriteDurationCache(path, len);
        // Find display index for this playlist index
        for (int di = 0; di < (int)m_filterMap.size(); di++) {
            if (m_filterMap[di] == index) { UpdateLVItem(di); break; }
        }

        {
            std::wstring status;
            if (!meta.empty())
                status = L"正在播放: " + meta;
            else
                status = L"正在播放: " + GetDisplayName(path);
            double speed = m_audio.GetSpeed();
            if (speed != 1.0) {
                wchar_t sb[16];
                swprintf(sb, 16, L" (%.2gx)", speed);
                status += sb;
            }
            SetWindowTextW(m_staticSong, status.c_str());
        }

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
        EnableWindow(m_btnMute, loaded);
        if (loaded) {
            SetWindowTextW(m_btnPlay, m_audio.IsPlaying() ? L"⏸" : L"▶");
        }
        UpdateModeUI();
        UpdateVolLabel();
        if (!loaded) {
            UpdateSeekDisplay();
            UpdateTimeDisplay();
        }
    }

    void LocateCurrentSong() {
        if (m_currentIndex < 0 || m_currentIndex >= m_playlist.GetCount()) return;
        for (int i = 0; i < (int)m_filterMap.size(); i++) {
            if (m_filterMap[i] == m_currentIndex) {
                ListView_SetItemState(m_playlistLV, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_SetItemState(m_playlistLV, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(m_playlistLV, i, FALSE);
                UpdatePlaylistSelection();
                break;
            }
        }
    }


    void UpdateVolLabel() {
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0);
        wchar_t buf[16];
        swprintf(buf, 16, L"%d%%", vol);
        SetWindowTextW(m_staticVolPct, buf);
        if (m_btnMute) SetWindowTextW(m_btnMute, vol == 0 ? L"🔇" : L"🔊");
    }

    void ToggleMute() {
        int vol = (int)SendMessageW(m_sliderVol, TBM_GETPOS, 0, 0);
        if (vol > 0) {
            m_lastVol = vol;
            vol = 0;
        } else {
            if (m_lastVol <= 0) m_lastVol = 80;
            vol = m_lastVol;
        }
        m_audio.SetVolume(vol);
        SendMessageW(m_sliderVol, TBM_SETPOS, TRUE, vol);
        UpdateVolLabel();
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
        // 先应用缓存时长到数据模型, 再渲染列表行; 否则 UpdateLVItem 读到 0 会一直显示 --:--
        ApplyDurationCache();

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

        // 将真正未知时长 (未命中缓存) 的歌曲入队渐进扫描
        RebuildDurationScanQueue();
    }

    // ---- Duration cache (.durations.txt) & progressive scan ----

    // 从 .durations.txt 加载时长缓存 (格式同 .loudness.txt)
    void LoadDurationCache() {
        m_durationCache.clear();
        std::wstring filePath = GetExeDirectory() + L"\\.durations.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0 && size < 16 * 1024 * 1024) {
            std::vector<char> buf(size + 1, 0);
            DWORD read;
            if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                char* p = buf.data();
                while (*p) {
                    char* nl = strchr(p, '\n');
                    if (!nl) nl = p + strlen(p);
                    *nl = '\0';
                    if (*p && *p != '\r') {
                        // 格式: duration<tab>size<tab>timeHigh<tab>timeLow<tab>path(UTF-8)
                        std::string line(p);
                        size_t t1 = line.find('\t');
                        size_t t2 = line.find('\t', t1 + 1);
                        size_t t3 = line.find('\t', t2 + 1);
                        size_t t4 = line.find('\t', t3 + 1);
                        if (t4 != std::string::npos) {
                            DurationCacheEntry e;
                            e.duration = atof(line.substr(0, t1).c_str());
                            e.size  = (ULONGLONG)strtoull(line.substr(t1 + 1, t2 - t1 - 1).c_str(), NULL, 10);
                            e.mtime.dwHighDateTime = (DWORD)strtoul(line.substr(t2 + 1, t3 - t2 - 1).c_str(), NULL, 10);
                            e.mtime.dwLowDateTime  = (DWORD)strtoul(line.substr(t3 + 1, t4 - t3 - 1).c_str(), NULL, 10);
                            std::wstring path = Utf8ToWide(line.substr(t4 + 1));
                            if (!path.empty()) m_durationCache[path] = e;
                        }
                    }
                    p = nl + 1;
                }
            }
        }
        CloseHandle(hFile);
    }

    // 写回整个 .durations.txt 缓存
    void SaveDurationCache() {
        std::wstring filePath = GetExeDirectory() + L"\\.durations.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        std::string all;
        all.reserve(m_durationCache.size() * 128);
        char tmp[128];
        for (const auto& kv : m_durationCache) {
            sprintf(tmp, "%.2f\t%llu\t%lu\t%lu\t", kv.second.duration,
                    (unsigned long long)kv.second.size,
                    (unsigned long)kv.second.mtime.dwHighDateTime,
                    (unsigned long)kv.second.mtime.dwLowDateTime);
            all += tmp;
            all += WideToUtf8(kv.first);
            all += "\n";
        }
        DWORD written;
        WriteFile(hFile, all.data(), (DWORD)all.size(), &written, NULL);
        CloseHandle(hFile);
    }

    // 将一次探测/播放得到的时长写入缓存 (带文件大小+修改时间做失效判断)
    void WriteDurationCache(const std::wstring& path, double duration) {
        if (!m_durationCacheLoaded) {
            LoadDurationCache();
            m_durationCacheLoaded = true;
        }
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return;
        DurationCacheEntry e;
        e.duration = duration;
        e.size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        e.mtime = fad.ftLastWriteTime;
        m_durationCache[path] = e;
        SaveDurationCache();
    }

    // 歌单中时长未知且缓存命中(文件未变)的歌曲, 直接恢复缓存时长
    void ApplyDurationCache() {
        if (!m_durationCacheLoaded) {
            LoadDurationCache();
            m_durationCacheLoaded = true;
        }
        for (int i = 0; i < m_playlist.GetCount(); i++) {
            auto& song = m_playlist.GetSongs()[i];
            if (song.duration > 0) continue;
            auto it = m_durationCache.find(song.filePath);
            if (it == m_durationCache.end()) continue;

            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (!GetFileAttributesExW(song.filePath.c_str(), GetFileExInfoStandard, &fad)) continue;
            ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
            if (it->second.size == sz &&
                it->second.mtime.dwHighDateTime == fad.ftLastWriteTime.dwHighDateTime &&
                it->second.mtime.dwLowDateTime == fad.ftLastWriteTime.dwLowDateTime) {
                song.duration = it->second.duration;
            }
        }
    }

    // 将所有未知时长歌曲入队, 并确保扫描定时器运行
    void RebuildDurationScanQueue() {
        m_pendingDurationScan.clear();
        for (int i = 0; i < m_playlist.GetCount(); i++) {
            if (m_playlist.GetSong(i).duration <= 0)
                m_pendingDurationScan.push_back(i);
        }
        if (!m_pendingDurationScan.empty())
            SetTimer(m_hwnd, TIMER_ID_DURATION_SCAN, 500, NULL);
        else
            KillTimer(m_hwnd, TIMER_ID_DURATION_SCAN);
    }

    // 重新统计歌曲时长: 全量重扫, 但逐首对比文件信息(size+mtime)与缓存, 一致则跳过
    void OnRescanDurations() {
        if (m_fullDurationScan) {
            MessageBoxW(m_hwnd, L"正在统计歌曲时长，请稍候", L"提示", MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (!m_durationCacheLoaded) {
            LoadDurationCache();
            m_durationCacheLoaded = true;
        }
        m_pendingDurationScan.clear();
        for (int i = 0; i < m_playlist.GetCount(); i++)
            m_pendingDurationScan.push_back(i);
        if (m_pendingDurationScan.empty()) return;
        m_fullDurationScan = true;
        SetTimer(m_hwnd, TIMER_ID_DURATION_SCAN, 500, NULL);
    }

    void UpdateLVItemForPlaylistIndex(int idx) {
        for (int di = 0; di < (int)m_filterMap.size(); di++) {
            if (m_filterMap[di] == idx) { UpdateLVItem(di); break; }
        }
    }

    // 每个 tick 探测一个文件, 成功后更新该行时长并写缓存
    void OnTimerDurationScan() {
        if (m_pendingDurationScan.empty()) {
            KillTimer(m_hwnd, TIMER_ID_DURATION_SCAN);
            m_fullDurationScan = false;
            return;
        }
        int idx = m_pendingDurationScan.back();
        m_pendingDurationScan.pop_back();
        if (idx < 0 || idx >= m_playlist.GetCount()) {
            if (m_pendingDurationScan.empty()) {
                KillTimer(m_hwnd, TIMER_ID_DURATION_SCAN);
                m_fullDurationScan = false;
            }
            return;
        }
        auto& song = m_playlist.GetSongs()[idx];
        bool needProbe = true;

        if (m_fullDurationScan) {
            // 文件信息与缓存一致且已有有效时长 → 跳过, 不重复探测
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(song.filePath.c_str(), GetFileExInfoStandard, &fad)) {
                ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                auto it = m_durationCache.find(song.filePath);
                if (it != m_durationCache.end() && it->second.duration > 0 &&
                    it->second.size == sz &&
                    it->second.mtime.dwHighDateTime == fad.ftLastWriteTime.dwHighDateTime &&
                    it->second.mtime.dwLowDateTime == fad.ftLastWriteTime.dwLowDateTime) {
                    needProbe = false;
                    if (song.duration <= 0) {
                        song.duration = it->second.duration;
                        UpdateLVItemForPlaylistIndex(idx);
                    }
                }
            }
        } else if (song.duration > 0) {
            needProbe = false;
        }

        if (needProbe) {
            double dur = AudioEngine::ProbeDuration(song.filePath);
            if (dur > 0) {
                song.duration = dur;
                WriteDurationCache(song.filePath, dur);
                UpdateLVItemForPlaylistIndex(idx);
            }
        }

        if (m_pendingDurationScan.empty()) {
            KillTimer(m_hwnd, TIMER_ID_DURATION_SCAN);
            m_fullDurationScan = false;
        }
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
        char buf[200];
        int len = sprintf(buf, "autoplay=%d\nremember_progress=%d\ntray_minimize=%d\nplay_mode=%d\nplay_speed=%.2f\nloudness_balance=%d\n",
                          m_settingsAutoplay,
                          m_settingsRememberProgress ? 1 : 0,
                          m_settingsTray ? 1 : 0,
                          (int)m_audio.GetPlayMode(),
                          m_audio.GetSpeed(),
                          m_balanceEnabled ? 1 : 0);
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
        int balanceEnabled = 1;
        double speed = 1.0;
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
                    else if (sscanf(p, "loudness_balance=%d", &balanceEnabled) == 1) {
                        m_balanceEnabled = (balanceEnabled != 0);
                        m_audio.SetBalanceEnabled(m_balanceEnabled);
                    }
                    else if (sscanf(p, "play_mode=%d", &mode) == 1) {
                        if (mode >= 0 && mode <= 2) {
                            m_audio.SetPlayMode(static_cast<PlayMode>(mode));
                        }
                    }
                    else if (sscanf(p, "play_speed=%lf", &speed) == 1) {
                        if (speed >= 0.1 && speed <= 10.0) {
                            m_audio.SetSpeed(speed);
                        }
                    }
                }
                p = nl + 1;
            }
            UpdateSpeedMenu();
        }
        CloseHandle(hFile);
    }


    // Tray icon

    // 资源管理器重启后延迟重加托盘图标: 通知区重建需要时间, 立即 NIM_ADD
    // 可能失败或图标被系统随后清除, 因此每隔 1.5 秒强制重加一次, 最多 12 次
    // (约 18 秒, 覆盖资源管理器 3~5 秒的重建/复位窗口)。
    void OnTrayReaddTimer() {
        if (++m_trayReaddAttempts >= 12) {
            KillTimer(m_hwnd, TIMER_ID_TRAY_READD);
            m_trayReaddActive = false;
        }
        // 先删后加: 即使 shell 已无该图标, NIM_DELETE 也无害, 可强制清理可能残留的旧状态
        NOTIFYICONDATAW del = {};
        del.cbSize = sizeof(del);
        del.hWnd = m_hwnd;
        del.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &del);
        m_trayIconAdded = false;  // 即使上次已成功也强制重加, 防止图标再次被系统清除
        AddTrayIcon();
    }

    // 周期性心跳: 不依赖 TaskbarCreated (Win11 及第三方任务栏可能不广播该消息),
    // 每 5 秒用 NIM_MODIFY 重新断言图标存在并保持可见; 若图标已被系统清除则 NIM_MODIFY
    // 失败, 借此探测并重新 NIM_ADD。
    void OnTrayWatchdog() {
        if (!m_trayIconAdded) {
            AddTrayIcon();
            return;
        }
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE;
        nid.dwState = 0;
        nid.dwStateMask = NIS_HIDDEN;  // 清除隐藏位, 强制图标显示在通知区而非被折叠隐藏
        nid.uCallbackMessage = WM_APP_TRAY;
        nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        BuildTrayTipText(nid.szTip, 128);
        if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) {
            WriteLog(L"Watchdog: NIM_MODIFY 失败, 图标已被系统清除, 重新 NIM_ADD");
            m_trayIconAdded = false;
            AddTrayIcon();
        }
    }

    void AddTrayIcon() {
        if (m_trayIconAdded) return;
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE;
        nid.dwState = 0;
        nid.dwStateMask = NIS_HIDDEN;  // 清除隐藏位, 强制图标显示在通知区而非被折叠隐藏
        nid.uCallbackMessage = WM_APP_TRAY;
        nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        BuildTrayTipText(nid.szTip, 128);
        if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
            WriteLog(L"Shell_NotifyIconW NIM_ADD failed (err=%lu)", GetLastError());
            return;
        }
        m_trayIconAdded = true;
        WriteLog(L"Shell_NotifyIconW NIM_ADD ok");
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
        ShowWindow(m_hwnd, SW_HIDE);
    }

    void HandleTrayMessage(WPARAM, LPARAM lp) {
        if (LOWORD(lp) == WM_LBUTTONUP) {
            ShowWindow(m_hwnd, SW_RESTORE);
            SetForegroundWindow(m_hwnd);
        } else if (LOWORD(lp) == WM_RBUTTONDOWN) {
            HMENU popup = CreatePopupMenu();
            AppendMenuW(popup, MF_STRING, ID_TRAY_PREV, L"上一首");
            AppendMenuW(popup, MF_STRING, ID_TRAY_NEXT, L"下一首");
            AppendMenuW(popup, MF_SEPARATOR, 0, NULL);
            AppendMenuW(popup, MF_STRING, ID_TRAY_PLAYPAUSE,
                m_audio.IsPlaying() ? L"暂停" : L"播放");
            AppendMenuW(popup, MF_SEPARATOR, 0, NULL);
            HMENU modeSub = CreatePopupMenu();
            PlayMode pm = m_audio.GetPlayMode();
            AppendMenuW(modeSub, MF_STRING | (pm == PlayMode::Sequential ? MF_CHECKED : MF_UNCHECKED),
                ID_PLAY_SEQUENTIAL, L"顺序播放");
            AppendMenuW(modeSub, MF_STRING | (pm == PlayMode::RepeatOne ? MF_CHECKED : MF_UNCHECKED),
                ID_PLAY_REPEATONE, L"单曲循环");
            AppendMenuW(modeSub, MF_STRING | (pm == PlayMode::Shuffle ? MF_CHECKED : MF_UNCHECKED),
                ID_PLAY_SHUFFLE, L"随机播放");
            AppendMenuW(popup, MF_POPUP, (UINT_PTR)modeSub, L"播放模式");
            AppendMenuW(popup, MF_SEPARATOR, 0, NULL);
            AppendMenuW(popup, MF_STRING, ID_TRAY_RESTORE, L"显示窗口");
            AppendMenuW(popup, MF_STRING, ID_TRAY_MINIMIZE, L"最小化到托盘");
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
            const std::wstring* targetPath = nullptr;
            int targetIndex = -1;
            if (!savedPath.empty()) {
                for (int i = 0; i < m_playlist.GetCount(); i++) {
                    if (m_playlist.GetFile(i) == savedPath) {
                        targetPath = &savedPath;
                        targetIndex = i;
                        break;
                    }
                }
            }
            if (targetIndex < 0 && savedIndex >= 0 && savedIndex < m_playlist.GetCount()) {
                targetPath = &m_playlist.GetFile(savedIndex);
                targetIndex = savedIndex;
            }
            if (targetIndex >= 0 && targetPath) {
                if (m_audio.Load(*targetPath)) {
                    m_currentIndex = targetIndex;
                    if (savedPos > 0) m_audio.SetPosition(savedPos);
                    loaded = true;
                }
            }
        } else { CloseHandle(hFile); }
        return loaded;
    }

    
    // Play count persistence
    void SavePlayCount() {
        if (m_playCount.empty()) return;
        std::wstring filePath = GetExeDirectory() + L"\\.playcount.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written;
        const WORD bom = 0xFEFF;
        WriteFile(hFile, &bom, 2, &written, NULL);
        for (const auto& entry : m_playCount) {
            std::wstring line = entry.first + L"=" + std::to_wstring(entry.second) + L"\n";
            WriteFile(hFile, line.c_str(), (DWORD)(line.size() * sizeof(wchar_t)), &written, NULL);
        }
        CloseHandle(hFile);
    }

    void LoadPlayCount() {
        std::wstring filePath = GetExeDirectory() + L"\\.playcount.txt";
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD size = GetFileSize(hFile, NULL);
        if (size > 2) {
            DWORD read = 0;
            std::wstring buf(size / 2 + 1, L'\0');
            ReadFile(hFile, &buf[0], size, &read, NULL);
            buf[read / 2] = L'\0';
            CloseHandle(hFile);
            const wchar_t* p = buf.c_str();
            if (*p == 0xFEFF) p++;
            while (*p) {
                const wchar_t* nl = wcschr(p, L'\n');
                size_t lineLen = nl ? (size_t)(nl - p) : wcslen(p);
                if (lineLen > 0 && p[lineLen-1] == L'\r') --lineLen;
                if (lineLen > 0) {
                    std::wstring line(p, lineLen);
                    size_t eq = line.find(L'=');
                    if (eq != std::wstring::npos) {
                        std::wstring path = line.substr(0, eq);
                        int count = _wtoi(line.substr(eq + 1).c_str());
                        if (count > 0) m_playCount[path] = count;
                    }
                }
                p = nl ? nl + 1 : p + lineLen;
            }
        } else { CloseHandle(hFile); }
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
    if (msg == WM_CAPTURECHANGED) {
        HKDlgCtx* c = (HKDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (c && c->recording >= 0) {
            int idx = c->recording;
            c->recording = -1;
            std::wstring ks = HotkeyToString(c->bindings[idx].vk, c->bindings[idx].mod);
            SetWindowTextW(GetDlgItem(hDlg, 100 + idx), ks.c_str());
        }
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
    if (msg == WM_COMMAND) {
        int ctrlId = LOWORD(wp);
        HKDlgCtx* c = (HKDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (!c) return DefWindowProcW(hDlg, msg, wp, lp);

        if (ctrlId >= 200 && ctrlId < 200 + c->count) {
            int idx = ctrlId - 200;
            c->recording = idx;
            SetWindowTextW(GetDlgItem(hDlg, 100 + idx), L"[按下新按键...]");
            SetCapture(hDlg);
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

// 统计对话框当前展示的 [from, to] 日期区间 (与列表显示一致)
static void ComputeStatsRange(const StatsDlgCtx* ctx, std::string& from, std::string& to) {
    time_t now_t = time(NULL);
    struct tm tm_now = *localtime(&now_t);
    char fromBuf[32], toBuf[32];
    if (ctx->useAllRange) {
        // 所有: 从最早有记录的那天到今天
        std::string earliest = ctx->win->GetHistoryEarliestDate();
        if (earliest.empty()) {
            snprintf(fromBuf, sizeof(fromBuf), "%04d-%02d-%02d",
                     tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
        } else {
            snprintf(fromBuf, sizeof(fromBuf), "%s", earliest.c_str());
        }
        snprintf(toBuf, sizeof(toBuf), "%04d-%02d-%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
    } else if (ctx->useCalendarRange) {
        snprintf(fromBuf, sizeof(fromBuf), "%04d-%02d-%02d",
                 ctx->calStart.wYear, ctx->calStart.wMonth, ctx->calStart.wDay);
        snprintf(toBuf, sizeof(toBuf), "%04d-%02d-%02d",
                 ctx->calEnd.wYear, ctx->calEnd.wMonth, ctx->calEnd.wDay);
    } else {
        tm_now.tm_mday -= ctx->rangeDays;
        tm_now.tm_isdst = -1;
        mktime(&tm_now);
        snprintf(fromBuf, sizeof(fromBuf), "%04d-%02d-%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

        struct tm tm_today = *localtime(&now_t);
        snprintf(toBuf, sizeof(toBuf), "%04d-%02d-%02d",
                 tm_today.tm_year + 1900, tm_today.tm_mon + 1, tm_today.tm_mday);
    }
    from = fromBuf;
    to = toBuf;
}

// Compute a SYSTEMTIME offset by `days` from today (negative = past)
static SYSTEMTIME DateFromNow(int days) {
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);
    tm.tm_mday += days;
    tm.tm_isdst = -1;
    mktime(&tm);
    SYSTEMTIME st = {};
    st.wYear   = tm.tm_year + 1900;
    st.wMonth  = tm.tm_mon + 1;
    st.wDay    = tm.tm_mday;
    return st;
}

// Set both DTP controls to reflect a fixed-day range (e.g. 7 or 30)
static void SyncDtpToRange(StatsDlgCtx* c) {
    if (!c->hDtpStart || !c->hDtpEnd) return;
    SYSTEMTIME endSt = DateFromNow(0);
    SYSTEMTIME startSt = DateFromNow(-c->rangeDays);
    c->dtpGuard = true;
    SendMessageW(c->hDtpStart, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&startSt);
    SendMessageW(c->hDtpEnd,   DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&endSt);
    c->dtpGuard = false;
}

// Validate custom DTP range.
static bool ValidateCustomRange(StatsDlgCtx* c, HWND hDlg) {
    SYSTEMTIME nowSt = DateFromNow(0);
    auto toInt = [](const SYSTEMTIME& s) -> int {
        return (int)s.wYear * 10000 + (int)s.wMonth * 100 + (int)s.wDay;
    };
    int endN = toInt(c->calEnd);
    int startN = toInt(c->calStart);
    int todayN = toInt(nowSt);

    bool changed = false;
    if (endN > todayN) {
        MessageBoxW(hDlg, L"终止日期不能超过今天，已自动修正。", L"日期范围", MB_OK | MB_ICONINFORMATION);
        c->calEnd = nowSt;
        changed = true;
    }
    if (startN > endN) {
        MessageBoxW(hDlg, L"起始日期晚于终止日期，已自动修正。", L"日期范围", MB_OK | MB_ICONINFORMATION);
        c->calStart = c->calEnd;
        changed = true;
    }
    if (changed) {
        c->dtpGuard = true;
        SendMessageW(c->hDtpStart, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&c->calStart);
        SendMessageW(c->hDtpEnd,   DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&c->calEnd);
        c->dtpGuard = false;
    }
    return true;
}

// Distribute extra space vertically to the three list views proportionally,
// and reflow DTPs / "刷新" horizontally.
static void LayoutStatsControls(StatsDlgCtx* c, int clientW, int clientH) {
    int extraH = clientH - c->baseH;
    int totalBaseH = c->hDay + c->hWeek + c->hPlay;

    auto scaleH = [&](int baseH) -> int {
        return baseH + extraH * baseH / totalBaseH;
    };

    int newDayH  = scaleH(c->hDay);
    int newWeekH = scaleH(c->hWeek);
    int newPlayH = scaleH(c->hPlay);

    int shiftDay  = newDayH - c->hDay;
    int shiftWeek = shiftDay + (newWeekH - c->hWeek);

    int margin = 15;
    int listW = (clientW > margin * 2) ? clientW - margin * 2 : 200;

    SetWindowPos(c->hDayList, NULL, margin, c->yDayList, listW, newDayH, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(c->hLabelDay, NULL, margin, c->yDayList - 20, listW, 20, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(c->hWeekList, NULL, margin, c->yWeekList + shiftDay, listW, newWeekH, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(c->hLabelWeek, NULL, margin, c->yWeekList + shiftDay - 20, listW, 20, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(c->hPlayCountList, NULL, margin, c->yPlayList + shiftWeek, listW, newPlayH, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(c->hLabelPlay, NULL, margin, c->yPlayList + shiftWeek - 20, listW, 20, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(c->hTotalText, NULL, margin, c->yTotal + extraH, listW, 22, SWP_NOZORDER | SWP_NOACTIVATE);

    int btnY = c->yButtons + extraH;
    SetWindowPos(GetDlgItem(c->hDlg, 305), NULL, clientW / 2 - 120, btnY, 120, 28, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(c->hDlg, IDCANCEL), NULL, clientW / 2 + 20, btnY, 90, 28, SWP_NOZORDER | SWP_NOACTIVATE);

    // Horizontal: widen DTPs, move labels, right-align "刷新"
    if (c->hDtpStart && c->hDtpEnd) {
        int dtpX = 50, dtpW = 130;
        int avail = clientW - dtpX - 110;
        if (avail > dtpW * 2 + 80) {
            dtpW = (avail - 80) / 2;
            if (dtpW > 250) dtpW = 250;
        }
        int dtp2X = dtpX + dtpW + 55;
        int dtpY = c->yDayList - 54;
        SetWindowPos(c->hDtpStart, NULL, dtpX, dtpY, dtpW, 26, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(c->hDtpEnd, NULL, dtp2X, dtpY, dtpW, 26, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(c->hDlg, 312), NULL, 15, dtpY + 3, 35, 20, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(c->hDlg, 313), NULL, dtp2X - 35, dtpY + 3, 35, 20, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SetWindowPos(GetDlgItem(c->hDlg, 304), NULL, clientW - 100, c->yDayList - 56, 80, 28, SWP_NOZORDER | SWP_NOACTIVATE);
}

// Stats dialog procedure
static LRESULT CALLBACK StatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = 660;
        mmi->ptMinTrackSize.y = 620;
        mmi->ptMaxTrackSize.x = 1400;
        mmi->ptMaxTrackSize.y = 1200;
        return 0;
    }
    if (msg == WM_SIZE && wp != SIZE_MINIMIZED) {
        StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (c && c->baseH > 0) {
            LayoutStatsControls(c, LOWORD(lp), HIWORD(lp));
        }
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
    if (msg == WM_DESTROY) {
        StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (c) {
            HWND hOwner = GetWindow(hDlg, GW_OWNER);
            delete c;
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
            // Post a message to bring the owner to foreground after
            // DestroyWindow fully completes. Direct SetForegroundWindow
            // inside WM_DESTROY can be overridden by Windows internal
            // focus processing during window destruction.
            if (hOwner && IsWindow(hOwner)) {
                PostMessage(hOwner, WM_APP_BRING_TO_TOP, 0, 0);
            }
        } else {
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
        }
        return 0;
    }
    if (msg == WM_NOTIFY) {
        LPNMHDR nm = (LPNMHDR)lp;
        if (nm->code == DTN_DATETIMECHANGE && (nm->idFrom == 310 || nm->idFrom == 311)) {
            StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (!c || c->dtpGuard) return 0;
            SYSTEMTIME st;
            if (SendMessageW(nm->hwndFrom, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID)
                return 0;
            if (nm->idFrom == 310) c->calStart = st;
            else c->calEnd = st;
            c->useAllRange = false;
            c->useCalendarRange = true;
            ValidateCustomRange(c, hDlg);
            SendMessageW(c->hRadioAll, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(c->hRadio7, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(c->hRadio30, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(c->hRadioCustom, BM_SETCHECK, BST_CHECKED, 0);
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        return DefWindowProcW(hDlg, msg, wp, lp);
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
            if (SendMessageW(c->hRadioAll, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                c->useAllRange = true;
                c->useCalendarRange = false;
            }
            else if (SendMessageW(c->hRadio7, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                c->useAllRange = false;
                c->useCalendarRange = false;
                c->rangeDays = 7;
            }
            else if (SendMessageW(c->hRadio30, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                c->useAllRange = false;
                c->useCalendarRange = false;
                c->rangeDays = 30;
            }
            else {
                SYSTEMTIME st;
                if (c->hDtpStart && SendMessageW(c->hDtpStart, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
                    c->calStart = st;
                if (c->hDtpEnd && SendMessageW(c->hDtpEnd, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
                    c->calEnd = st;
                c->useAllRange = false;
                c->useCalendarRange = true;
                ValidateCustomRange(c, hDlg);
            }
            c->win->RefreshStatsDisplay(c);
            return 0;
        }

        if (id == 300) { // Radio 7
            c->useAllRange = false;
            c->useCalendarRange = false;
            c->rangeDays = 7;
            SyncDtpToRange(c);
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 301) { // Radio 30
            c->useAllRange = false;
            c->useCalendarRange = false;
            c->rangeDays = 30;
            SyncDtpToRange(c);
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 302) { // Radio Custom
            c->useAllRange = false;
            c->useCalendarRange = true;
            c->win->RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 303) { // Radio All
            c->useAllRange = true;
            c->useCalendarRange = false;
            c->win->RefreshStatsDisplay(c);
            return 0;
        }

        if (id == 305) { // Export button
            c->win->ShowExportStatsDialog(c);
            return 0;
        }
        return 0;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// Stats export selection dialog proc
static LRESULT CALLBACK ExportStatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) {
        DestroyWindow(hDlg);
        return 0;
    }
    if (msg == WM_COMMAND) {
        ExportCtx* c = (ExportCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (!c) return DefWindowProcW(hDlg, msg, wp, lp);
        int id = LOWORD(wp);
        if (id == IDOK) {
            bool chkDaily = SendMessageW(c->hChkDaily, BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool chkWeekly = SendMessageW(c->hChkWeekly, BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool chkTop = SendMessageW(c->hChkTop, BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool chkTotal = SendMessageW(c->hChkTotal, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (!chkDaily && !chkWeekly && !chkTop && !chkTotal) {
                MessageBoxW(hDlg, L"请至少勾选一项导出内容。", L"提示", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            c->result = 1;
            c->asCsv = SendMessageW(c->hRadioCsv, BM_GETCHECK, 0, 0) == BST_CHECKED;
            c->includeDaily = chkDaily;
            c->includeWeekly = chkWeekly;
            c->includeTopSongs = chkTop;
            c->includeTotal = chkTotal;
            DestroyWindow(hDlg);
            return 0;
        }
        if (id == IDCANCEL) {
            DestroyWindow(hDlg);
            return 0;
        }
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// Speed input dialog proc
static LRESULT CALLBACK SpeedInputDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) {
        DestroyWindow(hDlg);
        return 0;
    }
    if (msg == WM_COMMAND) {
        int id = LOWORD(wp);
        if (id == IDOK) {
            wchar_t buf[16];
            GetWindowTextW(GetDlgItem(hDlg, 500), buf, 16);
            double val = _wtof(buf);
            if (val < 0.1 || val > 10.0) {
                MessageBoxW(hDlg, L"请输入 0.1 ~ 10.0 之间的数值", L"无效输入", MB_OK | MB_ICONWARNING);
                return 0;
            }
            // Store result in context
            struct SpeedCtx { MainWindow* win; double result; };
            SpeedCtx* ctx = (SpeedCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (ctx) ctx->result = val;
            DestroyWindow(hDlg);
            return 0;
        }
        if (id == IDCANCEL) {
            DestroyWindow(hDlg);
            return 0;
        }
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

static LRESULT CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    AboutCtx* ctx = (AboutCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = ABOUT_MIN_W;
        mmi->ptMinTrackSize.y = ABOUT_MIN_H;
        mmi->ptMaxTrackSize.x = ABOUT_MAX_W;
        mmi->ptMaxTrackSize.y = ABOUT_MAX_H;
        return 0;
    }
    // WM_SIZE 在 CreateWindowExW 期间可能早于 GWLP_USERDATA 设置, ctx 为空时跳过
    if (msg == WM_SIZE && ctx) {
        LayoutAboutControls(ctx);
        return 0;
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hDlg);
        return 0;
    }
    if (msg == WM_DESTROY) {
        HWND hOwner = GetWindow(hDlg, GW_OWNER);
        if (hOwner && IsWindow(hOwner))
            PostMessage(hOwner, WM_APP_BRING_TO_TOP, 0, 0);
        if (ctx) {
            if (ctx->hGuiFont)   DeleteObject(ctx->hGuiFont);
            if (ctx->hTitleFont) DeleteObject(ctx->hTitleFont);
            delete ctx;
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
        }
        return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        DestroyWindow(hDlg);
        return 0;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Single-instance check: a named mutex ensures only one process runs.
    const wchar_t MUTEX_NAME[] = L"Local\\MusicPlayer_SingleInstance";
    HANDLE hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hWnd = FindWindowW(CLASS_NAME, NULL);
        if (hWnd) {
            if (IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
        }
        CloseHandle(hMutex);
        return 0;
    }

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
