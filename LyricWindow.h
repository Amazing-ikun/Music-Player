// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "LyricParser.h"

// ============================================
// 桌面歌词悬浮窗 (GDI+ 逐像素 alpha 渲染)
// ============================================
// 职责: 无边框、置顶、可透明的悬浮窗口, 显示当前歌词行(高亮)及下一行。
// 背景在鼠标悬停时显示半透明卡片, 鼠标移开 5 秒后淡出为透明, 仅保留歌词文字。
// 由 LyricsController 驱动: 切歌 SetLyrics() / 定时器 SetCurrentIndex();
// 用户操作(右键菜单/拖拽)经 LyricWindowListener 回调出去。
// ============================================

// 悬浮窗事件监听: 由驱动方(桌面歌词控制器)实现, 集中于一处处理用户操作
class LyricWindowListener {
public:
    virtual void OnHidden() = 0;                              // 右键「隐藏桌面歌词」
    virtual void OnFontSizeChanged(int first, int second) = 0; // 拖拽改变大小导致字号变化
    virtual void OnLockedChanged(bool locked) = 0;
    virtual void OnTranslationChanged(bool show) = 0;
    virtual void OnPrevNext(bool next) = 0;                   // true=下一首, false=上一首
protected:
    ~LyricWindowListener() = default;
};

class LyricWindow {
public:
    LyricWindow();
    ~LyricWindow();

    bool Create(HINSTANCE hInst);
    void Destroy();
    void Show();
    void Hide();
    bool Visible() const { return m_hwnd != NULL && m_visible; }
    HWND Hwnd() const { return m_hwnd; }

    void SetLyrics(const LyricParser& parser);
    void SetCurrentIndex(int index);
    void SetEmptyText(const std::wstring& text) { m_emptyText = text; }
    void SetColor(COLORREF color);        // 首行文字颜色
    void SetNextColor(COLORREF color);    // 次行文字颜色
    void SetFontSize(int px);             // 首行字号(像素)
    void SetSecondFontSize(int px);       // 次行字号(像素)

    // 用户操作事件统一回调 (须在窗口创建前设置; 监听方生命周期需覆盖窗口)
    void SetListener(LyricWindowListener* l) { m_listener = l; }

    // 锁定/译文状态 (供驱动方初始化)
    void SetLocked(bool locked);
    void SetShowTranslation(bool show);

private:
    bool ShouldShowBackground() const { return !m_locked && m_bgVisible; }
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
    void Redraw();
    void RecalcSize();
    void EnsureDib(int w, int h);
    void FreeDib();

    HWND m_hwnd;
    HINSTANCE m_hInst;
    bool m_visible;

    std::vector<LyricLine> m_lines;
    int m_currentIndex;               // -1 = 尚未开始
    std::wstring m_emptyText;         // 无歌词提示文案
    COLORREF m_color;                 // 首行颜色
    COLORREF m_nextColor;             // 次行颜色
    int m_fontSize;                   // 首行字号(像素)
    int m_secondFontSize;             // 次行字号(像素)

    bool m_bgVisible;                 // 背景卡片是否显示(鼠标悬停时显示)
    bool m_mouseTracking;             // 是否已在跟踪鼠标离开

    bool m_scrolling;                 // 当前行是否在横向滚动(走马灯)
    int m_scrollOffset;               // 滚动偏移(像素)
    int m_scrollMax;                  // 滚动最大偏移

    bool m_locked;                    // 锁定(仅歌词、不显示背景)
    bool m_showTranslation;           // 是否显示译文

    HBITMAP m_dib;
    void* m_dibBits;
    HDC m_dibDC;
    HBITMAP m_dibOldBmp;
    int m_dibW, m_dibH;

    LyricWindowListener* m_listener;
};
