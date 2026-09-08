#pragma once
#include <windows.h>
#include <map>
#include <string>

// ============================================
// 对话框模块
// ============================================
// 职责: 封装各对话框, 通过参数/返回值与调用方交互, 不再反向依赖 MainWindow。
// ============================================

class ListeningHistory;
struct HotkeyBinding;

// 歌词样式 (歌词设置对话框的结果)
struct LyricsStyle {
    COLORREF firstColor;
    COLORREF secondColor;
    int firstFontSize;
    int secondFontSize;
};

// 统计窗口的数据源 (只读)
struct StatsData {
    ListeningHistory* history;
    const std::map<std::wstring, int>* playCount;
};

// 倍速输入对话框: 返回输入值, 取消返回 -1
double ShowSpeedInputDialog(HINSTANCE hInst, HWND owner, double current);

// 歌词设置对话框: 返回 true 表示确定 (style 为 in/out), false 表示取消
bool ShowLyricsSettingsDialog(HINSTANCE hInst, HWND owner, LyricsStyle& style);

// 关于对话框 (无模式)
void ShowAboutDialog(HINSTANCE hInst, HWND owner, const wchar_t* version, const wchar_t* changelog);

// 统计窗口 (无模式)
void ShowStatsWindow(HINSTANCE hInst, HWND owner, const StatsData& data);

// 快捷键对话框: 返回 true 表示确定 (bindings 为 in/out), false 表示取消
bool ShowHotkeyDialog(HINSTANCE hInst, HWND owner, HotkeyBinding* bindings, int count);
