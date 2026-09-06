#include "LyricParser.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <cstdlib>

namespace {

// 读取文件原始字节; 失败返回 false
bool ReadFileBytes(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER li;
    if (!GetFileSizeEx(h, &li) || li.QuadPart < 0) { CloseHandle(h); return false; }
    size_t size = (size_t)li.QuadPart;

    out.resize(size);
    DWORD read = 0;
    bool ok = (size == 0) || (ReadFile(h, &out[0], (DWORD)size, &read, NULL) && read == size);
    CloseHandle(h);
    if (!ok) out.clear();
    return ok;
}

// 将字节按 BOM / UTF-8 / 系统 ANSI(GBK) 自动识别解码为宽字符串
std::wstring DecodeBytes(const std::string& b) {
    if (b.empty()) return L"";
    const unsigned char* u = reinterpret_cast<const unsigned char*>(b.data());
    size_t n = b.size();

    // UTF-8 BOM
    if (n >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) {
        int len = MultiByteToWideChar(CP_UTF8, 0, b.data() + 3, (int)(n - 3), NULL, 0);
        if (len > 0) {
            std::wstring ws(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, b.data() + 3, (int)(n - 3), &ws[0], len);
            return ws;
        }
        return L"";
    }
    // UTF-16 LE BOM
    if (n >= 2 && u[0] == 0xFF && u[1] == 0xFE) {
        const wchar_t* p = reinterpret_cast<const wchar_t*>(b.data() + 2);
        size_t cnt = (n - 2) / 2;
        return std::wstring(p, cnt);
    }

    // 无 BOM: 先按严格 UTF-8 校验, 失败回退系统 ANSI (中文环境为 GBK)
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b.data(), (int)n, NULL, 0);
    if (len > 0) {
        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b.data(), (int)n, &ws[0], len);
        return ws;
    }
    len = MultiByteToWideChar(CP_ACP, 0, b.data(), (int)n, NULL, 0);
    if (len > 0) {
        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_ACP, 0, b.data(), (int)n, &ws[0], len);
        return ws;
    }
    return L"";
}

// 尝试在 s[pos] 解析 [mm:ss.xx] 时间戳; 成功返回毫秒并推进 pos
bool ParseTimestamp(const std::wstring& s, size_t& pos, double& ms) {
    if (pos >= s.size() || s[pos] != L'[') return false;
    size_t i = pos + 1;

    int minutes = 0, mdigits = 0;
    while (i < s.size() && iswdigit(s[i])) { minutes = minutes * 10 + (s[i] - L'0'); mdigits++; i++; }
    if (mdigits == 0 || i >= s.size() || s[i] != L':') return false;
    i++;

    int seconds = 0, sdigits = 0;
    while (i < s.size() && iswdigit(s[i])) { seconds = seconds * 10 + (s[i] - L'0'); sdigits++; i++; }
    if (sdigits == 0) return false;

    // 小数部分 (毫秒, 最多取 3 位)
    int frac = 0, fdigits = 0;
    if (i < s.size() && s[i] == L'.') {
        i++;
        while (i < s.size() && iswdigit(s[i])) {
            if (fdigits < 3) { frac = frac * 10 + (s[i] - L'0'); fdigits++; }
            i++;
        }
    }
    if (i >= s.size() || s[i] != L']') return false;
    i++;

    double total = minutes * 60000.0 + seconds * 1000.0;
    if (fdigits == 1) total += frac * 100.0;
    else if (fdigits == 2) total += frac * 10.0;
    else if (fdigits == 3) total += frac * 1.0;
    ms = total;
    pos = i;
    return true;
}

