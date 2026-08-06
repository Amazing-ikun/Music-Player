#include "AudioEngine.h"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#pragma warning(disable : 4996)

static bool HasExtension(const std::wstring& path, const wchar_t* ext) {
    size_t dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring e = path.substr(dot);
    for (auto& c : e) c = towlower(c);
    return e == ext;
}

// UTF-8 → UTF-16, fallback to system ANSI
static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, NULL, 0);
    if (len > 0) {
        std::wstring ws(static_cast<size_t>(len) - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
        return ws;
    }
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
    if (len <= 0) return L"";
    std::wstring ws(static_cast<size_t>(len) - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

// UTF-16 → UTF-8
static std::string ToUtf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string s(static_cast<size_t>(len) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, NULL, NULL);
    return s;
}

static std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* last = wcsrchr(path, L'\\');
    if (last) *last = L'\0';
    return path;
}

// ============================================
// EBU R128 / BS.1770 响度测量
// 每个声道级联 K-weighting 高通 + 高频搁架两个双二阶滤波器,
// 按 400ms 分块计算均方, 再做绝对门限 (-70 LUFS) 与相对门限 (-10 LU) 的两遍门控。
// ============================================
struct Biquad {
    double b0, b1, b2, a1, a2;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double Process(double x) {
        double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

static void MakeKHighpass(Biquad& f, double fs) {
    const double f0 = 38.13547087602444;
    double w0 = 2.0 * 3.14159265358979323846 * f0 / fs;
    double alpha = sin(w0) / 2.0;
    double cs = cos(w0);
    double a0 = 1.0 + alpha;
    f.b0 = (1.0 + cs) / 2.0 / a0;
    f.b1 = -(1.0 + cs) / a0;
    f.b2 = (1.0 + cs) / 2.0 / a0;
    f.a1 = -2.0 * cs / a0;
    f.a2 = (1.0 - alpha) / a0;
}

static void MakeKHighShelf(Biquad& f, double fs) {
    const double f0 = 1681.974450955533;
    const double gdB = 4.0;
    double A = pow(10.0, gdB / 40.0);
    double w0 = 2.0 * 3.14159265358979323846 * f0 / fs;
    double alpha = sin(w0) / 2.0 * sqrt(2.0);   // S = 1
    double cs = cos(w0);
    double t1 = A + 1.0, t2 = A - 1.0;
    double sqrtA = sqrt(A);
    double a0 = t1 - t2 * cs + 2.0 * sqrtA * alpha;
    f.b0 = A * (t1 + t2 * cs + 2.0 * sqrtA * alpha) / a0;
    f.b1 = -2.0 * A * (t2 + t1 * cs) / a0;
    f.b2 = A * (t1 + t2 * cs - 2.0 * sqrtA * alpha) / a0;
    f.a1 = 2.0 * (t2 - t1 * cs) / a0;
    f.a2 = (t1 - t2 * cs - 2.0 * sqrtA * alpha) / a0;
}

// ID3v1 tag reader via BASS
struct ID3Reader {
    struct Result {
        std::string artist;
        std::string title;
        bool valid = false;
    };

    static Result Read(HSTREAM stream) {
        Result r;
        const char* id3 = (const char*)BASS_ChannelGetTags(stream, BASS_TAG_ID3);
        if (!id3 || memcmp(id3, "TAG", 3) != 0) return r;

        auto trimField = [](const char* data, int maxLen) {
            std::string s;
            s.reserve(maxLen);
            for (int i = 0; i < maxLen && data[i] && data[i] != '\0'; ++i) {
                if (data[i] != ' ' || !s.empty()) s += data[i];
            }
            while (!s.empty() && s.back() == ' ') s.pop_back();
            return s;
        };

        r.artist = trimField(id3 + 33, 30);
        r.title  = trimField(id3 + 3, 30);
        r.valid  = !r.artist.empty() || !r.title.empty();
        return r;
    }
};

// 构造函数
AudioEngine::AudioEngine()
    : m_stream(0)
    , m_endSync(0)
    , m_volume(80)
    , m_playing(false)
    , m_paused(false)
    , m_playMode(PlayMode::Sequential)
    , m_speed(1.0)
    , m_notifyHwnd(NULL)
    , m_notifyMsg(0)
    , m_fadeHwnd(NULL)
    , m_fadeMsg(0)
    , m_fading(false)
    , m_fadeSync(0)
    , m_error(AudioError::Success)
    , m_balanceEnabled(false)
    , m_songGain(1.0f)
    , m_cacheLoaded(false)
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
    // 检查 BASS_FX 可用
    if (HIWORD(BASS_FX_GetVersion()) != BASSVERSION) {
        m_error = AudioError::MissingCodec;
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

    m_currentPath = filePath;
    m_songGain = 1.0f;   // 待 MainWindow 调用 ApplyBalance() 后按歌曲响度覆盖

    // 释放前一个流（如果未被 AUTOFREE 自动释放）
    m_fading = false;
    m_fadeSync = 0;
    if (m_stream) {
        BASS_StreamFree(m_stream);
        m_stream = 0;
    }

    // 检查文件是否为空
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fad)) {
        if (fad.nFileSizeHigh == 0 && fad.nFileSizeLow == 0) {
            m_error = AudioError::FileNotFound;
            return false;
        }
    }

