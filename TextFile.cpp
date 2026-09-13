// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#include "TextFile.h"
#include <windows.h>

namespace {

// 单个文本文件的大小上限, 防止误读大文件把内存吃满
const DWORD kMaxTextFileBytes = 32u * 1024u * 1024u;

std::wstring FromCodePage(const char* data, int size, UINT codepage, bool strict) {
    if (size <= 0) return L"";
    DWORD flags = strict ? MB_ERR_INVALID_CHARS : 0;
    int len = MultiByteToWideChar(codepage, flags, data, size, NULL, 0);
    if (len <= 0) return L"";
    std::wstring ws(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(codepage, flags, data, size, &ws[0], len);
    return ws;
}

// 按 BOM 判断编码并解码; 无 BOM 视为 UTF-8
std::wstring DecodeAuto(const char* data, DWORD size) {
    if (size >= 2 && (BYTE)data[0] == 0xFF && (BYTE)data[1] == 0xFE) {
        // UTF-16LE + BOM
        size_t wcharCount = (size - 2) / sizeof(wchar_t);
        std::wstring out(reinterpret_cast<const wchar_t*>(data + 2), wcharCount);
        while (!out.empty() && out.back() == L'\0') out.pop_back();
        return out;
    }
    DWORD offset = 0;
    if (size >= 3 && (BYTE)data[0] == 0xEF && (BYTE)data[1] == 0xBB && (BYTE)data[2] == 0xBF)
        offset = 3;
    return Utf8ToWide(std::string(data + offset, size - offset));
}

}  // namespace

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), &out[0], len, NULL, NULL);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    return FromCodePage(s.data(), (int)s.size(), CP_UTF8, false);
}

std::wstring Utf8OrAnsiToWide(const std::string& s) {
    std::wstring ws = FromCodePage(s.data(), (int)s.size(), CP_UTF8, true);
    if (!ws.empty()) return ws;
    return FromCodePage(s.data(), (int)s.size(), CP_ACP, false);
}

std::wstring ReadTextAuto(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return L"";

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0 || size == INVALID_FILE_SIZE || size > kMaxTextFileBytes) {
        CloseHandle(hFile);
        return L"";
    }

    std::vector<char> buf(size);
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, buf.data(), size, &read, NULL);
    CloseHandle(hFile);
    if (!ok || read == 0) return L"";
    return DecodeAuto(buf.data(), read);
}

std::vector<std::wstring> ReadLinesAuto(const std::wstring& path) {
    std::vector<std::wstring> lines;
    std::wstring text = ReadTextAuto(path);
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find(L'\n', start);
        size_t end = (nl == std::wstring::npos) ? text.size() : nl;
        if (end > start && text[end - 1] == L'\r') --end;
        if (end > start) lines.emplace_back(text, start, end - start);
        if (nl == std::wstring::npos) break;
        start = nl + 1;
    }
    return lines;
}

bool WriteTextUtf8(const std::wstring& path, const std::wstring& text, bool withBom) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    if (withBom) {
        const BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
        WriteFile(hFile, bom, 3, &written, NULL);
    }
    std::string utf8 = WideToUtf8(text);
    BOOL ok = TRUE;
    if (!utf8.empty())
        ok = WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, NULL);
    CloseHandle(hFile);
    return ok != FALSE;
}

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* last = wcsrchr(path, L'\\');
    if (last) *last = L'\0';
    return path;
}

// ---- 状态文件目录与统一后缀 ----

namespace {
const wchar_t kDataDirName[] = L"Data";
const wchar_t kDataFileExt[] = L".mpdf";
}

std::wstring GetDataDirectory() {
    // 目录是否可用在运行期不会变, 只解析一次
    static std::wstring cached;
    if (!cached.empty()) return cached;

    std::wstring dir = GetExeDirectory() + L"\\" + kDataDirName;
    bool usable = CreateDirectoryW(dir.c_str(), NULL) != FALSE ||
                  GetLastError() == ERROR_ALREADY_EXISTS;
    if (usable) {
        // 目录可能已存在但不可写, 用临时探针确认; FILE_FLAG_DELETE_ON_CLOSE 使其关闭即消失
        std::wstring probe = dir + L"\\writetest";
        HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        usable = (h != INVALID_HANDLE_VALUE);
        if (usable) CloseHandle(h);
    }
    cached = usable ? dir : GetExeDirectory();
    return cached;
}

std::wstring DataFile(const wchar_t* name) {
    return GetDataDirectory() + L"\\" + name + kDataFileExt;
}

void MigrateLegacyStateFiles() {
    static const wchar_t* kNames[] = {
        L"settings", L"hotkeys", L"volume", L"lastsong", L"lastfolder",
        L"playlist", L"playcount", L"lyrics_map", L"history",
        L"durations", L"loudness",
    };
    std::wstring exeDir = GetExeDirectory();
    for (const wchar_t* name : kNames) {
        std::wstring oldPath = exeDir + L"\\." + name + L".txt";
        std::wstring newPath = DataFile(name);   // 顺带确保 Data\ 已创建
        if (GetFileAttributesW(oldPath.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        if (GetFileAttributesW(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        MoveFileW(oldPath.c_str(), newPath.c_str());
    }
}
