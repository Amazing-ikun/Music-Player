#include "DurationCache.h"
#include "TextFile.h"
#include <cstdio>
#include <cstdlib>

namespace {

bool ReadFileInfo(const std::wstring& path, ULONGLONG& size, FILETIME& mtime) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return false;
    size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    mtime = fad.ftLastWriteTime;
    return true;
}

}  // namespace

void DurationCache::EnsureLoaded() const {
    if (m_loaded) return;
    m_loaded = true;
    // 格式: duration<tab>size<tab>timeHigh<tab>timeLow<tab>path(UTF-8)
    for (const std::wstring& line : ReadLinesAuto(DataFile(L"durations"))) {
        size_t t1 = line.find(L'\t');
        if (t1 == std::wstring::npos) continue;
        size_t t2 = line.find(L'\t', t1 + 1);
        if (t2 == std::wstring::npos) continue;
        size_t t3 = line.find(L'\t', t2 + 1);
        if (t3 == std::wstring::npos) continue;
        size_t t4 = line.find(L'\t', t3 + 1);
        if (t4 == std::wstring::npos) continue;

        DurationCacheEntry e;
        e.duration = wcstod(line.substr(0, t1).c_str(), NULL);
        e.size = wcstoull(line.substr(t1 + 1, t2 - t1 - 1).c_str(), NULL, 10);
        e.mtime.dwHighDateTime = (DWORD)wcstoul(line.substr(t2 + 1, t3 - t2 - 1).c_str(), NULL, 10);
        e.mtime.dwLowDateTime  = (DWORD)wcstoul(line.substr(t3 + 1, t4 - t3 - 1).c_str(), NULL, 10);
        std::wstring path = line.substr(t4 + 1);
        if (!path.empty()) m_map[path] = e;
    }
}

double DurationCache::Get(const std::wstring& path) const {
    EnsureLoaded();
    auto it = m_map.find(path);
    if (it == m_map.end() || it->second.duration <= 0) return 0.0;

    ULONGLONG size = 0;
    FILETIME mtime = {};
    if (!ReadFileInfo(path, size, mtime)) return 0.0;
    if (it->second.size != size ||
        it->second.mtime.dwHighDateTime != mtime.dwHighDateTime ||
        it->second.mtime.dwLowDateTime != mtime.dwLowDateTime) {
        return 0.0;
    }
    return it->second.duration;
}

void DurationCache::Put(const std::wstring& path, double duration) {
    EnsureLoaded();
    ULONGLONG size = 0;
    FILETIME mtime = {};
    if (!ReadFileInfo(path, size, mtime)) return;

    DurationCacheEntry e;
    e.duration = duration;
    e.size = size;
    e.mtime = mtime;
    m_map[path] = e;
    Save();
}

void DurationCache::Save() const {
    std::wstring text;
    wchar_t tmp[128];
    for (const auto& kv : m_map) {
        swprintf(tmp, 128, L"%.2f\t%llu\t%lu\t%lu\t",
                 kv.second.duration,
                 (unsigned long long)kv.second.size,
                 (unsigned long)kv.second.mtime.dwHighDateTime,
                 (unsigned long)kv.second.mtime.dwLowDateTime);
        text += tmp;
        text += kv.first;
        text += L"\n";
    }
    WriteTextUtf8(DataFile(L"durations"), text);
}
