// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
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

// ---- 状态文件目录 (Data\) 与统一后缀 (.mpdf) ----
// 所有状态文件统一放在 exe 目录下的 Data\ 里、以 .mpdf 为后缀, 不再散落成 exe 旁的 .txt。
// 后缀自定义是为了让系统不再把它们关联到文本编辑器, 避免用户随手双击改坏。
// 数据目录不可写时回退到 exe 目录; 调用方可用 GetDataDirectory() == GetExeDirectory() 判断是否发生了回退。
std::wstring GetDataDirectory();             // exe\Data (必要时创建); 不可用则返回 exe 目录
std::wstring DataFile(const wchar_t* name);  // 数据目录下 name + ".mpdf" 的全路径
// 把旧版散落在 exe 目录的 .<name>.txt 一次性搬进 Data\(*.mpdf)。
// 旧文件不存在、或新文件已存在时均跳过, 绝不覆盖已有数据。
void MigrateLegacyStateFiles();
