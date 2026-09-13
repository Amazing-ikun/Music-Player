#pragma once
#include <windows.h>
#include <string>

// ============================================
// 设置持久化 (Data\settings.mpdf)
// ============================================
// 职责: 统一管理所有标量设置的读取与写入, 供 MainWindow 加载后应用到
//       音频引擎 / 歌词悬浮窗等模块。
// ============================================

// 解析颜色: 支持 #RRGGBB / RRGGBB / R,G,B / R G B; 失败返回 false
bool ParseColor(const std::wstring& input, COLORREF& out);
// COLORREF → "#RRGGBB" 十六进制字符串
std::wstring ColorToHex(COLORREF c);

struct Settings {
    // ---- 通用 ----
    int  autoplay = 1;             // 0=不进行操作, 1=自动播放
    bool rememberProgress = true;
    bool trayMinimize = true;
    bool balanceEnabled = true;
    int  playMode = 0;             // 0=顺序 1=单曲 2=随机
    double playSpeed = 1.0;

    // ---- 桌面歌词 ----
    bool lyricsShow = false;
    bool lyricsLocked = false;
    bool lyricsTranslation = false;
    COLORREF lyricsColor = RGB(255, 255, 255);        // 首行颜色
    COLORREF lyricsNextColor = RGB(150, 150, 150);    // 次行颜色
    int lyricsFontSize = 30;                          // 首行字号
    int lyricsSecondFontSize = 20;                    // 次行字号

    void Load(const std::wstring& path);
    void Save(const std::wstring& path) const;
};
