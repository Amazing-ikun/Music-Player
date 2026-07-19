#pragma once
#include <string>
#include <vector>

// SongInfo - 单曲信息 (延迟加载: 启动时只读ID3v1快速标签)
struct SongInfo {
    std::wstring filePath;
    std::wstring title;     // 显示标题 (来自ID3v1或回退到文件名)
    std::wstring artist;    // 歌手 (来自ID3v1)
    std::wstring album;     // 专辑 (来自ID3v1)
    double       duration;  // 时长(秒), 0 = 未知 (播放时通过BASS获取)
};

// PlaylistManager - 播放列表管理器
class PlaylistManager {
public:
    PlaylistManager();

    void ScanFolder(const std::wstring& folderPath);
    void AddFile(const std::wstring& filePath);
    void Clear();

    int  GetCount() const { return (int)m_songs.size(); }
    bool IsEmpty() const { return m_songs.empty(); }

    // 兼容旧接口: 返回文件路径
    const std::wstring& GetFile(int index) const;
    // 新的SongInfo接口
    const SongInfo& GetSong(int index) const;
    std::vector<SongInfo>& GetSongs() { return m_songs; }

    // 播放后从BASS更新元数据和时长
    void UpdateMetadata(int index, const std::wstring& artist,
                        const std::wstring& title,
                        const std::wstring& album,
                        double duration);

    // 排序 (column: 0=序号, 1=标题, 2=专辑, 3=时长)
    void Sort(int column, bool ascending);

    static bool IsAudioExtension(const std::wstring& extension);

private:
    void ScanDirectory(const std::wstring& dirPath);

    // 快速读取ID3v1标签 (不依赖BASS, 只读文件末尾128字节)
    static bool ReadID3v1(const std::wstring& filePath, SongInfo& info);

    // 从文件路径提取显示名 (无扩展名)
    static std::wstring GetDisplayNameFromPath(const std::wstring& path);

    std::vector<SongInfo> m_songs;
};