    // 创建临时解码流，然后用 BASS_FX_TempoCreate 包装以实现变速不变调
    HSTREAM decoder = BASS_StreamCreateFile(FALSE, filePath.c_str(), 0, 0,
        BASS_STREAM_DECODE | BASS_UNICODE);
    if (!decoder) {
        m_error = MapBassError(BASS_ErrorGetCode());
        // 如果是 FLAC 文件且解码器缺失，给出更明确的提示
        if ((m_error == AudioError::UnsupportedFormat || m_error == AudioError::UnsupportedParam)
            && HasExtension(filePath, L".flac")) {
            if (!BASS_PluginLoad(L"bassflac.dll", 0)) {
                m_error = AudioError::MissingCodec;
            }
        }
        return false;
    }

    m_stream = BASS_FX_TempoCreate(decoder, BASS_FX_FREESOURCE | BASS_STREAM_AUTOFREE);
    if (!m_stream) {
        m_error = MapBassError(BASS_ErrorGetCode());
        BASS_StreamFree(decoder);
        return false;
    }

    // 应用当前倍速
    double tempo = (m_speed - 1.0) * 100.0;
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_TEMPO, tempo);

    m_error = AudioError::Success;

    // 设置单曲音量: chvol 承载平衡增益 (用户音量走全局 gvol)
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, m_songGain);

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
// 用户主音量只作用于全局 BASS_CONFIG_GVOL_STREAM, 单曲 BASS_ATTRIB_VOL 承载平衡增益。
// 这样显示 80% 就是真实 80% (修复原先 gvol×chvol 的双重衰减), 且静音/调节不干扰平衡增益。
void AudioEngine::SetVolume(int volume) {
    m_volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
    BASS_SetConfig(BASS_CONFIG_GVOL_STREAM, (DWORD)(100 * m_volume));
}

// ============================================
// 音量平衡: 按歌曲响度归一化
// ============================================
void AudioEngine::SetBalanceEnabled(bool enabled) {
    m_balanceEnabled = enabled;
    ApplyBalance();
}

void AudioEngine::ApplyBalance() {
    float gain = 1.0f;
    if (m_balanceEnabled && !m_currentPath.empty()) {
        double lufs = GetLoudnessLUFS(m_currentPath);
        gain = ComputeGainFromLUFS(lufs);
    }
    m_songGain = gain;
    if (m_stream) {
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, gain);
    }
}

// 目标响度 -14 LUFS (与主流播放器一致), 增益限幅 ±15 dB。
// 抬升方向上限 1.0: BASS 单曲音量不能超过满音量, 安静歌曲最高抬到用户音量水平。
float AudioEngine::ComputeGainFromLUFS(double lufs) {
    const double targetLUFS = -14.0;
    double gainDb = targetLUFS - lufs;
    if (gainDb > 15.0) gainDb = 15.0;
    if (gainDb < -15.0) gainDb = -15.0;
    double lin = pow(10.0, gainDb / 20.0);
    if (lin < 0.01) lin = 0.01;
    if (lin > 1.0) lin = 1.0;
    return (float)lin;
}

// 读取 .loudness.txt 响度缓存
void AudioEngine::LoadLoudnessCache() {
    m_loudnessCache.clear();
    std::wstring filePath = ExeDir() + L"\\.loudness.txt";
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, NULL);
    if (size > 0 && size < 16 * 1024 * 1024) {
        std::vector<char> buf(size + 1, 0);
        DWORD read;
        if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
            char* p = buf.data();
            while (*p) {
                char* nl = strchr(p, '\n');
                if (!nl) nl = p + strlen(p);
                *nl = '\0';
                if (*p && *p != '\r') {
                    // 格式: lufs<TAB>size<TAB>timeHigh<TAB>timeLow<TAB>path(UTF-8)
                    std::string line(p);
                    size_t t1 = line.find('\t');
                    size_t t2 = line.find('\t', t1 + 1);
                    size_t t3 = line.find('\t', t2 + 1);
                    size_t t4 = line.find('\t', t3 + 1);
                    if (t4 != std::string::npos) {
                        LoudnessEntry e;
                        e.lufs  = atof(line.substr(0, t1).c_str());
                        e.size  = (ULONGLONG)strtoull(line.substr(t1 + 1, t2 - t1 - 1).c_str(), NULL, 10);
                        e.mtime.dwHighDateTime = (DWORD)strtoul(line.substr(t2 + 1, t3 - t2 - 1).c_str(), NULL, 10);
                        e.mtime.dwLowDateTime  = (DWORD)strtoul(line.substr(t3 + 1, t4 - t3 - 1).c_str(), NULL, 10);
                        std::wstring path = ToWide(line.substr(t4 + 1));
                        if (!path.empty()) m_loudnessCache[path] = e;
                    }
                }
                p = nl + 1;
            }
        }
    }
    CloseHandle(hFile);
}

