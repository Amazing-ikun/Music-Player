#include "PlaylistManager.h"
#include <windows.h>
#include <cstring>
#include <algorithm>

PlaylistManager::PlaylistManager() {}

bool PlaylistManager::IsAudioExtension(const std::wstring& ext) {
    std::wstring lower = ext;
    for (auto& c : lower) c = towlower(c);
    return (lower == L".mp3" || lower == L".flac" || lower == L".wav");
}

void PlaylistManager::ScanFolder(const std::wstring& folderPath) {
    m_songs.clear();
    ScanDirectory(folderPath);
    std::sort(m_songs.begin(), m_songs.end(),
        [](const SongInfo& a, const SongInfo& b) {
            return _wcsicmp(a.filePath.c_str(), b.filePath.c_str()) < 0;
        });
}

void PlaylistManager::ScanDirectory(const std::wstring& dirPath) {
    std::wstring searchPath = dirPath + L"\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoBasic,
                                     &findData, FindExSearchNameMatch, NULL, 0);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0)
            continue;

        std::wstring fullPath = dirPath + L"\\" + findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDirectory(fullPath);
        } else {
            const wchar_t* dot = wcsrchr(findData.cFileName, L'.');
            std::wstring ext = dot ? dot : L"";
            if (IsAudioExtension(ext)) {
                SongInfo info;
                info.filePath = fullPath;
                info.duration = 0;
                // 快速读取ID3v1标签, 失败或标题为空时回退到文件名
                if (!ReadID3v1(fullPath, info) || info.title.empty()) {
                    info.title = GetDisplayNameFromPath(fullPath);
                }
                m_songs.push_back(info);
            }
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
}

// ID3v1 快速解析 (文件末尾128字节)
bool PlaylistManager::ReadID3v1(const std::wstring& filePath, SongInfo& info) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BYTE buf[128];
    DWORD read;
    SetFilePointer(hFile, -128, NULL, FILE_END);
    BOOL ok = ReadFile(hFile, buf, 128, &read, NULL) && read == 128;
    CloseHandle(hFile);
    if (!ok || memcmp(buf, "TAG", 3) != 0) return false;

    auto trim = [](const char* data, int len) -> std::string {
        std::string s(data, len);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
        return s;
    };
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::wstring ws(len - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &ws[0], len);
        return ws;
    };

    info.title  = toWide(trim((const char*)buf + 3, 30));
    info.artist = toWide(trim((const char*)buf + 33, 30));
    info.album  = toWide(trim((const char*)buf + 63, 30));
    return true;
}

std::wstring PlaylistManager::GetDisplayNameFromPath(const std::wstring& path) {
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

void PlaylistManager::AddFile(const std::wstring& filePath) {
    SongInfo info;
    info.filePath = filePath;
    info.duration = 0;
    if (!ReadID3v1(filePath, info)) {
        info.title = GetDisplayNameFromPath(filePath);
    }
    m_songs.push_back(info);
}

void PlaylistManager::Clear() {
    m_songs.clear();
}

const std::wstring& PlaylistManager::GetFile(int index) const {
    static const std::wstring empty;
    if (index < 0 || index >= (int)m_songs.size()) return empty;
    return m_songs[index].filePath;
}

const SongInfo& PlaylistManager::GetSong(int index) const {
    static const SongInfo empty{};
    if (index < 0 || index >= (int)m_songs.size()) return empty;
    return m_songs[index];
}

void PlaylistManager::UpdateMetadata(int index, const std::wstring& artist,
                                     const std::wstring& title,
                                     const std::wstring& album,
                                     double duration) {
    if (index < 0 || index >= (int)m_songs.size()) return;
    if (!title.empty())  m_songs[index].title    = title;
    if (!artist.empty()) m_songs[index].artist   = artist;
    if (!album.empty())  m_songs[index].album    = album;
    if (duration > 0)    m_songs[index].duration = duration;
}

void PlaylistManager::Sort(int column, bool ascending) {
    std::sort(m_songs.begin(), m_songs.end(),
        [column, ascending](const SongInfo& a, const SongInfo& b) {
            int cmp = 0;
            switch (column) {
                case 1: // 标题
                    cmp = _wcsicmp(a.title.c_str(), b.title.c_str());
                    if (cmp == 0) cmp = _wcsicmp(a.artist.c_str(), b.artist.c_str());
                    break;
                case 2: // 专辑
                    cmp = _wcsicmp(a.album.c_str(), b.album.c_str());
                    if (cmp == 0) cmp = _wcsicmp(a.title.c_str(), b.title.c_str());
                    break;
                case 3: // 时长
                    if (a.duration < b.duration) cmp = -1;
                    else if (a.duration > b.duration) cmp = 1;
                    else cmp = 0;
                    break;
                default:
                    return false;
            }
            return ascending ? (cmp < 0) : (cmp > 0);
        });
}
