#define UNICODE
#define _UNICODE
#include "Dialogs.h"
#include "Resource.h"
#include "Settings.h"
#include "ListeningHistory.h"
#include "PlaylistManager.h"
#include "Hotkey.h"
#include <commctrl.h>
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <cwctype>

namespace {
    std::wstring GetDisplayName(const std::wstring& path) {
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
}

// ---- 倍速输入对话框 ----

static LRESULT CALLBACK SpeedInputDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
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
            double* result = (double*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (result) *result = val;
            DestroyWindow(hDlg);
            return 0;
        }
        if (id == IDCANCEL) { DestroyWindow(hDlg); return 0; }
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

double ShowSpeedInputDialog(HINSTANCE hInst, HWND owner, double current) {
    const wchar_t DLG_CLASS[] = L"SpeedInputDlg";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = SpeedInputDlgProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = DLG_CLASS;
    if (!RegisterClassExW(&wc)) return -1.0;

    int dlgW = 280, dlgH = 140;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

    HWND hDlg = CreateWindowExW(0, DLG_CLASS, L"自定义倍速",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH, owner, NULL, hInst, NULL);
    if (!hDlg) return -1.0;

    double result = -1.0;
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)&result);

    CreateWindowExW(0, L"STATIC", L"输入倍速 (0.1 ~ 10.0):",
        WS_CHILD | WS_VISIBLE, 15, 15, 250, 20, hDlg, NULL, hInst, NULL);
    wchar_t cur[16];
    swprintf(cur, 16, L"%.2g", current);
    CreateWindowExW(0, L"EDIT", cur,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        15, 40, 250, 26, hDlg, (HMENU)500, hInst, NULL);
    SendMessageW(GetDlgItem(hDlg, 500), EM_SETLIMITTEXT, 6, 0);

    CreateWindowExW(0, L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW / 2 - 95, 80, 80, 28, hDlg, (HMENU)IDOK, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW / 2 + 15, 80, 80, 28, hDlg, (HMENU)IDCANCEL, hInst, NULL);

    EnableWindow(owner, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);

    UnregisterClassW(DLG_CLASS, hInst);
    return result;
}

// ---- 歌词设置对话框 ----

struct LyricsResult {
    bool ok;
    LyricsStyle style;
};

