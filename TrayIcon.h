#pragma once
#include <windows.h>
#include <string>

// ============================================
// 系统托盘图标
// ============================================
// 职责: 封装 NOTIFYICONDATA 的添加/删除/修改与 added 状态, 不依赖 MainWindow。
// 提示文字由调用方传入, 心跳重断言(Reassert)也在此封装。
// ============================================

class TrayIcon {
public:
    TrayIcon() {}
    void SetTarget(HINSTANCE hInst, HWND hwnd) { m_hInst = hInst; m_hwnd = hwnd; }
    bool IsAdded() const { return m_added; }

    void Add(const std::wstring& tip);          // NIM_ADD
    void Remove();                              // NIM_DELETE
    void UpdateTip(const std::wstring& tip);    // 仅更新提示文字 (NIM_MODIFY)
    void Reassert(const std::wstring& tip);     // 心跳: NIM_MODIFY, 失败则 NIM_ADD

private:
    void FillNid(NOTIFYICONDATAW& nid, const std::wstring& tip);

    HINSTANCE m_hInst = NULL;
    HWND m_hwnd = NULL;
    bool m_added = false;
};
