#pragma once
#include <string>
#include <windows.h>
#include <bass.h>

// ============================================
// Playback Modes
// ============================================
enum class PlayMode {
    Sequential,     // 顺序播放
    RepeatOne,      // 单曲循环
    Shuffle         // 随机播放
};

// ============================================
// AudioEngine - BASS 音频引擎封装
// ============================================
// 职责: BASS初始化/清理、音频流加载/播放控制、
//       音量控制、进度控制、播放模式、元数据读取
// ============================================
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // 初始化/清理
    bool Initialize(HWND hwnd);
    void Cleanup();

    // 通知：BASS已自动释放播放结束的流
    void NotifyEndOfSong();

    // 文件加载/卸载
    bool Load(const std::wstring& filePath);
    void Unload();

    // 播放控制
    void Play();
    void Pause();
    void Stop();
    bool IsPlaying() const { return m_playing; }
    bool IsPaused() const { return m_paused; }
    bool IsLoaded() const { return m_stream != 0; }

    // 音量 (0-100)
    void SetVolume(int volume);
    int GetVolume() const { return m_volume; }

    // 进度 (秒)
    double GetPosition() const;
    double GetLength() const;
    void SetPosition(double seconds);

    // 播放模式
    void SetPlayMode(PlayMode mode) { m_playMode = mode; }
    PlayMode GetPlayMode() const { return m_playMode; }
    PlayMode CyclePlayMode();

    // 元数据（延迟加载，播放时才读取）
    std::wstring GetFormattedMetadata() const;

    // 设置通知窗口（歌曲结束时发送消息）
    void SetNotifyWindow(HWND hwnd, UINT msg) {
        m_notifyHwnd = hwnd;
        m_notifyMsg = msg;
    }

private:
    // BASS 同步回调：歌曲播放结束
    static void CALLBACK EndSyncProc(HSYNC handle, DWORD channel, DWORD data, void* user);

    HSTREAM m_stream;       // BASS 音频流句柄
    HSYNC   m_endSync;      // 结束同步器句柄
    int     m_volume;       // 音量 0-100
    bool    m_playing;      // 是否正在播放
    bool    m_paused;       // 是否已暂停
    PlayMode m_playMode;    // 当前播放模式
    HWND    m_notifyHwnd;   // 通知窗口
    UINT    m_notifyMsg;    // 通知消息
};
