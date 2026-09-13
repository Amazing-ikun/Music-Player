// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#include "LyricsController.h"
#include "Settings.h"
#include "TextFile.h"
#include "Dialogs.h"
#include <commdlg.h>

namespace {

// 取文件名 (去目录、去任意扩展名)
std::wstring BaseNameNoExt(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring file = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    size_t dot = file.rfind(L'.');
    if (dot != std::wstring::npos) file = file.substr(0, dot);
    return file;
}

// 忽略大小写, 只要一方文件名包含另一方即视为「匹配」(不弹确认)
bool NamesDifferTooMuch(const std::wstring& song, const std::wstring& lrc) {
    std::wstring a = song, b = lrc;
    for (auto& c : a) c = towlower(c);
    for (auto& c : b) c = towlower(c);
    if (a.empty() || b.empty()) return false;
    return (a.find(b) == std::wstring::npos && b.find(a) == std::wstring::npos);
}

}  // namespace

LyricsController::~LyricsController() {
    Destroy();
}

void LyricsController::Init(Settings* settings, LyricsHost host) {
    m_settings = settings;
    m_host = host;
    m_window.SetListener(this);
    if (settings) {
        m_window.SetLocked(settings->lyricsLocked);
        m_window.SetShowTranslation(settings->lyricsTranslation);
    }
    ApplyStyle();
}

void LyricsController::Destroy() {
    m_window.SetListener(nullptr);
    m_window.Destroy();
}

double LyricsController::CurrentPosition() const {
    return m_host.playbackPosition ? m_host.playbackPosition() : 0.0;
}

// 切歌时加载当前歌曲的 .lrc 歌词并刷新悬浮窗
void LyricsController::LoadForCurrentSong() {
    m_parser.Clear();
    std::wstring emptyText = L"暂无歌词";
    std::wstring songPath = m_host.currentSongPath ? m_host.currentSongPath() : std::wstring();
    if (!songPath.empty()) {
        auto it = m_map.find(songPath);
        if (it != m_map.end() && !it->second.empty()) {
            // 有显式映射: 校验文件是否存在; 失效则不回退, 显示「未找到歌词」
            if (GetFileAttributesW(it->second.c_str()) != INVALID_FILE_ATTRIBUTES)
                m_parser.LoadFile(it->second);
            else
                emptyText = L"未找到歌词/无效的歌词";
        } else {
            // 无映射: 同目录同名兜底
            std::wstring lrc = FindLrcFile(songPath);
            if (!lrc.empty()) m_parser.LoadFile(lrc);
        }
    }
    m_window.SetEmptyText(emptyText);
    m_window.SetLyrics(m_parser);
    m_window.SetCurrentIndex(m_parser.FindIndex(CurrentPosition()));
}

// 显示桌面歌词悬浮窗 (首次时创建)
void LyricsController::ShowWindowInternal() {
    if (!m_window.Hwnd() && !m_window.Create(m_host.hInst)) return;
    m_window.SetLyrics(m_parser);
    m_window.SetCurrentIndex(m_parser.FindIndex(CurrentPosition()));
    m_window.Show();
}

void LyricsController::Tick() {
    if (m_window.Visible())
        m_window.SetCurrentIndex(m_parser.FindIndex(CurrentPosition()));
}

// 统一设置桌面歌词显示状态 (按钮/菜单/托盘/右键共用)
void LyricsController::SetVisible(bool on) {
    if (m_settings) m_settings->lyricsShow = on;
    if (on) ShowWindowInternal();
    else m_window.Hide();
    if (m_host.visibilityChanged) m_host.visibilityChanged();
}

void LyricsController::ApplyStyle() {
    if (!m_settings) return;
    m_window.SetColor(m_settings->lyricsColor);
    m_window.SetNextColor(m_settings->lyricsNextColor);
    m_window.SetFontSize(m_settings->lyricsFontSize);
    m_window.SetSecondFontSize(m_settings->lyricsSecondFontSize);
}

