#include "Settings.h"
#include <cstdio>
#include <cstring>
#include <cwctype>

bool ParseColor(const std::wstring& input, COLORREF& out) {
    std::wstring s = input;
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) b++;
    while (e > b && iswspace(s[e - 1])) e--;
    s = s.substr(b, e - b);
    if (s.empty()) return false;
    if (s[0] == L'#') s = s.substr(1);
    if (s.empty()) return false;

    // RGB: r,g,b 或 r g b
    int r = 0, g = 0, bl = 0;
    if (swscanf(s.c_str(), L"%d,%d,%d", &r, &g, &bl) == 3 ||
        swscanf(s.c_str(), L"%d %d %d", &r, &g, &bl) == 3) {
        if (r < 0 || r > 255 || g < 0 || g > 255 || bl < 0 || bl > 255) return false;
        out = RGB(r, g, bl);
        return true;
    }
    // 十六进制: 6 位 RRGGBB
    if (s.size() == 6) {
        unsigned v = 0;
        bool ok = true;
        for (wchar_t c : s) {
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) { ok = false; break; }
            v = v * 16 + (c >= L'0' && c <= L'9' ? c - L'0' : (towlower(c) - L'a' + 10));
        }
        if (ok) {
            out = RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
            return true;
        }
    }
    return false;
}

std::wstring ColorToHex(COLORREF c) {
    wchar_t buf[16];
    swprintf(buf, 16, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

void Settings::Load(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, NULL);
    int iRemember = 0, iTray = 0, iBalance = 0, iLyricsShow = 0, iLyricsLocked = 0, iLyricsTranslation = 0;
    if (size > 0 && size < 2048) {
        char buf[2048] = {};
        DWORD read;
        ReadFile(hFile, buf, size, &read, NULL);
        char* p = buf;
        while (*p) {
            char* nl = strchr(p, '\n');
            if (!nl) nl = p + strlen(p);
            *nl = '\0';
            if (strncmp(p, "hk_", 3) != 0) {
                if (sscanf(p, "autoplay=%d", &autoplay) == 1) {}
                else if (sscanf(p, "remember_progress=%d", &iRemember) == 1) { rememberProgress = (iRemember != 0); }
                else if (sscanf(p, "tray_minimize=%d", &iTray) == 1) { trayMinimize = (iTray != 0); }
                else if (sscanf(p, "loudness_balance=%d", &iBalance) == 1) { balanceEnabled = (iBalance != 0); }
                else if (sscanf(p, "lyrics_show=%d", &iLyricsShow) == 1) { lyricsShow = (iLyricsShow != 0); }
                else if (strncmp(p, "lyrics_color=", 13) == 0) {
                    std::wstring hex;
                    for (char* q = p + 13; *q; ++q) hex += (wchar_t)(unsigned char)*q;
                    ParseColor(hex, lyricsColor);
                }
                else if (strncmp(p, "lyrics_nextcolor=", 17) == 0) {
                    std::wstring hex;
                    for (char* q = p + 17; *q; ++q) hex += (wchar_t)(unsigned char)*q;
                    ParseColor(hex, lyricsNextColor);
                }
                else if (sscanf(p, "lyrics_locked=%d", &iLyricsLocked) == 1) { lyricsLocked = (iLyricsLocked != 0); }
                else if (sscanf(p, "lyrics_translation=%d", &iLyricsTranslation) == 1) { lyricsTranslation = (iLyricsTranslation != 0); }
                else if (sscanf(p, "lyrics_fontsize=%d", &lyricsFontSize) == 1) {
                    if (lyricsFontSize < 12) lyricsFontSize = 12;
                    if (lyricsFontSize > 72) lyricsFontSize = 72;
                }
                else if (sscanf(p, "lyrics_secondfontsize=%d", &lyricsSecondFontSize) == 1) {
                    if (lyricsSecondFontSize < 12) lyricsSecondFontSize = 12;
                    if (lyricsSecondFontSize > 72) lyricsSecondFontSize = 72;
                }
                else if (sscanf(p, "play_mode=%d", &playMode) == 1) {
                    if (playMode < 0) playMode = 0;
                    if (playMode > 2) playMode = 2;
                }
                else if (sscanf(p, "play_speed=%lf", &playSpeed) == 1) {
                    if (playSpeed < 0.1) playSpeed = 0.1;
                    if (playSpeed > 10.0) playSpeed = 10.0;
                }
            }
            p = nl + 1;
        }
    }
    CloseHandle(hFile);
}

void Settings::Save(const std::wstring& path) const {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written;
    char buf[300];
    int len = sprintf(buf, "autoplay=%d\nremember_progress=%d\ntray_minimize=%d\nplay_mode=%d\nplay_speed=%.2f\nloudness_balance=%d\nlyrics_show=%d\nlyrics_color=#%02X%02X%02X\nlyrics_nextcolor=#%02X%02X%02X\nlyrics_locked=%d\nlyrics_translation=%d\nlyrics_fontsize=%d\nlyrics_secondfontsize=%d\n",
                      autoplay,
                      rememberProgress ? 1 : 0,
                      trayMinimize ? 1 : 0,
                      playMode,
                      playSpeed,
                      balanceEnabled ? 1 : 0,
                      lyricsShow ? 1 : 0,
                      (unsigned)GetRValue(lyricsColor), (unsigned)GetGValue(lyricsColor), (unsigned)GetBValue(lyricsColor),
                      (unsigned)GetRValue(lyricsNextColor), (unsigned)GetGValue(lyricsNextColor), (unsigned)GetBValue(lyricsNextColor),
                      lyricsLocked ? 1 : 0,
                      lyricsTranslation ? 1 : 0,
                      lyricsFontSize,
                      lyricsSecondFontSize);
    WriteFile(hFile, buf, (DWORD)len, &written, NULL);
    CloseHandle(hFile);
}
