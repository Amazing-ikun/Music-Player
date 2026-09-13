// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#include "AudioEngine.h"
#include "TextFile.h"
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

// 解码 ID3v2 文本帧内容 (data[0] 为编码字节)
static std::wstring DecodeId3Text(const char* data, DWORD size) {
    if (size == 0) return L"";
    BYTE enc = (BYTE)data[0];
    const char* p = data + 1;
    DWORD n = size - 1;
    std::wstring out;

    if (enc == 0x01 || enc == 0x02) {          // UTF-16: 01 带 BOM, 02 大端
        bool be = (enc == 0x02);
        if (enc == 0x01 && n >= 2) {
            if ((BYTE)p[0] == 0xFF && (BYTE)p[1] == 0xFE) { be = false; p += 2; n -= 2; }
            else if ((BYTE)p[0] == 0xFE && (BYTE)p[1] == 0xFF) { be = true; p += 2; n -= 2; }
        }
        out.resize(n / 2);
        for (size_t i = 0; i < out.size(); ++i) {
            BYTE a = (BYTE)p[i * 2], b = (BYTE)p[i * 2 + 1];
            out[i] = be ? (wchar_t)((a << 8) | b) : (wchar_t)((b << 8) | a);
        }
        while (!out.empty() && out.back() == L'\0') out.pop_back();
        return out;
    }

    // 单字节编码: 03 = UTF-8; 00 规范上是 ISO-8859-1, 但实际文件多为系统编码,
    // 与 ID3v1 的处理保持一致按 ANSI 解
    std::string s(p, n);
    size_t nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);
    if (s.empty()) return L"";
    UINT cp = (enc == 0x03) ? CP_UTF8 : CP_ACP;
    int len = MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), NULL, 0);
    if (len <= 0) return L"";
    out.resize(len);
    MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), &out[0], len);
    return out;
}

// 解析 ID3v2.3/2.4 标签块, 取标题(TIT2)与歌手(TPE1); 结果以 UTF-8 写回
static void ParseId3v2(const char* tag, std::string& artist, std::string& title) {
    if (!tag || memcmp(tag, "ID3", 3) != 0) return;
    BYTE version = (BYTE)tag[3];
    DWORD size = ((DWORD)(BYTE)tag[6] << 21) | ((DWORD)(BYTE)tag[7] << 14)
               | ((DWORD)(BYTE)tag[8] << 7) | (BYTE)tag[9];
    const char* p = tag + 10;
    const char* end = p + size;

    while (p + 10 <= end) {
        if (p[0] == '\0') break;                       // 其后为填充
        char id[5] = { p[0], p[1], p[2], p[3], '\0' };
        DWORD fsz;
        if (version >= 4)                              // v2.4: syncsafe
            fsz = ((DWORD)(BYTE)p[4] << 21) | ((DWORD)(BYTE)p[5] << 14)
                | ((DWORD)(BYTE)p[6] << 7) | (BYTE)p[7];
        else                                           // v2.3: 大端
            fsz = ((DWORD)(BYTE)p[4] << 24) | ((DWORD)(BYTE)p[5] << 16)
                | ((DWORD)(BYTE)p[6] << 8) | (BYTE)p[7];
        const char* data = p + 10;
        if (fsz == 0 || data + fsz > end) break;

        if (strcmp(id, "TIT2") == 0 || strcmp(id, "TPE1") == 0) {
            std::wstring text = DecodeId3Text(data, fsz);
            if (!text.empty()) {
                std::string utf8 = WideToUtf8(text);
                if (strcmp(id, "TIT2") == 0) title = utf8;
                else                         artist = utf8;
            }
        }
        p = data + fsz;
    }
}

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
    , m_fadeDeadline(0)
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
    // 让 BASS 跟随系统默认输出设备的变化 (合盖休眠、拔插耳机、接入扩展坞都可能切换默认设备)
    BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, TRUE);
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

// 读取 Data\loudness.mpdf 响度缓存
void AudioEngine::LoadLoudnessCache() {
    m_loudnessCache.clear();
    std::wstring filePath = DataFile(L"loudness");
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
                        std::wstring path = Utf8ToWide(line.substr(t4 + 1));
                        if (!path.empty()) m_loudnessCache[path] = e;
                    }
                }
                p = nl + 1;
            }
        }
    }
    CloseHandle(hFile);
}