// 尝试在 s[pos] 解析 [key:value] 元信息标签; 成功返回 key/value 并推进 pos
bool ParseMetaTag(const std::wstring& s, size_t& pos, std::wstring& key, std::wstring& value) {
    if (pos >= s.size() || s[pos] != L'[') return false;
    size_t i = pos + 1;
    size_t keyStart = i;
    while (i < s.size() && s[i] != L':' && s[i] != L']') i++;
    if (i >= s.size() || s[i] != L':') return false;
    key = s.substr(keyStart, i - keyStart);
    i++;  // 跳过 ':'
    size_t valStart = i;
    while (i < s.size() && s[i] != L']') i++;
    if (i >= s.size()) return false;  // 无闭合 ']'
    value = s.substr(valStart, i - valStart);
    i++;  // 跳过 ']'
    pos = i;
    return true;
}

// 去掉两端空白
void Trim(std::wstring& s) {
    size_t b = 0;
    while (b < s.size() && iswspace(s[b])) b++;
    size_t e = s.size();
    while (e > b && iswspace(s[e - 1])) e--;
    s = s.substr(b, e - b);
}

}  // namespace

bool LyricParser::LoadFile(const std::wstring& lrcPath) {
    Clear();
    std::string bytes;
    if (!ReadFileBytes(lrcPath, bytes)) return false;
    std::wstring text = DecodeBytes(bytes);
    if (text.empty()) return false;
    return Parse(text);
}

void LyricParser::Clear() {
    m_lines.clear();
    m_title.clear();
    m_artist.clear();
    m_album.clear();
    m_offsetMs = 0.0;
}

bool LyricParser::Parse(const std::wstring& text) {
    // 按行拆分 (兼容 \n 与 \r\n)
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find(L'\n', start);
        if (nl == std::wstring::npos) nl = text.size();
        std::wstring line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        size_t pos = 0;
        std::vector<double> times;   // 本行所有时间戳

        // 循环解析行首的多个 [timestamp] 或 [meta]
        while (pos < line.size() && line[pos] == L'[') {
            double ms = 0;
            size_t save = pos;
            if (ParseTimestamp(line, pos, ms)) {
                times.push_back(ms);
                continue;
            }
            // 非时间戳 → 尝试元信息标签
            pos = save;
            std::wstring key, value;
            if (ParseMetaTag(line, pos, key, value)) {
                for (auto& c : key) c = towlower(c);
                if (key == L"ti") m_title = value;
                else if (key == L"ar") m_artist = value;
                else if (key == L"al") m_album = value;
                else if (key == L"offset") m_offsetMs = wcstod(value.c_str(), NULL);
                continue;
            }
            break;  // 无法识别, 当作正文
        }

        // 行尾剩余文本
        std::wstring content = line.substr(pos);
        Trim(content);

        // 每个时间戳生成一行 (一行多时间戳会被拆成多行)
        for (double t : times) {
            m_lines.push_back({ t, content });
        }

        start = nl + 1;
    }

    // 按时间升序排序 (稳定排序: 保留同一时间戳下「原文在前、译文在后」的文件顺序)
    std::stable_sort(m_lines.begin(), m_lines.end(),
              [](const LyricLine& a, const LyricLine& b) { return a.timeMs < b.timeMs; });

    return !m_lines.empty();
}

int LyricParser::FindIndex(double positionSeconds) const {
    if (m_lines.empty()) return -1;
    // 应用 offset: 正值 = 歌词延后, 即用 position - offset 去匹配原始时间戳
    double tMs = positionSeconds * 1000.0 - m_offsetMs;
    int idx = -1;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].timeMs <= tMs) idx = (int)i;
        else break;
    }
    return idx;
}

std::wstring FindLrcFile(const std::wstring& audioPath) {
    if (audioPath.empty()) return L"";
    // 去掉扩展名, 拼 .lrc
    size_t dot = audioPath.rfind(L'.');
    size_t slash = audioPath.find_last_of(L"\\/");
    std::wstring base;
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
        base = audioPath.substr(0, dot);
    else
        base = audioPath;
    std::wstring lrc = base + L".lrc";
    DWORD attr = GetFileAttributesW(lrc.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return lrc;
    return L"";
}
