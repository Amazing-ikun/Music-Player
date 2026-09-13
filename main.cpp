#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM

// MusicPlayer version
static const wchar_t* APP_VERSION = L"2.0.2";

// Changelog — shown in the About dialog
static const wchar_t* CHANGELOG =
    L"v2.0.2\r\n"
    L"  - 重构: 桌面歌词编排抽成 LyricsController(歌词解析/悬浮窗/映射表集中管理), 悬浮窗 6 个回调合并为 LyricWindowListener 接口, 歌曲时长缓存抽成 DurationCache, 新增共用的 TextFile 文本读写模块(消除 4 处重复的编码转换), main.cpp 再精简约 500 行\r\n"
    L"  - 统一: .playlist.txt/.lastsong.txt/.lastfolder.txt/.playcount.txt/.lyrics_map.txt/.history.txt 落盘编码统一为 UTF-8 (读取时自动识别旧的 UTF-16, 旧文件仍可正常加载)\r\n"
    L"  - 优化: 进度条点击轨道任意位置立即跳转并播放 (原为固定翻页, 每次只移动全曲的 5%)\r\n"
    L"  - 修复: 休眠唤醒后托盘提示冻结, 换歌/暂停都不再刷新 (根因: 唤醒瞬间一次 NIM_MODIFY 瞬时失败后只再尝试 NIM_ADD, 而 shell 中的图标仍在, NIM_ADD 始终失败。改为始终先试 NIM_MODIFY, 失败才先 NIM_DELETE 清理残留再重加)\r\n"
    L"  - 修复: 休眠唤醒后播放状态与界面/托盘脱节 (新增与 BASS 真实状态的周期性对账; 唤醒时重新对账并在设备被系统停掉时尝试恢复输出; 另收尾被休眠打断的暂停淡出, 避免永久卡在\"正在播放\")\r\n"
    L"  - 修复: 暂停淡出结束、歌单播放完毕、移除当前歌曲、打开空文件夹后托盘提示不同步\r\n"
    L"  - 修复: MP3 显示成歌手名而非\"歌手 - 标题\" (根因: 只读 ID3v1 且该标签标题字段可能为空; 新增 ID3v2 TIT2/TPE1 解析, 支持 UTF-8/UTF-16/ANSI 文本帧)\r\n"
    L"  - 修复: FLAC/Ogg 标签读取用错 BASS 常量 (BASS_TAG_META 实为网络流 ICY 元数据), 改用 BASS_TAG_OGG\r\n"
    L"\r\n"
    L"v2.0.1\r\n"
    L"  - 重构: 代码架构优化(高内聚低耦合), 从主窗口拆出 Settings(设置持久化)/TrayIcon(托盘图标)/Dialogs(全部对话框)/Hotkey(热键类型) 四个模块, 消除对话框对主窗口的反向依赖, main.cpp 代码行缩减约35%\r\n"
    L"\r\n"
    L"v2.0.0\r\n"
    L"  - 新增: 桌面歌词悬浮窗 (置顶、无边框、GDI+ 逐像素半透明渲染, 鼠标移开 5 秒后背景透明仅显示歌词)\r\n"
    L"  - 新增: 本地 .lrc 歌词解析 (自动识别 UTF-8/UTF-16/GBK 编码, 支持多时间戳与 offset 偏移)\r\n"
    L"  - 新增: 双语歌词 (同一时间戳第 2 行为译文, 可切换显示/隐藏, 原文在上译文在下)\r\n"
    L"  - 新增: 当前句 + 下一句两行显示, 长歌词超宽时横向滚动(走马灯)\r\n"
    L"  - 新增: 悬浮窗可拖动、自由缩放(高度↔首行/次行字号等比联动)、锁定模式(仅歌词、禁止拖拽缩放、命中范围收紧到字幕)\r\n"
    L"  - 新增: 歌词匹配 (文件菜单「扫描歌词」统计同目录同名 .lrc; 歌曲右键「匹配歌词」手动选择 .lrc)\r\n"
    L"  - 新增: 歌词设置 (首行/次行字号、首行/次行颜色, 颜色支持十六进制与 RGB 输入)\r\n"
    L"  - 新增: 主界面「词」按钮与托盘菜单开关桌面歌词; 悬浮窗右键菜单(居中/锁定/显示译文/上一首/下一首/隐藏)\r\n"
    L"  - 修复: 歌词按时间戳排序不稳定导致原文/译文顺序颠倒 (改用稳定排序)\r\n"
    L"\r\n"
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
#include "LyricsController.h"
#include "DurationCache.h"
#include "Settings.h"
#include "TrayIcon.h"
#include "Dialogs.h"
#include "Hotkey.h"
#include "TextFile.h"
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