// 写回整个 Data\loudness.mpdf 缓存
void AudioEngine::SaveLoudnessCache() {
    std::wstring filePath = DataFile(L"loudness");
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
        all += WideToUtf8(kv.first);
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

// 独立解码流探测文件时长(秒), 不发声, 不影响正在播放的流
double AudioEngine::ProbeDuration(const std::wstring& filePath) {
    HSTREAM decoder = BASS_StreamCreateFile(FALSE, filePath.c_str(), 0, 0,
        BASS_STREAM_DECODE | BASS_UNICODE);
    if (!decoder) return 0.0;
    QWORD bytes = BASS_ChannelGetLength(decoder, BASS_POS_BYTE);
    double secs = (bytes != 0) ? BASS_ChannelBytes2Seconds(decoder, bytes) : 0.0;
    BASS_StreamFree(decoder);
    return (secs > 0) ? secs : 0.0;
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

    // --- FLAC / Ogg 的 Vorbis 注释 (UTF-8) ---
    const char* meta = (const char*)BASS_ChannelGetTags(m_stream, BASS_TAG_OGG);
    if (meta && *meta) {
        while (*meta) {
            const char* eq = strchr(meta, '=');
            if (eq) {
                std::string key(meta, eq - meta);
                std::string val(eq + 1);
                if (_stricmp(key.c_str(), "ARTIST") == 0 && artist.empty()) {
                    artist = val;
                } else if (_stricmp(key.c_str(), "TITLE") == 0 && title.empty()) {
                    title = val;
                }
            }
            meta += strlen(meta) + 1;
        }
    }

    // --- ID3v2 (MP3): TIT2/TPE1 ---
    if (artist.empty() || title.empty()) {
        ParseId3v2((const char*)BASS_ChannelGetTags(m_stream, BASS_TAG_ID3V2), artist, title);
    }

    // --- ID3v1 兜底 (系统编码，中文环境下为 GBK) ---
    if (artist.empty() || title.empty()) {
        auto r = ID3Reader::Read(m_stream);
        if (r.valid) {
            if (artist.empty()) artist = r.artist;
            if (title.empty())  title = r.title;
        }
    }

    // --- 编码转换：UTF-8/ANSI → UTF-16 ---
    std::wstring wa = Utf8OrAnsiToWide(artist);
    std::wstring wt = Utf8OrAnsiToWide(title);

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
    // 留 1 秒余量: 正常情况下滑动同步会先到; 超时说明回调丢失(休眠/设备丢失), 由兜底收尾
    m_fadeDeadline = GetTickCount64() + fadeMs + 1000;
    if (m_fadeSync) BASS_ChannelRemoveSync(m_stream, m_fadeSync);
    m_fadeSync = BASS_ChannelSetSync(m_stream, BASS_SYNC_SLIDE, 0, FadeSyncProc, this);
    BASS_ChannelSlideAttribute(m_stream, BASS_ATTRIB_VOL, 0, fadeMs);
}

void AudioEngine::PlayFade() {
    if (!m_stream) return;
    if (m_fading) {
        m_fading = false;
        m_fadeDeadline = 0;
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
    m_fadeDeadline = 0;
    if (!m_stream) return;
    BASS_ChannelPause(m_stream);
    float vol = m_songGain;
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, vol);
    m_playing = false;
    m_paused = true;
}

// 以 BASS 的真实状态为准校正播放/暂停标志
// m_playing/m_paused 只在应用主动调用 Play/Pause 等时才变, 与设备实际状态无关;
// 设备丢失或休眠唤醒后二者可能永久脱节, 这里负责重新对账。
bool AudioEngine::SyncStateFromBass() {
    if (!m_stream) {
        bool changed = m_playing || m_paused || m_fading;
        m_playing = false;
        m_paused = false;
        m_fading = false;
        m_fadeSync = 0;
        return changed;
    }
    if (m_fading) return false;   // 淡出进行中, 最终状态由淡出回调决定

    DWORD state = BASS_ChannelIsActive(m_stream);
    if (state == BASS_ACTIVE_STOPPED && BASS_ErrorGetCode() == BASS_ERROR_HANDLE)
        return false;             // 流已被 AUTOFREE 释放, 交给歌曲结束通知处理

    bool playing = (state == BASS_ACTIVE_PLAYING);
    bool paused  = (state == BASS_ACTIVE_PAUSED || state == BASS_ACTIVE_PAUSED_DEVICE);
    if (playing == m_playing && paused == m_paused) return false;

    m_playing = playing;
    m_paused  = paused;
    return true;
}

// 休眠唤醒后恢复输出
bool AudioEngine::ResumeAfterSuspend() {
    if (!m_stream) return false;

    // 淡出若在休眠中被中断, BASS 的滑动同步回调不会再触发 → 按"暂停"的意图收尾,
    // 否则 m_fading 会永久卡住, 播放/暂停判据也跟着失效。
    if (m_fading) {
        // 淡出若在休眠中被中断, BASS 的滑动同步回调不会再触发 → 按"暂停"的意图收尾,
        // 否则 m_fading 会永久卡住, 播放/暂停判据也跟着失效。
        FinishPendingPause();
        return false;
    }

    if (BASS_ChannelIsActive(m_stream) == BASS_ACTIVE_PLAYING)
        return true;   // 音频始终没断

    // 应用以为在播, 但设备已被系统停掉 / 流被挂起 → 重新起播(会重新获取输出设备)
    if (m_playing && BASS_ChannelPlay(m_stream, FALSE)) {
        m_paused = false;
        return true;
    }

    SyncStateFromBass();   // 恢复失败: 如实反映现状, 避免继续谎报"正在播放"
    return m_playing;
}

// 结束进行中的暂停淡出: 取消同步器, 立即暂停并还原音量, 并把状态定死为"已暂停"
bool AudioEngine::FinishPendingPause() {
    if (!m_fading) return false;
    m_fading = false;
    m_fadeDeadline = 0;
    if (m_fadeSync) {
        if (m_stream) BASS_ChannelRemoveSync(m_stream, m_fadeSync);
        m_fadeSync = 0;
    }
    if (m_stream) {
        BASS_ChannelPause(m_stream);
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_VOL, m_songGain);
    }
    m_playing = false;
    m_paused = true;
    return true;
}

// 暂停淡出是否已超过预期耗时仍未完成
bool AudioEngine::IsFadeStuck() const {
    return m_fading && m_fadeDeadline != 0 && GetTickCount64() >= m_fadeDeadline;
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
