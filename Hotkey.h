// SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
#pragma once
#include <windows.h>
#include <string>

// ============================================
// 快捷键类型与辅助
// ============================================
// 供 MainWindow 的快捷键系统与快捷键对话框共用。
// ============================================

// 单个快捷键配置
struct HotkeyBinding {
    int id;
    const wchar_t* actionName;
    int vk;
    int mod;   // MOD_CONTROL / MOD_ALT 组合
};

// 快捷键键名 (持久化与显示共用)
extern const char* HK_KEY_NAMES[7];

// 显示用: "Ctrl+Left"
std::wstring HotkeyToString(int vk, int mod);
// 持久化编码/解码: "C+P" / "CA+Left"
std::wstring BindingToCode(int vk, int mod);
bool CodeToBinding(const std::wstring& code, int& vk, int& mod);