static LRESULT CALLBACK LyricsSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
    if (msg == WM_COMMAND) {
        int id = LOWORD(wp);
        if (id == IDOK) {
            LyricsResult* r = (LyricsResult*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (r) {
                wchar_t colorBuf[64];
                GetWindowTextW(GetDlgItem(hDlg, 501), colorBuf, 64);
                COLORREF color;
                if (!ParseColor(colorBuf, color)) {
                    MessageBoxW(hDlg, L"首行颜色格式无效，请输入 #RRGGBB 或 R,G,B", L"无效输入", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                wchar_t nextBuf[64];
                GetWindowTextW(GetDlgItem(hDlg, 503), nextBuf, 64);
                COLORREF nextColor;
                if (!ParseColor(nextBuf, nextColor)) {
                    MessageBoxW(hDlg, L"次行颜色格式无效，请输入 #RRGGBB 或 R,G,B", L"无效输入", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                wchar_t sizeBuf[16];
                GetWindowTextW(GetDlgItem(hDlg, 502), sizeBuf, 16);
                int fontSize = _wtoi(sizeBuf);
                if (fontSize < 12 || fontSize > 72) {
                    MessageBoxW(hDlg, L"首行字号需在 12 ~ 72 之间", L"无效输入", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                wchar_t sizeBuf2[16];
                GetWindowTextW(GetDlgItem(hDlg, 504), sizeBuf2, 16);
                int secondFontSize = _wtoi(sizeBuf2);
                if (secondFontSize < 12 || secondFontSize > 72) {
                    MessageBoxW(hDlg, L"次行字号需在 12 ~ 72 之间", L"无效输入", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                r->style.firstColor = color;
                r->style.secondColor = nextColor;
                r->style.firstFontSize = fontSize;
                r->style.secondFontSize = secondFontSize;
                r->ok = true;
            }
            DestroyWindow(hDlg);
            return 0;
        }
        if (id == IDCANCEL) { DestroyWindow(hDlg); return 0; }
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

bool ShowLyricsSettingsDialog(HINSTANCE hInst, HWND owner, LyricsStyle& style) {
    const wchar_t DLG_CLASS[] = L"LyricsSettingsDlg";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = LyricsSettingsDlgProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = DLG_CLASS;
    if (!RegisterClassExW(&wc)) return false;

    int dlgW = 400, dlgH = 270;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

    HWND hDlg = CreateWindowExW(0, DLG_CLASS, L"歌词设置",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH, owner, NULL, hInst, NULL);
    if (!hDlg) return false;

    LyricsResult result = { false, style };
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)&result);

    CreateWindowExW(0, L"STATIC", L"颜色格式: #RRGGBB 或 R,G,B",
        WS_CHILD | WS_VISIBLE, 15, 12, 370, 18, hDlg, NULL, hInst, NULL);

    CreateWindowExW(0, L"STATIC", L"首行颜色:",
        WS_CHILD | WS_VISIBLE, 15, 42, 150, 18, hDlg, NULL, hInst, NULL);
    std::wstring hexCur = ColorToHex(style.firstColor);
    CreateWindowExW(0, L"EDIT", hexCur.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        165, 40, 220, 26, hDlg, (HMENU)501, hInst, NULL);

    CreateWindowExW(0, L"STATIC", L"次行颜色:",
        WS_CHILD | WS_VISIBLE, 15, 76, 150, 18, hDlg, NULL, hInst, NULL);
    std::wstring hexNext = ColorToHex(style.secondColor);
    CreateWindowExW(0, L"EDIT", hexNext.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        165, 74, 220, 26, hDlg, (HMENU)503, hInst, NULL);

    CreateWindowExW(0, L"STATIC", L"首行字号 (12 ~ 72):",
        WS_CHILD | WS_VISIBLE, 15, 110, 150, 18, hDlg, NULL, hInst, NULL);
    wchar_t szBuf[16];
    swprintf(szBuf, 16, L"%d", style.firstFontSize);
    CreateWindowExW(0, L"EDIT", szBuf,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        165, 108, 220, 26, hDlg, (HMENU)502, hInst, NULL);

    CreateWindowExW(0, L"STATIC", L"次行字号 (12 ~ 72):",
        WS_CHILD | WS_VISIBLE, 15, 144, 150, 18, hDlg, NULL, hInst, NULL);
    wchar_t szBuf2[16];
    swprintf(szBuf2, 16, L"%d", style.secondFontSize);
    CreateWindowExW(0, L"EDIT", szBuf2,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        165, 142, 220, 26, hDlg, (HMENU)504, hInst, NULL);

    CreateWindowExW(0, L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW / 2 - 95, 184, 80, 28, hDlg, (HMENU)IDOK, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW / 2 + 15, 184, 80, 28, hDlg, (HMENU)IDCANCEL, hInst, NULL);

    EnableWindow(owner, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);

    UnregisterClassW(DLG_CLASS, hInst);
    if (result.ok) style = result.style;
    return result.ok;
}

// ---- 关于对话框 ----

namespace {
    const int ABOUT_MIN_W = 420;
    const int ABOUT_MIN_H = 380;
    const int ABOUT_MAX_W = 800;
    const int ABOUT_MAX_H = 640;
}

struct AboutCtx {
    HWND hDlg;
    HFONT hGuiFont;
    HFONT hTitleFont;
    HWND hTitle;
    HWND hVersion;
    HWND hChangelogLabel;
    HWND hChangelog;
    HWND hClose;
    int lineH;
};

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
    if (msg == WM_SIZE && ctx) { LayoutAboutControls(ctx); return 0; }
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
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
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) { DestroyWindow(hDlg); return 0; }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

void ShowAboutDialog(HINSTANCE hInst, HWND owner, const wchar_t* version, const wchar_t* changelog) {
    const wchar_t ABOUT_CLASS[] = L"AboutWindow";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = AboutDlgProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = ABOUT_CLASS;
    RegisterClassExW(&wc);

    int dlgW = ABOUT_MIN_W, dlgH = ABOUT_MIN_H;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

    HWND hDlg = CreateWindowExW(0, ABOUT_CLASS, L"关于 MusicPlayer",
        WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX,
        x, y, dlgW, dlgH, owner, NULL, hInst, NULL);
    if (!hDlg) return;

    AboutCtx* ctx = new AboutCtx{};
    ctx->hDlg = hDlg;
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

    ctx->hGuiFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    ctx->hTitleFont = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

    HDC hdc = GetDC(hDlg);
    HFONT hOldFont = (HFONT)SelectObject(hdc, ctx->hGuiFont);
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    SelectObject(hdc, hOldFont);
    ReleaseDC(hDlg, hdc);
    ctx->lineH = tm.tmHeight;

    ctx->hTitle = CreateWindowExW(0, L"STATIC", L"MusicPlayer",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hDlg, NULL, hInst, NULL);
    SendMessageW(ctx->hTitle, WM_SETFONT, (WPARAM)ctx->hTitleFont, TRUE);

    wchar_t verBuf[64];
    swprintf(verBuf, 64, L"版本: %ls", version);
    ctx->hVersion = CreateWindowExW(0, L"STATIC", verBuf,
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hDlg, NULL, hInst, NULL);
    SendMessageW(ctx->hVersion, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);

    ctx->hChangelogLabel = CreateWindowExW(0, L"STATIC", L"更新历史:",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hDlg, NULL, hInst, NULL);
    SendMessageW(ctx->hChangelogLabel, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);

    ctx->hChangelog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", changelog,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
        ES_AUTOVSCROLL | WS_VSCROLL | ES_LEFT,
        0, 0, 0, 0, hDlg, NULL, hInst, NULL);
    SendMessageW(ctx->hChangelog, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);
    SendMessageW(ctx->hChangelog, EM_SETSEL, 0, 0);

    ctx->hClose = CreateWindowExW(0, L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hDlg, (HMENU)IDOK, hInst, NULL);
    SendMessageW(ctx->hClose, WM_SETFONT, (WPARAM)ctx->hGuiFont, TRUE);

    LayoutAboutControls(ctx);
    ShowWindow(hDlg, SW_SHOW);
}

// ---- 统计窗口 + 导出 ----

struct StatsDlgCtx {
    StatsData data;
    HINSTANCE hInst;
    HWND hDlg;
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
    bool dtpGuard;
};

struct ExportCtx {
    HWND hDlg;
    HWND hRadioCsv, hRadioJson;
    HWND hChkDaily, hChkWeekly, hChkTop, hChkTotal;
    bool asCsv;
    bool includeDaily, includeWeekly, includeTopSongs, includeTotal;
    int result;
};

static void FormatListenTime(double seconds, wchar_t* buf, int bufLen) {
    int total = (int)seconds;
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    if (h > 0) swprintf(buf, bufLen, L"%d小时%d分%d秒", h, m, s);
    else if (m > 0) swprintf(buf, bufLen, L"%d分%d秒", m, s);
    else swprintf(buf, bufLen, L"%d秒", s);
}

static void ComputeStatsRange(const StatsDlgCtx* ctx, std::string& from, std::string& to) {
    time_t now_t = time(NULL);
    struct tm tm_now = *localtime(&now_t);
    char fromBuf[32], toBuf[32];
    if (ctx->useAllRange) {
        std::string earliest = ctx->data.history->GetEarliestDate();
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

static SYSTEMTIME DateFromNow(int days) {
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);
    tm.tm_mday += days;
    tm.tm_isdst = -1;
    mktime(&tm);
    SYSTEMTIME st = {};
    st.wYear  = tm.tm_year + 1900;
    st.wMonth = tm.tm_mon + 1;
    st.wDay   = tm.tm_mday;
    return st;
}

static void SyncDtpToRange(StatsDlgCtx* c) {
    if (!c->hDtpStart || !c->hDtpEnd) return;
    SYSTEMTIME endSt = DateFromNow(0);
    SYSTEMTIME startSt = DateFromNow(-c->rangeDays);
    c->dtpGuard = true;
    SendMessageW(c->hDtpStart, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&startSt);
    SendMessageW(c->hDtpEnd,   DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&endSt);
    c->dtpGuard = false;
}

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

static void RefreshStatsDisplay(StatsDlgCtx* ctx) {
    std::string fromDate, toDate;
    ComputeStatsRange(ctx, fromDate, toDate);

    auto daily = ctx->data.history->GetDailyRecords(fromDate, toDate);
    double total = ctx->data.history->GetTotalSeconds(fromDate, toDate);
    auto weekly = ctx->data.history->GetWeeklySummaries(fromDate, toDate);

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

    ListView_DeleteAllItems(ctx->hPlayCountList);
    const auto& playCount = *ctx->data.playCount;
    std::vector<std::pair<std::wstring, int>> sorted(playCount.begin(), playCount.end());
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

static void DoExportStats(StatsDlgCtx* statsCtx, bool asCsv,
                          bool includeDaily, bool includeWeekly,
                          bool includeTopSongs, bool includeTotal) {
    std::string fromDate, toDate;
    ComputeStatsRange(statsCtx, fromDate, toDate);

    StatsExportData data;
    data.dailyRecords = statsCtx->data.history->GetDailyRecords(fromDate, toDate);
    data.weeklyRecords = statsCtx->data.history->GetWeeklySummaries(fromDate, toDate);
    data.totalSeconds = statsCtx->data.history->GetTotalSeconds(fromDate, toDate);

    const auto& playCount = *statsCtx->data.playCount;
    std::vector<std::pair<std::wstring, int>> sorted(playCount.begin(), playCount.end());
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
    const BYTE bomUtf8[] = { 0xEF, 0xBB, 0xBF };
    WriteFile(hFile, bomUtf8, 3, &written, NULL);
    WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(hFile);

    std::wstring msg = L"统计数据已导出到:\n" + std::wstring(filePath);
    MessageBoxW(statsCtx->hDlg, msg.c_str(), L"导出完成", MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK ExportStatsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
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
        if (id == IDCANCEL) { DestroyWindow(hDlg); return 0; }
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

static void ShowExportStatsDialog(HINSTANCE hInst, StatsDlgCtx* statsCtx) {
    const wchar_t DLG_CLASS[] = L"ExportStatsDlg";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = ExportStatsDlgProc;
    wc.hInstance     = hInst;
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
        x, y, dlgW, dlgH, statsCtx->hDlg, NULL, hInst, NULL);
    if (!hDlg) { UnregisterClassW(DLG_CLASS, hInst); return; }

    ExportCtx* ctx = new ExportCtx();
    ctx->hDlg = hDlg;
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

    CreateWindowExW(0, L"BUTTON", L"导出格式",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        15, 12, dlgW - 30, 52, hDlg, NULL, hInst, NULL);
    ctx->hRadioCsv = CreateWindowExW(0, L"BUTTON", L"CSV (Excel)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
        30, 32, 130, 22, hDlg, (HMENU)600, hInst, NULL);
    ctx->hRadioJson = CreateWindowExW(0, L"BUTTON", L"JSON",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP,
        170, 32, 100, 22, hDlg, (HMENU)601, hInst, NULL);
    SendMessageW(ctx->hRadioCsv, BM_SETCHECK, BST_CHECKED, 0);

    CreateWindowExW(0, L"BUTTON", L"导出内容",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        15, 72, dlgW - 30, 140, hDlg, NULL, hInst, NULL);
    ctx->hChkDaily = CreateWindowExW(0, L"BUTTON", L"每日听歌记录 (日期/星期/时长)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        30, 92, dlgW - 60, 22, hDlg, (HMENU)602, hInst, NULL);
    ctx->hChkWeekly = CreateWindowExW(0, L"BUTTON", L"每周统计 (周次/日期范围/时长)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        30, 118, dlgW - 60, 22, hDlg, (HMENU)603, hInst, NULL);
    ctx->hChkTop = CreateWindowExW(0, L"BUTTON", L"常听歌曲排行 (前 20)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        30, 144, dlgW - 60, 22, hDlg, (HMENU)604, hInst, NULL);
    ctx->hChkTotal = CreateWindowExW(0, L"BUTTON", L"总计",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        30, 170, dlgW - 60, 22, hDlg, (HMENU)605, hInst, NULL);
    SendMessageW(ctx->hChkDaily, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(ctx->hChkWeekly, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(ctx->hChkTop, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(ctx->hChkTotal, BM_SETCHECK, BST_CHECKED, 0);

    CreateWindowExW(0, L"BUTTON", L"导出",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW / 2 - 115, 224, 90, 30, hDlg, (HMENU)IDOK, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW / 2 + 25, 224, 90, 30, hDlg, (HMENU)IDCANCEL, hInst, NULL);

    HWND hOwner = GetWindow(statsCtx->hDlg, GW_OWNER);
    EnableWindow(hOwner, FALSE);
    EnableWindow(statsCtx->hDlg, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(statsCtx->hDlg, TRUE);
    EnableWindow(hOwner, TRUE);
    SetForegroundWindow(statsCtx->hDlg);

    if (ctx->result == 1) {
        DoExportStats(statsCtx, ctx->asCsv,
            ctx->includeDaily, ctx->includeWeekly, ctx->includeTopSongs, ctx->includeTotal);
    }
    delete ctx;
    UnregisterClassW(DLG_CLASS, hInst);
}

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
        if (c && c->baseH > 0) LayoutStatsControls(c, LOWORD(lp), HIWORD(lp));
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(hDlg); return 0; }
    if (msg == WM_DESTROY) {
        StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (c) {
            HWND hOwner = GetWindow(hDlg, GW_OWNER);
            delete c;
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
            if (hOwner && IsWindow(hOwner)) PostMessage(hOwner, WM_APP_BRING_TO_TOP, 0, 0);
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
            if (SendMessageW(nm->hwndFrom, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) return 0;
            if (nm->idFrom == 310) c->calStart = st;
            else c->calEnd = st;
            c->useAllRange = false;
            c->useCalendarRange = true;
            ValidateCustomRange(c, hDlg);
            SendMessageW(c->hRadioAll, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(c->hRadio7, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(c->hRadio30, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(c->hRadioCustom, BM_SETCHECK, BST_CHECKED, 0);
            RefreshStatsDisplay(c);
            return 0;
        }
        return DefWindowProcW(hDlg, msg, wp, lp);
    }
    if (msg == WM_COMMAND) {
        StatsDlgCtx* c = (StatsDlgCtx*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (!c) return DefWindowProcW(hDlg, msg, wp, lp);
        int id = LOWORD(wp);
        if (id == IDCANCEL) { DestroyWindow(hDlg); return 0; }

        if (id == 304) {
            if (SendMessageW(c->hRadioAll, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                c->useAllRange = true;
                c->useCalendarRange = false;
            } else if (SendMessageW(c->hRadio7, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                c->useAllRange = false;
                c->useCalendarRange = false;
                c->rangeDays = 7;
            } else if (SendMessageW(c->hRadio30, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                c->useAllRange = false;
                c->useCalendarRange = false;
                c->rangeDays = 30;
            } else {
                SYSTEMTIME st;
                if (c->hDtpStart && SendMessageW(c->hDtpStart, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
                    c->calStart = st;
                if (c->hDtpEnd && SendMessageW(c->hDtpEnd, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
                    c->calEnd = st;
                c->useAllRange = false;
                c->useCalendarRange = true;
                ValidateCustomRange(c, hDlg);
            }
            RefreshStatsDisplay(c);
            return 0;
        }
        if (id == 300) { c->useAllRange = false; c->useCalendarRange = false; c->rangeDays = 7; SyncDtpToRange(c); RefreshStatsDisplay(c); return 0; }
        if (id == 301) { c->useAllRange = false; c->useCalendarRange = false; c->rangeDays = 30; SyncDtpToRange(c); RefreshStatsDisplay(c); return 0; }
        if (id == 302) { c->useAllRange = false; c->useCalendarRange = true; RefreshStatsDisplay(c); return 0; }
        if (id == 303) { c->useAllRange = true; c->useCalendarRange = false; RefreshStatsDisplay(c); return 0; }
        if (id == 305) { ShowExportStatsDialog(c->hInst, c); return 0; }
        return 0;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

void ShowStatsWindow(HINSTANCE hInst, HWND owner, const StatsData& data) {
    const wchar_t STATS_CLASS[] = L"StatsWindow";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = StatsDlgProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = STATS_CLASS;
    RegisterClassExW(&wc);

    int dlgW = 660, dlgH = 620;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

    HWND hDlg = CreateWindowExW(0, STATS_CLASS, L"听歌时长统计",
        WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_VISIBLE,
        x, y, dlgW, dlgH, owner, NULL, hInst, NULL);
    if (!hDlg) return;

    StatsDlgCtx* ctx = new StatsDlgCtx();
    ctx->data = data;
    ctx->hInst = hInst;
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

    CreateWindowExW(0, L"BUTTON", L"统计范围",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, yPos, dlgW - 20, 50, hDlg, NULL, hInst, NULL);

    ctx->hRadioAll = CreateWindowExW(0, L"BUTTON", L"所有",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        25, rY, 55, 22, hDlg, (HMENU)303, hInst, NULL);
    SendMessageW(ctx->hRadioAll, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    ctx->hRadio7 = CreateWindowExW(0, L"BUTTON", L"7天",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        85, rY, 55, 22, hDlg, (HMENU)300, hInst, NULL);
    SendMessageW(ctx->hRadio7, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    ctx->hRadio30 = CreateWindowExW(0, L"BUTTON", L"30天",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        145, rY, 60, 22, hDlg, (HMENU)301, hInst, NULL);
    SendMessageW(ctx->hRadio30, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
    SendMessageW(ctx->hRadio30, BM_SETCHECK, BST_CHECKED, 0);

    ctx->hRadioCustom = CreateWindowExW(0, L"BUTTON", L"自定义",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        210, rY, 65, 22, hDlg, (HMENU)302, hInst, NULL);
    SendMessageW(ctx->hRadioCustom, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    yPos += 62;
    int dtpY = yPos;

    CreateWindowExW(0, L"STATIC", L"起始:",
        WS_CHILD | WS_VISIBLE, 15, dtpY + 3, 35, 20, hDlg, (HMENU)312, hInst, NULL);
    ctx->hDtpStart = CreateWindowExW(0, DATETIMEPICK_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
        50, dtpY, 130, 26, hDlg, (HMENU)310, hInst, NULL);
    SendMessageW(ctx->hDtpStart, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    CreateWindowExW(0, L"STATIC", L"终止:",
        WS_CHILD | WS_VISIBLE, 200, dtpY + 3, 35, 20, hDlg, (HMENU)313, hInst, NULL);
    ctx->hDtpEnd = CreateWindowExW(0, DATETIMEPICK_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
        235, dtpY, 130, 26, hDlg, (HMENU)311, hInst, NULL);
    SendMessageW(ctx->hDtpEnd, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    CreateWindowExW(0, L"BUTTON", L"刷新",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW - 100, dtpY - 2, 80, 28, hDlg, (HMENU)304, hInst, NULL);
    SendMessageW(GetDlgItem(hDlg, 304), WM_SETFONT, (WPARAM)hGuiFont, TRUE);

    SYSTEMTIME todayDtp;
    GetLocalTime(&todayDtp);
    SendMessageW(ctx->hDtpStart, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&todayDtp);
    SendMessageW(ctx->hDtpEnd, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&todayDtp);
    ctx->calStart = todayDtp;
    ctx->calEnd = todayDtp;

    yPos = dtpY + 34;

    ctx->hLabelDay = CreateWindowExW(0, L"STATIC", L"每日详情",
        WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, hInst, NULL);

    yPos += 20;
    ctx->hDayList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
        15, yPos, dlgW - 30, 130, hDlg, NULL, hInst, NULL);
    ListView_SetExtendedListViewStyle(ctx->hDayList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

    LVCOLUMNW lc = {};
    lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lc.fmt  = LVCFMT_LEFT;
    lc.cx = 120; lc.pszText = (LPWSTR)L"日期"; ListView_InsertColumn(ctx->hDayList, 0, &lc);
    lc.cx = 70;  lc.pszText = (LPWSTR)L"星期"; ListView_InsertColumn(ctx->hDayList, 1, &lc);
    lc.cx = 150; lc.pszText = (LPWSTR)L"听歌时长"; ListView_InsertColumn(ctx->hDayList, 2, &lc);

    yPos += 138;

    ctx->hLabelWeek = CreateWindowExW(0, L"STATIC", L"每周统计",
        WS_CHILD | WS_VISIBLE, 15, yPos, 100, 20, hDlg, NULL, hInst, NULL);

    yPos += 20;
    ctx->hWeekList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
        15, yPos, dlgW - 30, 70, hDlg, NULL, hInst, NULL);
    ListView_SetExtendedListViewStyle(ctx->hWeekList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

    lc.cx = 100; lc.pszText = (LPWSTR)L"周"; ListView_InsertColumn(ctx->hWeekList, 0, &lc);
    lc.cx = 140; lc.pszText = (LPWSTR)L"日期范围"; ListView_InsertColumn(ctx->hWeekList, 1, &lc);
    lc.cx = 150; lc.pszText = (LPWSTR)L"累计时长"; ListView_InsertColumn(ctx->hWeekList, 2, &lc);

    yPos += 78;

    ctx->hLabelPlay = CreateWindowExW(0, L"STATIC", L"我常听的",
        WS_CHILD | WS_VISIBLE, 15, yPos, 180, 20, hDlg, NULL, hInst, NULL);

    yPos += 20;
    ctx->hPlayCountList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
        15, yPos, dlgW - 30, 90, hDlg, NULL, hInst, NULL);
    ListView_SetExtendedListViewStyle(ctx->hPlayCountList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

    lc.cx = 40;  lc.pszText = (LPWSTR)L"#";   ListView_InsertColumn(ctx->hPlayCountList, 0, &lc);
    lc.cx = 250; lc.pszText = (LPWSTR)L"歌曲"; ListView_InsertColumn(ctx->hPlayCountList, 1, &lc);
    lc.cx = 80;  lc.pszText = (LPWSTR)L"次数"; ListView_InsertColumn(ctx->hPlayCountList, 2, &lc);

    yPos += 98;

    ctx->hTotalText = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        15, yPos, dlgW - 30, 22, hDlg, (HMENU)400, hInst, NULL);
    SendMessageW(ctx->hTotalText, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    yPos += 28;
    CreateWindowExW(0, L"BUTTON", L"导出数据...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW / 2 - 120, yPos, 120, 28, hDlg, (HMENU)305, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"关闭",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW / 2 + 20, yPos, 90, 28, hDlg, (HMENU)IDCANCEL, hInst, NULL);

    RefreshStatsDisplay(ctx);

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

    {
        RECT cr;
        GetClientRect(hDlg, &cr);
        LayoutStatsControls(ctx, cr.right, cr.bottom);
    }

    DeleteObject(hBoldFont);
    DeleteObject(hGuiFont);
}

// ---- 快捷键对话框 ----

struct HKDlgCtx {
    HotkeyBinding* bindings;
    int recording;
    int count;
    int result;
};

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

bool ShowHotkeyDialog(HINSTANCE hInst, HWND owner, HotkeyBinding* bindings, int count) {
    const wchar_t DLG_CLASS[] = L"HotkeyConfigDlg";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = HotkeyDlgProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = DLG_CLASS;
    if (!RegisterClassExW(&wc)) return false;

    int dlgW = 460, dlgH = 350;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - dlgW) / 2, y = (sh - dlgH) / 2;

    HWND hDlg = CreateWindowExW(0, DLG_CLASS, L"配置快捷键",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH, owner, NULL, hInst, NULL);
    if (!hDlg) return false;

    HKDlgCtx* ctx = new HKDlgCtx{ bindings, -1, count, 0 };
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);

    HFONT hGuiFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);

    int rowY = 20;
    for (int i = 0; i < count; i++) {
        wchar_t label[64];
        swprintf(label, 64, L"%ls:", bindings[i].actionName);
        CreateWindowExW(0, L"STATIC", label,
            WS_CHILD | WS_VISIBLE, 20, rowY, 140, 24, hDlg, NULL, hInst, NULL);

        std::wstring keyText = HotkeyToString(bindings[i].vk, bindings[i].mod);
        HWND hKey = CreateWindowExW(0, L"STATIC", keyText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_SUNKEN,
            170, rowY, 200, 24, hDlg, (HMENU)(size_t)(100 + i), hInst, NULL);
        SendMessageW(hKey, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        CreateWindowExW(0, L"BUTTON", L"更改",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, rowY, 60, 24, hDlg, (HMENU)(size_t)(200 + i), hInst, NULL);
        rowY += 32;
    }

    CreateWindowExW(0, L"STATIC", L"提示: 点击\"更改\"后按下新的按键组合",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, rowY + 10, dlgW - 40, 20, hDlg, NULL, hInst, NULL);

    CreateWindowExW(0, L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW / 2 - 110, rowY + 40, 90, 28, hDlg, (HMENU)(size_t)IDOK, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW / 2 + 20, rowY + 40, 90, 28, hDlg, (HMENU)(size_t)IDCANCEL, hInst, NULL);

    EnableWindow(owner, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
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
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);

    bool ok = (ctx->result == 1);
    delete ctx;
    DeleteObject(hGuiFont);
    UnregisterClassW(DLG_CLASS, hInst);
    return ok;
}