// 扫描报告: 统计歌单里有多少首能在同目录找到同名 .lrc (不写映射表, 播放时实时兜底)
void LyricsController::MatchAll() {
    int count = m_host.songCount ? m_host.songCount() : 0;
    int found = 0;
    for (int i = 0; i < count; ++i) {
        std::wstring path = m_host.songPathAt ? m_host.songPathAt(i) : std::wstring();
        if (!FindLrcFile(path).empty()) found++;
    }
    MessageBoxW(m_host.owner,
        (L"共 " + std::to_wstring(count) + L" 首歌曲，其中 "
         + std::to_wstring(found) + L" 首能在同目录找到同名歌词。").c_str(),
        L"匹配歌词", MB_OK | MB_ICONINFORMATION);
}

// 手动为单首歌曲选择 .lrc
void LyricsController::MatchForSong(int songIdx) {
    std::wstring songPath = m_host.songPathAt ? m_host.songPathAt(songIdx) : std::wstring();
    if (songPath.empty()) return;

    wchar_t file[1024] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_host.owner;
    ofn.lpstrFilter = L"歌词文件 (*.lrc)\0*.lrc\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"选择歌词文件";
    if (!GetOpenFileNameW(&ofn)) return;  // 用户取消

    std::wstring lrcPath = file;

    if (NamesDifferTooMuch(BaseNameNoExt(songPath), BaseNameNoExt(lrcPath))) {
        int r = MessageBoxW(m_host.owner,
            L"您选择的歌词文件名与歌曲名似乎不匹配，要继续吗？",
            L"匹配歌词", MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES) return;
    }

    m_map[songPath] = lrcPath;
    SaveMap();
    std::wstring current = m_host.currentSongPath ? m_host.currentSongPath() : std::wstring();
    if (!current.empty() && current == songPath) LoadForCurrentSong();
    MessageBoxW(m_host.owner, L"歌词匹配成功。", L"匹配歌词", MB_OK | MB_ICONINFORMATION);
}

void LyricsController::SaveMap() {
    std::wstring text;
    for (const auto& kv : m_map)
        text += kv.first + L"|" + kv.second + L"\n";
    WriteTextUtf8(DataFile(L"lyrics_map"), text);
}

void LyricsController::LoadMap() {
    m_map.clear();
    for (const std::wstring& line :
            ReadLinesAuto(DataFile(L"lyrics_map"))) {
        size_t sep = line.find(L'|');
        if (sep == std::wstring::npos) continue;
        std::wstring song = line.substr(0, sep);
        std::wstring lrc = line.substr(sep + 1);
        if (!song.empty() && !lrc.empty()) m_map[song] = lrc;
    }
}

// 歌词设置对话框: 颜色(十六进制/RGB) + 字号
void LyricsController::ShowSettingsDialog() {
    if (!m_settings) return;
    LyricsStyle style;
    style.firstColor = m_settings->lyricsColor;
    style.secondColor = m_settings->lyricsNextColor;
    style.firstFontSize = m_settings->lyricsFontSize;
    style.secondFontSize = m_settings->lyricsSecondFontSize;
    if (!::ShowLyricsSettingsDialog(m_host.hInst, m_host.owner, style)) return;
    m_settings->lyricsColor = style.firstColor;
    m_settings->lyricsNextColor = style.secondColor;
    m_settings->lyricsFontSize = style.firstFontSize;
    m_settings->lyricsSecondFontSize = style.secondFontSize;
    ApplyStyle();
    if (m_host.persistSettings) m_host.persistSettings();
}

void LyricsController::OnHidden() {
    SetVisible(false);
}

void LyricsController::OnFontSizeChanged(int first, int second) {
    if (!m_settings) return;
    m_settings->lyricsFontSize = first;
    m_settings->lyricsSecondFontSize = second;
    if (m_host.persistSettings) m_host.persistSettings();
}

void LyricsController::OnLockedChanged(bool locked) {
    if (m_settings) m_settings->lyricsLocked = locked;
    if (m_host.persistSettings) m_host.persistSettings();
}

void LyricsController::OnTranslationChanged(bool show) {
    if (m_settings) m_settings->lyricsTranslation = show;
    if (m_host.persistSettings) m_host.persistSettings();
}

void LyricsController::OnPrevNext(bool next) {
    if (m_host.navigate) m_host.navigate(next);
}
