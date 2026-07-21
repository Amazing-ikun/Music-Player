#include "ListeningHistory.h"
#include <ctime>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <windows.h>

// ---- helpers ----

static bool IsLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int DaysInMonth(int y, int m) {
    static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && IsLeap(y)) return 29;
    return d[m - 1];
}

// Convert YYYY-MM-DD to days since epoch (for date arithmetic)
static int DateToDays(const std::string& date) {
    int y, m, d;
    sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d);
    int total = 0;
    for (int Y = 1970; Y < y; Y++)
        total += IsLeap(Y) ? 366 : 365;
    for (int M = 1; M < m; M++)
        total += DaysInMonth(y, M);
    total += d - 1;
    return total;
}

static std::string DaysToDate(int days) {
    int y = 1970;
    while (true) {
        int yd = IsLeap(y) ? 366 : 365;
        if (days < yd) break;
        days -= yd;
        y++;
    }
    int m = 1;
    while (true) {
        int md = DaysInMonth(y, m);
        if (days < md) break;
        days -= md;
        m++;
    }
    int d = days + 1;
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return buf;
}

// ---- ListeningHistory ----

ListeningHistory::ListeningHistory() {}

ListeningHistory::~ListeningHistory() {
    if (m_dirty && !m_filePath.empty())
        Save(m_filePath);
}

void ListeningHistory::AddSession(double seconds) {
    if (seconds <= 0) return;
    std::string today = GetToday();
    m_daily[today] += seconds;
    m_dirty = true;
}

void ListeningHistory::Load(const std::wstring& filePath) {
    m_filePath = filePath;
    m_daily.clear();
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, NULL);
    if (size > 2) {
        DWORD read = 0;
        std::wstring buf(size / 2 + 1, L'\0');
        ReadFile(hFile, &buf[0], size, &read, NULL);
        buf[read / 2] = L'\0';
        const wchar_t* p = buf.c_str();
        if (*p == 0xFEFF) p++;
        while (*p) {
            const wchar_t* nl = wcschr(p, L'\n');
            size_t lineLen = nl ? (size_t)(nl - p) : wcslen(p);
            if (lineLen > 0 && p[lineLen - 1] == L'\r') --lineLen;
            if (lineLen > 0) {
                std::wstring line(p, lineLen);
                size_t eq = line.find(L'=');
                if (eq != std::wstring::npos && eq > 0) {
                    std::wstring date = line.substr(0, eq);
                    std::wstring val = line.substr(eq + 1);
                    char dateBuf[32];
                    snprintf(dateBuf, sizeof(dateBuf), "%ls", date.c_str());
                    m_daily[dateBuf] += wcstod(val.c_str(), NULL);
                }
            }
            p = nl ? nl + 1 : p + lineLen;
        }
    }
    CloseHandle(hFile);
    m_dirty = false;
}

void ListeningHistory::Save(const std::wstring& filePath) {
    if (!m_dirty && filePath == m_filePath) return;
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written;
    const WORD bom = 0xFEFF;
    WriteFile(hFile, &bom, 2, &written, NULL);
    for (const auto& entry : m_daily) {
        if (entry.second <= 0) continue;
        wchar_t dateW[32];
        swprintf(dateW, 32, L"%hs", entry.first.c_str());
        std::wstring line = std::wstring(dateW) + L"=" +
            std::to_wstring(entry.second) + L"\n";
        WriteFile(hFile, line.c_str(),
            (DWORD)(line.size() * sizeof(wchar_t)), &written, NULL);
    }
    CloseHandle(hFile);
    m_dirty = false;
}

std::vector<ListenRecord> ListeningHistory::GetDailyRecords(
    const std::string& fromDate, const std::string& toDate) const
{
    std::vector<ListenRecord> result;
    int start = DateToDays(fromDate);
    int end = DateToDays(toDate);
    for (int d = start; d <= end; d++) {
        std::string key = DaysToDate(d);
        auto it = m_daily.find(key);
        double secs = (it != m_daily.end()) ? it->second : 0.0;
        if (secs <= 0) continue;
        int y, m, day;
        KeyToDate(key, y, m, day);
        ListenRecord rec;
        wchar_t buf[16];
        swprintf(buf, 16, L"%hs", key.c_str());
        rec.date = buf;
        rec.weekday = GetWeekday(y, m, day);
        rec.seconds = secs;
        result.push_back(rec);
    }
    return result;
}

double ListeningHistory::GetTotalSeconds(const std::string& fromDate,
                                          const std::string& toDate) const
{
    double total = 0;
    int start = DateToDays(fromDate);
    int end = DateToDays(toDate);
    for (int d = start; d <= end; d++) {
        std::string key = DaysToDate(d);
        auto it = m_daily.find(key);
        if (it != m_daily.end())
            total += it->second;
    }
    return total;
}

