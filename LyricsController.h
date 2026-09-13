// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include <string>
#include "LyricParser.h"
#include "LyricWindow.h"

struct Settings;

// ============================================
// 桌面歌词编排
// ============================================
// 职责: 把 LyricParser(解析) + LyricWindow(悬浮窗) + 歌曲→歌词映射表 编排在一起,
//       对外只暴露「切歌/定时器/显示开关/匹配/样式设置」几个动作, 供 MainWindow 调用。
//       顺带实现 LyricWindowListener, 把悬浮窗的用户操作转发给宿主。
// 依赖: 宿主通过 LyricsHost 回调提供当前歌曲/播放位置/上一首下一首/持久化等能力,
//       控制器本身不依赖 MainWindow 或 AudioEngine。
// ============================================

// 宿主(MainWindow)需提供的外部能力; 未设置的回调按空操作处理
struct LyricsHost {
    HINSTANCE hInst = nullptr;                          // 创建悬浮窗 / 样式对话框
    HWND      owner = nullptr;                          // 消息框、文件选择框的父窗口
    std::function<std::wstring()>    currentSongPath;   // 当前歌曲全路径, 无歌返回空串
    std::function<double()>          playbackPosition;  // 当前播放位置(秒)
    std::function<void(bool)>        navigate;          // true=下一首, false=上一首
    std::function<void()>            persistSettings;   // 歌词设置变化后落盘
    std::function<void()>            visibilityChanged; // 显示状态变化 → 同步按钮/菜单
    std::function<int()>             songCount;         // 歌单歌曲数 (匹配扫描)
    std::function<std::wstring(int)> songPathAt;        // 歌单第 i 首路径 (匹配扫描)
};

class LyricsController : public LyricWindowListener {
public:
    ~LyricsController();

    // 绑定设置对象与宿主能力, 并把当前歌词设置应用到悬浮窗
    void Init(Settings* settings, LyricsHost host);
    void Destroy();

    void LoadMap();                 // 读取 Data\lyrics_map.mpdf
    void LoadForCurrentSong();      // 切歌: 加载当前歌曲歌词并刷新悬浮窗
    void Tick();                    // 定时器: 按播放位置刷新高亮行
    void SetVisible(bool on);
    bool Visible() const { return m_window.Visible(); }

    void MatchAll();                // 扫描报告: 统计能同目录找到歌词的歌曲数
    void MatchForSong(int songIdx); // 手动为单首歌曲选择 .lrc
    void ShowSettingsDialog();      // 颜色/字号设置对话框

    // LyricWindowListener
    void OnHidden() override;
    void OnFontSizeChanged(int first, int second) override;
    void OnLockedChanged(bool locked) override;
    void OnTranslationChanged(bool show) override;
    void OnPrevNext(bool next) override;

private:
    void ShowWindowInternal();
    void ApplyStyle();
    void SaveMap();
    double CurrentPosition() const;

    Settings*  m_settings = nullptr;
    LyricsHost m_host;
    LyricParser m_parser;
    LyricWindow m_window;
    std::map<std::wstring, std::wstring> m_map;  // 歌曲路径 → lrc 路径
};