// MainWindow
class MainWindow {
public:
    MainWindow()
        : m_hwnd(NULL), m_hInst(NULL)
        , m_playlistLV(NULL), m_searchEdit(NULL)
        , m_btnPrev(NULL), m_btnPlay(NULL), m_btnNext(NULL), m_btnMode(NULL), m_btnLocate(NULL)
        , m_btnMute(NULL)
        , m_btnLyrics(NULL)
        , m_trackSeek(NULL), m_sliderVol(NULL), m_staticVolPct(NULL)
        , m_lastVol(80)
        , m_staticTime(NULL), m_staticSong(NULL)
        , m_currentIndex(-1), m_userDraggingSeek(false)
        , m_sortColumn(-1), m_sortAscending(true)
        , m_shufflePos(0)
        , m_taskbarCreatedMsg(0)
        , m_trayReaddAttempts(0), m_trayReaddActive(false)
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

private:
    // ---- Controls ----
    HWND m_hwnd;
    HINSTANCE m_hInst;
    HWND m_playlistLV;
    HWND m_searchEdit;
    HWND m_btnPrev, m_btnPlay, m_btnNext, m_btnMode, m_btnLocate;
    HWND m_btnMute;
    HWND m_btnLyrics;
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

    // ---- Settings / Tray ----
    Settings m_settings;
    TrayIcon m_trayIcon;
    UINT m_taskbarCreatedMsg; // "TaskbarCreated" 注册消息, 用于探测资源管理器重启
    UINT m_trayReaddAttempts; // 资源管理器重启后延迟重加托盘图标的尝试次数
    bool m_trayReaddActive;   // 重试定时器当前是否在运行

    // ---- Desktop lyrics ----
    LyricsController m_lyricsCtl;

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
    DurationCache m_durationCache;
    std::vector<int> m_pendingDurationScan;  // playlist indexes to scan (unknown durations, or all during full rescan)
    bool m_fullDurationScan = false;         // 全量重扫进行中 (用于防重复执行)

    // ---- Undo remove ----
    SongInfo m_undoSong;
    int m_undoIndex;
    bool m_undoValid;

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