std::vector<WeekSummary> ListeningHistory::GetWeeklySummaries(
    const std::string& fromDate, const std::string& toDate) const
{
    // Group days into Mon-Sun weeks
    std::map<std::string, double> weekly; // Monday date -> total seconds

    int start = DateToDays(fromDate);
    int end = DateToDays(toDate);
    for (int d = start; d <= end; d++) {
        std::string key = DaysToDate(d);
        auto it = m_daily.find(key);
        if (it == m_daily.end() || it->second <= 0) continue;
        std::string mon = GetMonday(key);
        weekly[mon] += it->second;
    }

    std::vector<WeekSummary> result;
    for (const auto& entry : weekly) {
        std::string mon = entry.first;
        std::string sun = GetSunday(mon);

        int y1, m1, d1;
        KeyToDate(mon, y1, m1, d1);
        int weekNum = GetISOWeek(y1, m1, d1);

        int y2, m2, d2;
        KeyToDate(sun, y2, m2, d2);

        WeekSummary ws;
        wchar_t wlabel[64];
        swprintf(wlabel, 64, L"第%d周", weekNum);
        ws.label = wlabel;
        wchar_t wrange[64];
        swprintf(wrange, 64, L"%02d/%02d-%02d/%02d", m1, d1, m2, d2);
        ws.dateRange = wrange;
        ws.seconds = entry.second;
        result.push_back(ws);
    }
    return result;
}

double ListeningHistory::GetTodaySeconds() const {
    std::string today = GetToday();
    auto it = m_daily.find(today);
    return (it != m_daily.end()) ? it->second : 0.0;
}

StatsExportData ListeningHistory::GetExportData(
    const std::string& fromDate, const std::string& toDate) const
{
    StatsExportData data;
    std::string f = fromDate.empty() ? "1970-01-01" : fromDate;
    std::string t = toDate.empty() ? DaysToDate(DateToDays(GetToday())) : toDate;
    data.dailyRecords = GetDailyRecords(f, t);
    data.totalSeconds = GetTotalSeconds(f, t);
    return data;
}

void ListeningHistory::Clear() {
    m_daily.clear();
    m_dirty = true;
}

// ---- internal statics ----

std::string ListeningHistory::GetToday() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    return DateToKey(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

std::string ListeningHistory::DateToKey(int year, int month, int day) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return buf;
}

void ListeningHistory::KeyToDate(const std::string& key, int& year, int& month, int& day) {
    sscanf(key.c_str(), "%d-%d-%d", &year, &month, &day);
}

std::wstring ListeningHistory::GetWeekday(int year, int month, int day) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_isdst = -1;
    mktime(&t);
    static const wchar_t* names[] = { L"周日", L"周一", L"周二", L"周三", L"周四", L"周五", L"周六" };
    return names[t.tm_wday % 7];
}

int ListeningHistory::GetISOWeek(int year, int month, int day) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_isdst = -1;
    mktime(&t);
    char buf[8];
    strftime(buf, sizeof(buf), "%V", &t);
    return atoi(buf);
}

std::string ListeningHistory::GetMonday(const std::string& date) {
    int y, m, d;
    sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d);
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_isdst = -1;
    mktime(&t);
    // tm_wday: 0=Sun, 1=Mon, ..., 6=Sat
    // Days to go back to Monday
    int diff = (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
    int days = DateToDays(date) - diff;
    return DaysToDate(days);
}

std::string ListeningHistory::GetSunday(const std::string& date) {
    // Input is a Monday, output is the following Sunday
    int days = DateToDays(date) + 6;
    return DaysToDate(days);
}

std::string ListeningHistory::FormatDateShort(int year, int month, int day) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d/%02d", month, day);
    return buf;
}

// ---- StatsExportData ----

std::string StatsExportData::ToJson() const {
    std::string json = "{\n";
    json += "  \"totalSeconds\": " + std::to_string(totalSeconds) + ",\n";
    json += "  \"dailyRecords\": [\n";
    for (size_t i = 0; i < dailyRecords.size(); i++) {
        const auto& rec = dailyRecords[i];
        char dateBuf[64];
        snprintf(dateBuf, sizeof(dateBuf), "%ls", rec.date.c_str());
        char wdBuf[16];
        snprintf(wdBuf, sizeof(wdBuf), "%ls", rec.weekday.c_str());
        json += "    {\"date\":\"" + std::string(dateBuf) + "\",";
        json += "\"weekday\":\"" + std::string(wdBuf) + "\",";
        json += "\"seconds\":" + std::to_string(rec.seconds) + "}";
        if (i < dailyRecords.size() - 1) json += ",";
        json += "\n";
    }
    json += "  ]\n";
    json += "}\n";
    return json;
}
