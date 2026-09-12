#pragma once
#include <string>
#include <vector>

// ============================================
// 文本文件读写 / 编码转换 (统一 UTF-8)
// ============================================
// 职责: 全项目共用的文本持久化与宽窄字符串转换, 取代此前散落在各模块里的
//       多份 WideToUtf8/Utf8ToWide 副本。
// 落盘统一为 UTF-8; 读取时自动识别历史遗留的 UTF-16LE BOM, 以便旧文件平滑升级。
// ============================================

// UTF-16 → UTF-8
std::string WideToUtf8(const std::wstring& ws);
// UTF-8 → UTF-16 (非法字节按 U+FFFD 替换)
std::wstring Utf8ToWide(const std::string& s);
// UTF-8 → UTF-16; 严格解析失败时回退按系统 ANSI 解码 (供 ID3 等来源不明的文本)
std::wstring Utf8OrAnsiToWide(const std::string& s);

// 读取整个文本文件: 识别 UTF-16LE BOM(FF FE) / UTF-8 BOM(EF BB BF) / 无 BOM 按 UTF-8。
// 文件不存在或过大返回空串。
std::wstring ReadTextAuto(const std::wstring& path);
// 按行读取 (去掉行尾 \r\n, 丢弃空行)
std::vector<std::wstring> ReadLinesAuto(const std::wstring& path);
// 以 UTF-8 写入整个文本, 覆盖原文件
bool WriteTextUtf8(const std::wstring& path, const std::wstring& text, bool withBom = false);

// 当前可执行文件所在目录 (无结尾反斜杠)
std::wstring GetExeDirectory();
