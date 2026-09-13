// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#pragma once
#include <windows.h>
#include <map>
#include <string>

// 时长缓存条目: 记录文件大小与修改时间用于缓存失效判断
struct DurationCacheEntry {
    double    duration = 0.0;  // 秒
    ULONGLONG size = 0;
    FILETIME  mtime = {};
};

// ============================================
// 歌曲时长缓存 (Data\durations.mpdf, UTF-8)
// ============================================
// 职责: 按「文件大小 + 修改时间」判断缓存是否仍有效, 文件一旦变化即视为失效。
//       首次访问时惰性加载。
// ============================================
class DurationCache {
public:
    // 命中且文件未变时返回缓存时长(秒); 未知或已失效返回 0
    double Get(const std::wstring& path) const;
    // 记录一次探测结果并立即写盘 (无法读取文件属性时不写入)
    void Put(const std::wstring& path, double duration);

private:
    void EnsureLoaded() const;
    void Save() const;

    // 惰性加载: Get 为 const, 首次访问时填充缓存, 故二者均声明为 mutable
    mutable std::map<std::wstring, DurationCacheEntry> m_map;
    mutable bool m_loaded = false;
};