    // 进度条子类过程: 点击轨道(非滑块)时立即跳到点击处, 而非默认的翻页行为;
    // 点在滑块上则交回默认处理, 保证拖拽功能不变。
    static LRESULT CALLBACK SeekTrackProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR id, DWORD_PTR refData) {
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT thumb = {};
            SendMessageW(hwnd, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb);
            if (!PtInRect(&thumb, pt)) {
                // 未抓滑块: 把点击横坐标换算成位置
                int min = (int)SendMessageW(hwnd, TBM_GETRANGEMIN, 0, 0);
                int max = (int)SendMessageW(hwnd, TBM_GETRANGEMAX, 0, 0);
                int cur = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);

                // 实测滑块中心在两端的位置, 免去对轨道/滑块几何的假设
                RECT t = {};
                SendMessageW(hwnd, TBM_SETPOS, FALSE, min);
                SendMessageW(hwnd, TBM_GETTHUMBRECT, 0, (LPARAM)&t);
                int leftCenter = (t.left + t.right) / 2;
                SendMessageW(hwnd, TBM_SETPOS, FALSE, max);
                SendMessageW(hwnd, TBM_GETTHUMBRECT, 0, (LPARAM)&t);
                int rightCenter = (t.left + t.right) / 2;
                SendMessageW(hwnd, TBM_SETPOS, FALSE, cur);   // 还原
                if (rightCenter <= leftCenter) {              // 兜底: 退回轨道矩形
                    RECT ch = {};
                    SendMessageW(hwnd, TBM_GETCHANNELRECT, 0, (LPARAM)&ch);
                    leftCenter = ch.left;
                    rightCenter = ch.right;
                }

                int pos = cur;
                if (rightCenter > leftCenter) {
                    double ratio = (double)(pt.x - leftCenter) / (rightCenter - leftCenter);
                    if (ratio < 0.0) ratio = 0.0;
                    if (ratio > 1.0) ratio = 1.0;
                    pos = min + (int)(ratio * (max - min) + 0.5);
                }
                SendMessageW(hwnd, TBM_SETPOS, TRUE, pos);

                MainWindow* self = (MainWindow*)refData;
                if (self) {   // 复用 WM_HSCROLL 的跳转逻辑
                    self->OnHScroll(MAKEWPARAM(TB_THUMBPOSITION, 0), (LPARAM)hwnd);
                }
                SetFocus(hwnd);
                return 0;   // 吞掉默认翻页/捕获处理
            }
        } else if (msg == WM_NCDESTROY) {
            RemoveWindowSubclass(hwnd, &MainWindow::SeekTrackProc, id);
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
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
                // 唤醒后设备可能已被系统停掉, 需重新对账并在必要时恢复输出,
                // 再把界面/托盘刷成真实状态 (否则会一直显示休眠前的歌曲信息)
                if (wp == PBT_APMSUSPEND) {
                    // 若此刻正在暂停淡出, 休眠会让滑动同步回调永不触发: 先按"暂停"收尾,
                    // 把状态定死在休眠之前, 免得醒来后卡在"正在播放"
                    m_audio.FinishPendingPause();
                    StopListening();
                    m_history.Save(GetExeDirectory() + L"\\.history.txt");
                } else if (wp == PBT_APMRESUMESUSPEND || wp == PBT_APMRESUMEAUTOMATIC) {
                    m_audio.ResumeAfterSuspend();
                    if (m_audio.IsPlaying())
                        StartListening();
                    UpdateUI();
                    UpdateTrayTip();
                    Log(L"唤醒: playing=%d paused=%d loaded=%d",
                        (int)m_audio.IsPlaying(), (int)m_audio.IsPaused(), (int)m_audio.IsLoaded());
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
            case WM_CTLCOLORBTN: {
                // 「词」按钮灰度差异化: 关闭时灰字, 开启时黑字
                if ((HWND)lp == m_btnLyrics) {
                    HDC hdc = (HDC)wp;
                    SetTextColor(hdc, m_settings.lyricsShow ? RGB(0, 0, 0) : RGB(150, 150, 150));
                    SetBkMode(hdc, TRANSPARENT);
                    return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
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
        m_trayIcon.SetTarget(m_hInst, m_hwnd);
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_DATE_CLASSES };
        InitCommonControlsEx(&icc);
        CreateMenuBar();
        CreateControls();

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
        LyricsHost lyricsHost;
        lyricsHost.hInst = m_hInst;
        lyricsHost.owner = m_hwnd;
        lyricsHost.currentSongPath = [this]() -> std::wstring {
            if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount())
                return m_playlist.GetFile(m_currentIndex);
            return L"";
        };
        lyricsHost.playbackPosition = [this]() { return m_audio.GetPosition(); };
        lyricsHost.navigate = [this](bool next) { if (next) OnNext(); else OnPrev(); };
        lyricsHost.persistSettings = [this]() { SaveSettings(); };
        lyricsHost.visibilityChanged = [this]() { UpdateLyricButton(); UpdateSettingsMenu(); };
        lyricsHost.songCount = [this]() { return m_playlist.GetCount(); };
        lyricsHost.songPathAt = [this](int i) -> std::wstring {
            if (i < 0 || i >= m_playlist.GetCount()) return L"";
            return m_playlist.GetFile(i);
        };
        m_lyricsCtl.Init(&m_settings, lyricsHost);
        m_lyricsCtl.LoadMap();

        bool startPlay = (m_settings.autoplay == 1);
        if (m_settings.rememberProgress && !m_playlist.IsEmpty()) {
            if (LoadLastSong()) {
                m_lyricsCtl.LoadForCurrentSong();
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
        // 桌面歌词: 同步按钮状态, 并按上次状态恢复显示
        UpdateLyricButton();
        if (m_settings.lyricsShow) {
            m_lyricsCtl.SetVisible(true);
        }
        UpdateUI();
    }

    void OnCloseRequest() {
        if (m_settings.trayMinimize) {
            MinimizeToTray();
        } else {
            OnRealClose();
        }
    }

    void OnRealClose() {
        StopListening();
        m_history.Save(GetExeDirectory() + L"\\.history.txt");
        if (m_settings.rememberProgress) SaveLastSong();
        SavePlayCount();
        SavePlaylist();
        SaveVolume();
        SaveSettings();
        UnregisterHotKeys();
        RemoveTrayIcon();
        m_lyricsCtl.Destroy();
        m_audio.Cleanup();
        DestroyWindow(m_hwnd);
    }

    // Menu bar
    void CreateMenuBar() {
        HMENU bar = CreateMenu();

        HMENU fileMenu = CreatePopupMenu();
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_OPENFOLDER, L"打开文件夹(&O)...");
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_ADDFILES, L"添加歌曲(&A)...");
        AppendMenuW(fileMenu, MF_STRING, ID_FILE_MATCH_LYRICS, L"扫描歌词(&M)");
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
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_LYRICS,
            L"桌面歌词(&L)");
        AppendMenuW(m_settingsMenu, MF_STRING, ID_SETTINGS_LYRICS_OPTIONS,
            L"歌词设置...");
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

        m_btnLyrics = CreateWindowExW(0, L"BUTTON", L"词",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_BTN_LYRICS, m_hInst, NULL);

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
        // 让点击轨道立即跳转 (默认是翻页), 见 SeekTrackProc
        SetWindowSubclass(m_trackSeek, &MainWindow::SeekTrackProc, IDC_TRACK_SEEK, (DWORD_PTR)this);

        m_staticTime = CreateWindowExW(0, L"STATIC", L"00:00 / 00:00",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
            m_hwnd, (HMENU)IDC_STAT_TIME, m_hInst, NULL);
        m_staticSong = CreateWindowExW(0, L"STATIC", L"就绪",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            0, 0, 0, 0, m_hwnd, (HMENU)IDC_STAT_SONG, m_hInst, NULL);

        HWND ctls[] = { m_btnMode, m_btnPrev, m_btnPlay, m_btnNext, m_btnLocate, m_btnMute, m_btnLyrics,
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
        int lyricW = 36;
        int volPctW = 36;
        int volW = 130;
        int volX = w - M - volPctW - volW - muteW - 4;
        SetWindowPos(m_btnLyrics, NULL, volX - lyricW - 4, y + 4, lyricW, BH, SWP_NOZORDER);
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
                case ID_FILE_MATCH_LYRICS:    m_lyricsCtl.MatchAll(); break;
                case ID_FILE_EXPORT_PLAYLIST: ExportPlaylist(); break;
                case ID_UNDO_REMOVE:          UndoRemove();     break;
                case ID_FILE_EXIT:            OnRealClose(); break;
                case ID_STARTUP_NOTHING:
                    m_settings.autoplay = 0;
                    UpdateSettingsMenu();
                    SaveSettings();
                    break;
                case ID_STARTUP_AUTOPLAY:
                    m_settings.autoplay = 1;
                    UpdateSettingsMenu();
                    SaveSettings();
                    break;
                case ID_SETTINGS_REMEMBER:
                    m_settings.rememberProgress = !m_settings.rememberProgress;
                    UpdateSettingsMenu();
                    break;
                case ID_SETTINGS_TRAY:
                    m_settings.trayMinimize = !m_settings.trayMinimize;
                    UpdateSettingsMenu();
                    break;
                case ID_SETTINGS_BALANCE:
                    m_settings.balanceEnabled = !m_settings.balanceEnabled;
                    m_audio.SetBalanceEnabled(m_settings.balanceEnabled);
                    UpdateSettingsMenu();
                    SaveSettings();
                    break;
                case ID_SETTINGS_LYRICS:
                    m_lyricsCtl.SetVisible(!m_settings.lyricsShow);
                    break;
                case ID_SETTINGS_LYRICS_OPTIONS:
                    m_lyricsCtl.ShowSettingsDialog();
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
                else if (hCtrl == m_btnLyrics) m_lyricsCtl.SetVisible(SendMessageW(m_btnLyrics, BM_GETCHECK, 0, 0) == BST_CHECKED);
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
                    if (m_settings.rememberProgress) SaveLastSong();
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
                    AppendMenuW(hMenu, MF_STRING, 3106, L"匹配歌词(&M)");
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
                            case 3106: m_lyricsCtl.MatchForSong(songIdx); break;
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
                                UpdateTrayTip();  // 若移除的正是当前歌曲, 托盘需同步清掉
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
            m_lyricsCtl.Tick();
            // 兜底: 暂停淡出的同步回调若丢失(休眠/设备丢失), 到点主动收尾, 否则会一直卡在"正在播放"
            if (m_audio.IsFadeStuck()) {
                Log(L"暂停淡出超时未完成, 主动收尾为已暂停");
                m_audio.FinishPendingPause();
                StopListening();
                UpdateUI();
                UpdateTrayTip();
            }
            // 兜底对账: 设备丢失/休眠等导致播放状态与 BASS 脱节时, 半秒内自愈并刷新界面与托盘
            if (m_audio.SyncStateFromBass()) {
                if (m_audio.IsPlaying()) StartListening(); else StopListening();
                UpdateUI();
                UpdateTrayTip();
            }
            if (++m_saveTick >= 20) {
                m_saveTick = 0;
                if (m_settings.rememberProgress)
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
            UpdateTrayTip();  // 否则托盘会一直挂着刚放完的那首歌
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
                UpdateTrayTip();  // 已卸载上一首, 托盘不能继续显示它
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
            // 此刻只是开始淡出, 播放状态尚未改变, 托盘留到 OnFadeDone() 里再刷,
            // 否则会先显示"正在播放"、与窗口里的"已暂停"自相矛盾
            m_audio.PauseFade(500);
            SetWindowTextW(m_staticSong, L"已暂停");
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
        UpdateTrayTip();   // 暂停状态到这一刻才真正生效, 此时刷新托盘才是正确的
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
            MF_BYCOMMAND | (m_settings.autoplay == 0 ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_startupSubMenu, ID_STARTUP_AUTOPLAY,
            MF_BYCOMMAND | (m_settings.autoplay == 1 ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_REMEMBER,
            MF_BYCOMMAND | (m_settings.rememberProgress ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_TRAY,
            MF_BYCOMMAND | (m_settings.trayMinimize ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_BALANCE,
            MF_BYCOMMAND | (m_settings.balanceEnabled ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_settingsMenu, ID_SETTINGS_LYRICS,
            MF_BYCOMMAND | (m_settings.lyricsShow ? MF_CHECKED : MF_UNCHECKED));
    }


    // Speed input dialog

    void ShowSpeedInputDialog() {
        double v = ::ShowSpeedInputDialog(m_hInst, m_hwnd, m_audio.GetSpeed());
        if (v > 0) ApplySpeed(v);
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
        if (::ShowHotkeyDialog(m_hInst, m_hwnd, tmp, m_hotkeyCount)) {
            memcpy(m_hotkeys, tmp, sizeof(m_hotkeys));
            UnregisterHotKeys();
            RegisterHotKeys();
            SaveHotkeyBindings();
        }
    }


    void ShowAboutWindow() {
        ::ShowAboutDialog(m_hInst, m_hwnd, APP_VERSION, CHANGELOG);
    }

    void ShowStatsWindow() {
        StatsData data;
        data.history = &m_history;
        data.playCount = &m_playCount;
        ::ShowStatsWindow(m_hInst, m_hwnd, data);
    }

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
        m_lyricsCtl.LoadForCurrentSong();
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
            m_durationCache.Put(path, len);
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

    // 歌单中时长未知且缓存命中(文件未变)的歌曲, 直接恢复缓存时长
    void ApplyDurationCache() {
        for (int i = 0; i < m_playlist.GetCount(); i++) {
            auto& song = m_playlist.GetSongs()[i];
            if (song.duration > 0) continue;
            double cached = m_durationCache.Get(song.filePath);
            if (cached > 0) song.duration = cached;
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
            // 文件未变且缓存已有有效时长 → 跳过, 不重复探测
            double cached = m_durationCache.Get(song.filePath);
            if (cached > 0) {
                needProbe = false;
                if (song.duration <= 0) {
                    song.duration = cached;
                    UpdateLVItemForPlaylistIndex(idx);
                }
            }
        } else if (song.duration > 0) {
            needProbe = false;
        }

        if (needProbe) {
            double dur = AudioEngine::ProbeDuration(song.filePath);
            if (dur > 0) {
                song.duration = dur;
                m_durationCache.Put(song.filePath, dur);
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
        m_settings.playMode = (int)m_audio.GetPlayMode();
        m_settings.playSpeed = m_audio.GetSpeed();
        m_settings.Save(GetExeDirectory() + L"\\.settings.txt");
    }

    void LoadSettings() {
        LoadHotkeyBindings();
        m_settings.Load(GetExeDirectory() + L"\\.settings.txt");
        m_audio.SetBalanceEnabled(m_settings.balanceEnabled);
        m_audio.SetPlayMode(static_cast<PlayMode>(m_settings.playMode));
        m_audio.SetSpeed(m_settings.playSpeed);
    }


    // Desktop lyrics

    // 同步「词」按钮勾选状态
    void UpdateLyricButton() {
        if (m_btnLyrics) {
            SendMessageW(m_btnLyrics, BM_SETCHECK, m_settings.lyricsShow ? BST_CHECKED : BST_UNCHECKED, 0);
            InvalidateRect(m_btnLyrics, NULL, TRUE);
        }
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
        if (!m_trayIcon.ReAdd(BuildTrayTipText()))
            Log(L"[托盘] 重启后重加图标失败 (尝试 %d, err=%lu)", m_trayReaddAttempts, m_trayIcon.LastError());
    }

    // 周期性心跳: 不依赖 TaskbarCreated (Win11 及第三方任务栏可能不广播该消息),
    // 每 5 秒用 NIM_MODIFY 重新断言图标存在并保持可见; 若图标已被系统清除则 NIM_MODIFY
    // 失败, 借此探测并重新 NIM_ADD。
    void OnTrayWatchdog() {
        if (!m_trayIcon.Reassert(BuildTrayTipText()))
            Log(L"[托盘] 心跳重断言失败 (added=%d err=%lu)", (int)m_trayIcon.IsAdded(), m_trayIcon.LastError());
    }

    void AddTrayIcon() {
        if (!m_trayIcon.ReAdd(BuildTrayTipText()))
            Log(L"[托盘] 添加图标失败 (err=%lu)", m_trayIcon.LastError());
    }

    void RemoveTrayIcon() {
        m_trayIcon.Remove();
    }

    std::wstring BuildTrayTipText() {
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.GetCount()) {
            std::wstring meta = m_audio.GetFormattedMetadata();
            std::wstring name = meta.empty() ? GetDisplayName(m_playlist.GetFile(m_currentIndex)) : meta;
            if (m_audio.IsLoaded() && m_audio.IsPlaying())
                return L"正在播放: " + name;
            if (m_audio.IsLoaded())
                return L"已暂停: " + name;
        }
        return WINDOW_TITLE;
    }

    void UpdateTrayTip() {
        if (!m_trayIcon.UpdateTip(BuildTrayTipText()))
            Log(L"[托盘] 更新提示失败 (added=%d err=%lu)", (int)m_trayIcon.IsAdded(), m_trayIcon.LastError());
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
            AppendMenuW(popup, MF_STRING | (m_settings.lyricsShow ? MF_CHECKED : MF_UNCHECKED),
                ID_SETTINGS_LYRICS, m_settings.lyricsShow ? L"关闭桌面歌词" : L"开启桌面歌词");
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
        wchar_t idxBuf[64], posBuf[64];
        swprintf(idxBuf, 64, L"%d\n", m_currentIndex);
        swprintf(posBuf, 64, L"%.3f\n", m_audio.GetPosition());
        std::wstring text = std::wstring(idxBuf) + posBuf +
                            m_playlist.GetFile(m_currentIndex) + L"\n";
        WriteTextUtf8(GetExeDirectory() + L"\\.lastsong.txt", text);
    }

    bool LoadLastSong() {
        std::vector<std::wstring> lines =
            ReadLinesAuto(GetExeDirectory() + L"\\.lastsong.txt");
        if (lines.size() < 2) return false;
        int savedIndex = _wtoi(lines[0].c_str());
        double savedPos = _wtof(lines[1].c_str());
        std::wstring savedPath = (lines.size() >= 3) ? lines[2] : std::wstring();

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
                return true;
            }
        }
        return false;
    }

    
    // Play count persistence
    void SavePlayCount() {
        if (m_playCount.empty()) return;
        std::wstring text;
        for (const auto& entry : m_playCount)
            text += entry.first + L"=" + std::to_wstring(entry.second) + L"\n";
        WriteTextUtf8(GetExeDirectory() + L"\\.playcount.txt", text);
    }

    void LoadPlayCount() {
        for (const std::wstring& line :
                ReadLinesAuto(GetExeDirectory() + L"\\.playcount.txt")) {
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring path = line.substr(0, eq);
            int count = _wtoi(line.substr(eq + 1).c_str());
            if (count > 0) m_playCount[path] = count;
        }
    }

    // Playlist persistence
    void SavePlaylist() {
        if (m_playlist.IsEmpty()) return;
        std::wstring text;
        for (int i = 0; i < m_playlist.GetCount(); i++)
            text += m_playlist.GetFile(i) + L"\n";
        WriteTextUtf8(GetExeDirectory() + L"\\.playlist.txt", text);
    }

    bool LoadPlaylist() {
        m_playlist.Clear();
        for (const std::wstring& line :
                ReadLinesAuto(GetExeDirectory() + L"\\.playlist.txt")) {
            if (GetFileAttributesW(line.c_str()) != INVALID_FILE_ATTRIBUTES)
                m_playlist.AddFile(line);
        }
        bool loaded = !m_playlist.IsEmpty();
        if (loaded) {
            m_playlist.Sort(1, true);
            m_sortColumn = 1;
            m_sortAscending = true;
        }
        RefreshPlaylistUI();
        if (loaded)
            SetWindowTextW(m_staticSong,
                (L"已加载 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
        return loaded;
    }


    // Last folder persistence
    void SaveLastFolder(const std::wstring& folderPath) {
        WriteTextUtf8(GetExeDirectory() + L"\\.lastfolder.txt", folderPath + L"\n");
    }

    bool LoadFromLastFolder() {
        std::vector<std::wstring> lines =
            ReadLinesAuto(GetExeDirectory() + L"\\.lastfolder.txt");
        if (lines.empty()) return false;
        const std::wstring& folder = lines[0];
        if (GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES ||
            !(GetFileAttributesW(folder.c_str()) & FILE_ATTRIBUTE_DIRECTORY)) {
            return false;
        }
        m_playlist.ScanFolder(folder);
        m_sortColumn = 1;
        m_sortAscending = true;
        RefreshPlaylistUI();
        if (m_playlist.IsEmpty()) return false;
        SetWindowTextW(m_staticSong,
            (L"已加载 " + std::to_wstring(m_playlist.GetCount()) + L" 首歌曲").c_str());
        return true;
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
