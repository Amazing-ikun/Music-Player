#pragma once
#include <string>
#include <vector>
#include <map>
#include <ctime>

// Daily listening record
struct ListenRecord {
    std::wstring date;      // YYYY-MM-DD
    std::wstring weekday;   // eg. 周一
    double seconds;         // total seconds listened
};

// Weekly summary
struct WeekSummary {
    std::wstring label;          // eg. "第29周"
    std::wstring dateRange;      // eg. "07/14-07/20"
    double seconds;
};

// ---- Exported data interface (for visualization tools) ----
struct TopSongRecord {
    std::wstring name;   // display name
    int count;           // play count
};

// Which sections to include and the output format
struct ExportOptions {
    bool asCsv = true;          // true: CSV, false: JSON
    bool includeDaily = true;
    bool includeWeekly = true;
    bool includeTopSongs = true;
    bool includeTotal = true;
};

struct StatsExportData {
    std::vector<ListenRecord> dailyRecords;
    std::vector<WeekSummary> weeklyRecords;
    std::vector<TopSongRecord> topSongs;
    double totalSeconds = 0.0;

    // Serialize the selected sections as UTF-8 JSON / CSV
    std::string ToJson(const ExportOptions& opt) const;
    std::string ToCsv(const ExportOptions& opt) const;
};

// Listening history tracker
class ListeningHistory {
public:
    ListeningHistory();
    ~ListeningHistory();

    // Add a listening session (seconds). startTime is the wall-clock start of
    // the session; sessions crossing midnight are split across both days.
    void AddSession(double seconds, time_t startTime);

    // Load/save from file
    void Load(const std::wstring& filePath);
    void Save(const std::wstring& filePath);

    // Get records for date range [fromDate, toDate] inclusive
    std::vector<ListenRecord> GetDailyRecords(const std::string& fromDate,
                                              const std::string& toDate) const;
    double GetTotalSeconds(const std::string& fromDate,
                           const std::string& toDate) const;

    // Weekly summaries within a date range
    // Weeks are Mon 00:00 – Sun 23:59
    std::vector<WeekSummary> GetWeeklySummaries(const std::string& fromDate,
                                                const std::string& toDate) const;

    // Today's total
    double GetTodaySeconds() const;

    // Earliest date with any recorded data ("YYYY-MM-DD"); empty if none
    std::string GetEarliestDate() const;

    // Clear all data (for testing / reset)
    void Clear();

    bool IsEmpty() const { return m_daily.empty(); }

private:
    std::map<std::string, double> m_daily;  // "YYYY-MM-DD" -> seconds
    std::wstring m_filePath;
    bool m_dirty = false;

    static std::string GetToday();
    static std::string DateOfTime(time_t t);
    static std::string DateToKey(int year, int month, int day);
    static void KeyToDate(const std::string& key, int& year, int& month, int& day);
    static std::wstring GetWeekday(int year, int month, int day);
    static std::string GetMonday(const std::string& date);
    static std::string GetSunday(const std::string& date);
    static std::string FormatDateShort(int year, int month, int day);
    static int GetISOWeek(int year, int month, int day);
};