// 写回整个 .loudness.txt 缓存
void AudioEngine::SaveLoudnessCache() {
    std::wstring filePath = ExeDir() + L"\\.loudness.txt";
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    std::string all;
    all.reserve(m_loudnessCache.size() * 96);
    char tmp[128];
    for (const auto& kv : m_loudnessCache) {
        sprintf(tmp, "%.2f\t%llu\t%lu\t%lu\t", kv.second.lufs,
                (unsigned long long)kv.second.size,
                (unsigned long)kv.second.mtime.dwHighDateTime,
                (unsigned long)kv.second.mtime.dwLowDateTime);
        all += tmp;
        all += ToUtf8(kv.first);
        all += "\n";
    }
    DWORD written;
    WriteFile(hFile, all.data(), (DWORD)all.size(), &written, NULL);
    CloseHandle(hFile);
}

// 取歌曲响度: 缓存命中(且文件未变)直接返回, 否则测量并写回缓存
double AudioEngine::GetLoudnessLUFS(const std::wstring& filePath) {
    if (!m_cacheLoaded) {
        LoadLoudnessCache();
        m_cacheLoaded = true;
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    ULONGLONG sz = 0;
    FILETIME mt = {};
    bool fileOk = GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fad);
    if (fileOk) {
        sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        mt = fad.ftLastWriteTime;
    }

    auto it = m_loudnessCache.find(filePath);
    if (fileOk && it != m_loudnessCache.end()
        && it->second.size == sz
        && it->second.mtime.dwHighDateTime == mt.dwHighDateTime
        && it->second.mtime.dwLowDateTime == mt.dwLowDateTime) {
        return it->second.lufs;
    }

    double lufs = MeasureLoudnessLUFS(filePath);
    if (fileOk) {
        LoudnessEntry e;
        e.lufs = lufs; e.size = sz; e.mtime = mt;
        m_loudnessCache[filePath] = e;
        SaveLoudnessCache();
    }
    return lufs;
}

// 独立解码流测量整曲响度 (EBU R128 两遍门控)
double AudioEngine::MeasureLoudnessLUFS(const std::wstring& filePath) {
    HSTREAM decoder = BASS_StreamCreateFile(FALSE, filePath.c_str(), 0, 0,
        BASS_STREAM_DECODE | BASS_UNICODE);
    if (!decoder) return -70.0;

    BASS_CHANNELINFO ci;
    if (!BASS_ChannelGetInfo(decoder, &ci) || ci.chans < 1) {
        BASS_StreamFree(decoder);
        return -70.0;
    }
    int fs = ci.freq > 0 ? (int)ci.freq : 44100;
    int chans = ci.chans > 2 ? 2 : ci.chans;   // 多声道场景取前两声道即可

    std::vector<Biquad> hp(chans), hs(chans);
    for (int c = 0; c < chans; c++) {
        MakeKHighpass(hp[c], (double)fs);
        MakeKHighShelf(hs[c], (double)fs);
    }

    const int blockFrames = (int)(0.4 * fs);
    std::vector<float> pcm((size_t)blockFrames * chans);
    std::vector<double> blockZ;
    blockZ.reserve(1024);

    for (;;) {
        DWORD bytes = BASS_ChannelGetData(decoder, pcm.data(),
            (DWORD)(pcm.size() * sizeof(float)) | BASS_DATA_FLOAT);
        if (bytes == 0 || bytes == (DWORD)-1) break;
        int frames = (int)(bytes / sizeof(float)) / chans;
        if (frames <= 0) continue;

        double z = 0;
        for (int c = 0; c < chans; c++) {
            double sumSq = 0;
            for (int i = 0; i < frames; i++) {
                double x = pcm[(size_t)i * chans + c];
                double y = hs[c].Process(hp[c].Process(x));
                sumSq += y * y;
            }
            z += sumSq / frames;
        }
        blockZ.push_back(z);
    }
    BASS_StreamFree(decoder);

    if (blockZ.empty()) return -70.0;

    const double absGate = pow(10.0, -70.0 / 10.0);   // 绝对门限 -70 LUFS

    double sum = 0; int n = 0;
    for (double z : blockZ) if (z > absGate) { sum += z; n++; }
    if (n == 0) return -70.0;
    double absMean = sum / n;

    double relGate = absMean * pow(10.0, -10.0 / 10.0);   // 相对门限 -10 LU
    sum = 0; n = 0;
    for (double z : blockZ) if (z > relGate) { sum += z; n++; }
    double finalMean = (n > 0) ? sum / n : absMean;

    double lufs = -0.691 + 10.0 * log10(finalMean);
    if (lufs < -70.0) lufs = -70.0;
    return lufs;
}

