#include "AudioEngine.h"
#include <cstring>
#pragma warning(disable : 4996)

// 构造函数
AudioEngine::AudioEngine()
    : m_stream(0)
    , m_endSync(0)
    , m_volume(80)
    , m_playing(false)
    , m_paused(false)
    , m_playMode(PlayMode::Sequential)
    , m_notifyHwnd(NULL)
    , m_notifyMsg(0)
    , m_error(AudioError::Success)
{
}

// 析构函数
AudioEngine::~AudioEngine() {
    Cleanup();
}

// 初始化 BASS 音频引擎=
bool AudioEngine::Initialize(HWND hwnd) {
    // 使用默认音频设备，44.1kHz采样率
    if (!BASS_Init(-1, 44100, 0, hwnd, NULL)) {
        m_error = MapBassError(BASS_ErrorGetCode());
        return false;
    }
    m_error = AudioError::Success;
    // 设置全局音量，BASS_CONFIG_GVOL_STREAM 范围 0-10000
    BASS_SetConfig(BASS_CONFIG_GVOL_STREAM, m_volume * 100);
    return true;
}

// 清理 BASS 资源
void AudioEngine::Cleanup() {
    Stop();
    if (m_stream) {
        BASS_StreamFree(m_stream);
        m_stream = 0;
    }
    BASS_Free();
}

// 加载音频文件（流式解码，不加载到内存）
bool AudioEngine::Load(const std::wstring& filePath) {
    Stop();

    // 释放前一个流（如果未被 AUTOFREE 自动释放）
    if (m_stream) {
        BASS_StreamFree(m_stream);
        m_stream = 0;
    }

    // 创建流式解码，BASS_STREAM_AUTOFREE 在播放结束后自动释放
    m_stream = BASS_StreamCreateFile(FALSE, filePath.c_str(), 0, 0,
        BASS_STREAM_AUTOFREE | BASS_UNICODE);

    if (!m_stream) {
        m_error = MapBassError(BASS_ErrorGetCode());
        return false;
    }

    m_error = AudioError::Success;

    // 设置音量
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, m_volume / 100.0f);

    // 设置播放结束同步回调
    m_endSync = BASS_ChannelSetSync(m_stream, BASS_SYNC_END, 0, EndSyncProc, this);

    m_playing = false;
    m_paused = false;
    return true;
}

// 卸载当前文件
void AudioEngine::Unload() {
    Stop();
    if (m_stream) {
        BASS_StreamFree(m_stream);
        m_stream = 0;
    }
}

// 播放
void AudioEngine::Play() {
    if (!m_stream) return;
    BASS_ChannelPlay(m_stream, FALSE);
    m_playing = true;
    m_paused = false;
}

// 暂停
void AudioEngine::Pause() {
    if (!m_stream) return;
    BASS_ChannelPause(m_stream);
    m_playing = false;
    m_paused = true;
}

// 停止
void AudioEngine::Stop() {
    if (!m_stream) return;
    BASS_ChannelStop(m_stream);
    m_playing = false;
    m_paused = false;
}

// 音量控制 0-100
void AudioEngine::SetVolume(int volume) {
    m_volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
    BASS_SetConfig(BASS_CONFIG_GVOL_STREAM, (DWORD)(100 * m_volume));
    if (m_stream) {
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, m_volume / 100.0f);
    }
}

// 获取当前播放位置（秒）
double AudioEngine::GetPosition() const {
    if (!m_stream) return 0.0;
    QWORD bytes = BASS_ChannelGetPosition(m_stream, BASS_POS_BYTE);
    if (bytes == 0) return 0.0;
    return BASS_ChannelBytes2Seconds(m_stream, bytes);
}

// 获取音频总长度（秒）
double AudioEngine::GetLength() const {
    if (!m_stream) return 0.0;
    QWORD bytes = BASS_ChannelGetLength(m_stream, BASS_POS_BYTE);
    if (bytes == 0) return 0.0;
    return BASS_ChannelBytes2Seconds(m_stream, bytes);
}

// 跳转到指定位置（秒）
void AudioEngine::SetPosition(double seconds) {
    if (!m_stream) return;
    QWORD bytes = BASS_ChannelSeconds2Bytes(m_stream, seconds);
    BASS_ChannelSetPosition(m_stream, bytes, BASS_POS_BYTE);
}

// 循环切换播放模式
PlayMode AudioEngine::CyclePlayMode() {
    switch (m_playMode) {
        case PlayMode::Sequential: m_playMode = PlayMode::RepeatOne; break;
        case PlayMode::RepeatOne:  m_playMode = PlayMode::Shuffle;    break;
        case PlayMode::Shuffle:    m_playMode = PlayMode::Sequential; break;
    }
    return m_playMode;
}

