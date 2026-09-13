// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#pragma once
#include <string>
#include <vector>

// ============================================
// LRC 歌词解析
// ============================================
// 职责: 读取本地 .lrc 歌词文件 (自动识别 UTF-8 / UTF-16 / GBK 编码),
//       解析出带时间戳的歌词时间轴。本模块只负责「读 + 解析」, 不涉及 UI。
//
// 后续模块的对接方式:
//   悬浮窗口: 用 Lines() 取全部歌词行, FindIndex() 定位当前行, Title()/Artist() 等取元信息;
//   同步:     每次定时器回调里用 FindIndex(GetPosition()) 得到当前行下标;
//   开关/设置: 切歌时 Clear() 后 LoadFile(FindLrcFile(当前音频路径)), 用 IsLoaded() 决定是否显示。
// ============================================

// 单行同步歌词
struct LyricLine {
    double timeMs = 0.0;   // 时间戳 (毫秒)
    std::wstring text;     // 歌词文本 (可为空, 表示纯间奏)
};

class LyricParser {
public:
    // 从 .lrc 文件加载并解析; 成功且至少解析出一行同步歌词返回 true
    bool LoadFile(const std::wstring& lrcPath);

    // 是否已加载有效歌词 (供开关模块决定是否显示悬浮窗)
    bool IsLoaded() const { return !m_lines.empty(); }

    // 元信息标签 ([ti:] [ar:] [al:])
    const std::wstring& Title()  const { return m_title; }
    const std::wstring& Artist() const { return m_artist; }
    const std::wstring& Album()  const { return m_album; }

    // [offset:] 偏移 (秒)。正值表示歌词整体延后显示; 已由 FindIndex 内部应用
    double OffsetSeconds() const { return m_offsetMs / 1000.0; }

    // 播放位置(秒) → 当前歌词行下标 (最后一个时间戳 <= 位置的同步行);
    // 未加载或位置早于首行返回 -1
    int FindIndex(double positionSeconds) const;

    // 全部同步歌词行 (已按时间升序), 供悬浮窗口渲染当前行及上下文
    const std::vector<LyricLine>& Lines() const { return m_lines; }

    void Clear();

private:
    bool Parse(const std::wstring& text);

    std::vector<LyricLine> m_lines;
    std::wstring m_title, m_artist, m_album;
    double m_offsetMs = 0.0;   // [offset:] 标签, 单位毫秒
};

// 由音频文件路径推导同目录同名的 .lrc 路径; 文件不存在返回空串
std::wstring FindLrcFile(const std::wstring& audioPath);
