#pragma once
#include <string>
#include <map>
#include <windows.h>
#include <bass.h>
#include <bass_fx.h>

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

// 错误码
enum class AudioError {
    Success = 0,
    FileNotFound,       // 文件不存在或无法访问
    UnsupportedFormat,  // 不支持此音频格式
    MissingCodec,       // 缺少所需的解码器
    UnsupportedParam,   // 不支持的音频格式参数
    DecodeFailed,       // 解码失败
    InitFailed,         // 音频引擎未初始化
    Unknown             // 未知错误
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // 初始化/清理
    bool Initialize(HWND hwnd);
    void Cleanup();

    // 通知：BASS已自动释放播放结束的流
    void NotifyEndOfSong();

    // 淡出完成：由主窗口在 WM_APP_FADE_DONE 中调用（主线程）
    void OnFadeComplete();

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

    // 淡入淡出 (变速不变调，通过音量滑动实现)
    void PauseFade(DWORD fadeMs = 500);
    void PlayFade();
    bool IsFading() const { return m_fading; }
    void SetFadeNotify(HWND hwnd, UINT msg) { m_fadeHwnd = hwnd; m_fadeMsg = msg; }

    // 音量 (0-100)
    void SetVolume(int volume);
    int GetVolume() const { return m_volume; }

    // 音量平衡: 按歌曲响度归一化, 使不同歌曲听感音量相近
    void SetBalanceEnabled(bool enabled);
    bool IsBalanceEnabled() const { return m_balanceEnabled; }
    // 重新测量并应用当前歌曲的平衡增益 (测量结果会写入 .loudness.txt 缓存)
    void ApplyBalance();

    // 进度 (秒)
    double GetPosition() const;
    double GetLength() const;
    void SetPosition(double seconds);

    // 独立解码流探测文件时长(秒), 不发声, 不影响正在播放的流; 失败返回 0
    static double ProbeDuration(const std::wstring& filePath);

    // 播放模式
    void SetPlayMode(PlayMode mode) { m_playMode = mode; }
    PlayMode GetPlayMode() const { return m_playMode; }
    PlayMode CyclePlayMode();

    // 倍速播放 (0.1 ~ 10.0)
    void SetSpeed(double speed);
    double GetSpeed() const { return m_speed; }

    // 元数据（延迟加载，播放时才读取）
    std::wstring GetFormattedMetadata() const;

    // 设置通知窗口（歌曲结束时发送消息）
    void SetNotifyWindow(HWND hwnd, UINT msg) {
        m_notifyHwnd = hwnd;
        m_notifyMsg = msg;
    }

    // 错误码接口
    AudioError GetError() const { return m_error; }
    std::wstring GetErrorMessage() const;

private:
    // BASS 同步回调：歌曲播放结束
    static void CALLBACK EndSyncProc(HSYNC handle, DWORD channel, DWORD data, void* user);
    // BASS 同步回调：淡出滑动完成
    static void CALLBACK FadeSyncProc(HSYNC handle, DWORD channel, DWORD data, void* user);

    // 将BASS错误码映射为内部错误码
    static AudioError MapBassError(int bassCode);

    HSTREAM m_stream;       // BASS 音频流句柄
    HSYNC   m_endSync;      // 结束同步器句柄
    int     m_volume;       // 音量 0-100

    // ---- 音量平衡 ----
    bool        m_balanceEnabled;   // 是否启用音量平衡
    float       m_songGain;         // 当前歌曲的平衡增益 (线性 0~1, 乘在单曲音量上)
    std::wstring m_currentPath;     // 当前已加载歌曲路径

    struct LoudnessEntry {
        double    lufs;             // 集成响度 (LUFS, 负数)
        ULONGLONG size;             // 文件大小, 用于缓存失效判断
        FILETIME  mtime;            // 文件修改时间, 用于缓存失效判断
    };
    std::map<std::wstring, LoudnessEntry> m_loudnessCache;  // 路径 → 响度缓存
    bool m_cacheLoaded;

    void LoadLoudnessCache();
    void SaveLoudnessCache();
    double MeasureLoudnessLUFS(const std::wstring& filePath);
    double GetLoudnessLUFS(const std::wstring& filePath);
    float ComputeGainFromLUFS(double lufs);
    bool    m_playing;      // 是否正在播放
    bool    m_paused;       // 是否已暂停
    PlayMode m_playMode;    // 当前播放模式
    double m_speed;         // 当前倍速 (1.0 = 正常)
    HWND    m_notifyHwnd;   // 通知窗口
    HWND    m_fadeHwnd;     // 淡出通知窗口
    UINT    m_fadeMsg;      // 淡出完成消息
    bool    m_fading;       // 是否正在淡出
    HSYNC   m_fadeSync;     // 淡出同步器句柄
    UINT    m_notifyMsg;    // 通知消息
    AudioError m_error;     // 最后一次操作的错误码
};