// 读取音频元数据（延迟加载策略：只在播放时读取）
std::wstring AudioEngine::GetFormattedMetadata() const {
    if (!m_stream) return L"";

    std::string artist, title;

    // --- 尝试 FLAC Vorbis Comments (UTF-8) ---
    const char* meta = (const char*)BASS_ChannelGetTags(m_stream, BASS_TAG_META);
    if (meta && *meta) {
        while (*meta) {
            const char* eq = strchr(meta, '=');
            if (eq) {
                std::string key(meta, eq - meta);
                std::string val(eq + 1);
                if (_stricmp(key.c_str(), "ARTIST") == 0) {
                    artist = val;
                } else if (_stricmp(key.c_str(), "TITLE") == 0) {
                    title = val;
                }
            }
            meta += strlen(meta) + 1;
        }
    }

    // --- 尝试 ID3v1 (系统编码，中文环境下为 GBK) ---
    if (artist.empty() && title.empty()) {
        const char* id3 = (const char*)BASS_ChannelGetTags(m_stream, BASS_TAG_ID3);
        if (id3 && memcmp(id3, "TAG", 3) == 0) {
            auto trimField = [](const char* data, int maxLen) {
                std::string s;
                s.reserve(maxLen);
                for (int i = 0; i < maxLen && data[i] && data[i] != '\0'; ++i) {
                    if (data[i] != ' ' || !s.empty()) s += data[i];
                }
                while (!s.empty() && s.back() == ' ') s.pop_back();
                return s;
            };
            std::string a = trimField(id3 + 33, 30);
            std::string t = trimField(id3 + 3, 30);
            if (!a.empty() || !t.empty()) {
                artist = a;
                title = t;
            }
        }
    }

    // --- 编码转换：UTF-8 → UTF-16, 失败时回退到系统编码 ---
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return L"";
        // 尝试 UTF-8
        int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                      s.c_str(), -1, NULL, 0);
        if (len > 0) {
            std::wstring ws(static_cast<size_t>(len) - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
            return ws;
        }
        // 回退到系统 ANSI 编码
        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::wstring ws(static_cast<size_t>(len) - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &ws[0], len);
        return ws;
    };

    std::wstring wa = toWide(artist);
    std::wstring wt = toWide(title);

    if (!wa.empty() && !wt.empty()) return wa + L" - " + wt;
    if (!wt.empty())  return wt;
    if (!wa.empty())  return wa;
    return L"";
}

// 通知：歌曲已播放结束（AUTOFREE已释放流）
// 由主窗口在收到 WM_USER_SONG_END 时调用
void AudioEngine::NotifyEndOfSong() {
    m_stream = 0;   // AUTOFREE 已释放
    m_playing = false;
    m_paused = false;
}

// 静态回调：BASS 播放结束同步
// 在线程上下文中调用，仅做 PostMessage
void CALLBACK AudioEngine::EndSyncProc(HSYNC /*handle*/, DWORD /*channel*/,
                                        DWORD /*data*/, void* user) {
    AudioEngine* engine = static_cast<AudioEngine*>(user);
    if (engine && engine->m_notifyHwnd) {
        PostMessage(engine->m_notifyHwnd, engine->m_notifyMsg, 0, 0);
    }
}

AudioError AudioEngine::MapBassError(int bassCode) {
    switch (bassCode) {
        case BASS_ERROR_FILEOPEN: return AudioError::FileNotFound;
        case BASS_ERROR_FILEFORM: return AudioError::UnsupportedFormat;
        case BASS_ERROR_CODEC:    return AudioError::MissingCodec;
        case BASS_ERROR_FORMAT:   return AudioError::UnsupportedParam;
        case BASS_ERROR_NOTAUDIO: return AudioError::UnsupportedFormat;
        case BASS_ERROR_DECODE:   return AudioError::DecodeFailed;
        case BASS_ERROR_INIT:     return AudioError::InitFailed;
        case BASS_ERROR_MEM:      return AudioError::Unknown;
        case BASS_ERROR_ILLPARAM: return AudioError::UnsupportedParam;
        default:                  return AudioError::Unknown;
    }
}

std::wstring AudioEngine::GetErrorMessage() const {
    switch (m_error) {
        case AudioError::Success:           return L"";
        case AudioError::FileNotFound:      return L"文件不存在或无法访问";
        case AudioError::UnsupportedFormat: return L"不支持此音频格式";
        case AudioError::MissingCodec:      return L"缺少所需的解码器";
        case AudioError::UnsupportedParam:  return L"不支持的音频格式参数";
        case AudioError::DecodeFailed:      return L"解码失败";
        case AudioError::InitFailed:        return L"音频引擎未初始化";
        case AudioError::Unknown:           return L"未知错误";
    }
    return L"未知错误";
}