// 获取当前播放位置（秒）
double AudioEngine::GetPosition() const {
    if (!m_stream) return 0.0;
    QWORD bytes = BASS_ChannelGetPosition(m_stream, BASS_POS_BYTE);
    QWORD len   = BASS_ChannelGetLength(m_stream, BASS_POS_BYTE);
    if (bytes == 0 || bytes > len) return 0.0;
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
    if (!m_stream || seconds < 0) return;
    double length = GetLength();
    if (length > 0 && seconds > length) seconds = length;
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
        auto r = ID3Reader::Read(m_stream);
        if (r.valid) {
            artist = r.artist;
            title = r.title;
        }
    }

    // --- 编码转换：UTF-8/ANSI → UTF-16 ---
    std::wstring wa = ToWide(artist);
    std::wstring wt = ToWide(title);

    if (!wa.empty() && !wt.empty()) return wa + L" - " + wt;
    if (!wt.empty())  return wt;
    if (!wa.empty())  return wa;
    return L"";
}

void AudioEngine::SetSpeed(double speed) {
    if (speed < 0.1) speed = 0.1;
    if (speed > 10.0) speed = 10.0;
    m_speed = speed;
    if (m_stream) {
        double tempo = (speed - 1.0) * 100.0;
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_TEMPO, tempo);
    }
}

void AudioEngine::PauseFade(DWORD fadeMs) {
    if (!m_stream || !m_playing || m_fading) return;
    m_fading = true;
    if (m_fadeSync) BASS_ChannelRemoveSync(m_stream, m_fadeSync);
    m_fadeSync = BASS_ChannelSetSync(m_stream, BASS_SYNC_SLIDE, 0, FadeSyncProc, this);
    BASS_ChannelSlideAttribute(m_stream, BASS_ATTRIB_VOL, 0, fadeMs);
}

void AudioEngine::PlayFade() {
    if (!m_stream) return;
    if (m_fading) {
        m_fading = false;
        if (m_fadeSync) {
            BASS_ChannelRemoveSync(m_stream, m_fadeSync);
            m_fadeSync = 0;
        }
    }
    float vol = m_songGain;
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, 0);
    BASS_ChannelPlay(m_stream, FALSE);
    BASS_ChannelSlideAttribute(m_stream, BASS_ATTRIB_VOL, vol, 200);
    m_playing = true;
    m_paused = false;
}

// 通知：歌曲已播放结束（AUTOFREE已释放流）
// 由主窗口在收到 WM_USER_SONG_END 时调用
void AudioEngine::NotifyEndOfSong() {
    m_stream = 0;   // AUTOFREE 已释放
    m_playing = false;
    m_paused = false;
    m_fading = false;
    m_fadeSync = 0;
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

// 淡出同步：音量滑到 0 后通知主线程执行暂停操作
// 注意：BASS 同步回调中禁止调用 BASS API，仅做 PostMessage
void CALLBACK AudioEngine::FadeSyncProc(HSYNC, DWORD, DWORD, void* user) {
    AudioEngine* engine = static_cast<AudioEngine*>(user);
    if (!engine) return;
    if (engine->m_fadeHwnd) {
        PostMessage(engine->m_fadeHwnd, engine->m_fadeMsg, 0, 0);
    }
}

// 在主线程中处理淡出完成（由 WM_APP_FADE_DONE 触发）
void AudioEngine::OnFadeComplete() {
    if (!m_fading || !m_fadeSync) return; // 已被 PlayFade() 取消
    m_fading = false;
    m_fadeSync = 0;
    if (!m_stream) return;
    BASS_ChannelPause(m_stream);
    float vol = m_songGain;
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, vol);
    m_playing = false;
    m_paused = true;
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
        case AudioError::MissingCodec:      return L"缺少所需的解码器 (bassflac.dll)";
        case AudioError::UnsupportedParam:  return L"不支持的音频格式参数";
        case AudioError::DecodeFailed:      return L"解码失败";
        case AudioError::InitFailed:        return L"音频引擎未初始化";
        case AudioError::Unknown:           return L"未知错误";
    }
    return L"未知错误";
}
