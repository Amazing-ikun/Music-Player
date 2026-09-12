#pragma once
#include <windows.h>
#include <string>

// ============================================
// 系统托盘图标
// ============================================
// 职责: 封装 NOTIFYICONDATA 的添加/删除/修改与 added 状态, 不依赖 MainWindow。
// 提示文字由调用方传入, 心跳重断言(Reassert)也在此封装。
//
// 注意: m_added 只是"上次操作是否成功"的记录, 不能当作 shell 的真实状态。
// 系统休眠唤醒、资源管理器重启后, shell 里的图标可能仍在, 而一次瞬时失败会让
// m_added 变成 false; 若此后只尝试 NIM_ADD, 就会因为图标已存在而永远失败,
// 提示便冻结在旧内容上。因此更新一律先试 NIM_MODIFY, 失败才删除后重加。
// ============================================

class TrayIcon {
public:
    TrayIcon() {}
    void SetTarget(HINSTANCE hInst, HWND hwnd) { m_hInst = hInst; m_hwnd = hwnd; }
    bool IsAdded() const { return m_added; }
    DWORD LastError() const { return m_lastError; }   // 最近一次 Shell_NotifyIcon 失败的错误码

    void Remove();                              // NIM_DELETE
    bool UpdateTip(const std::wstring& tip);    // 仅更新提示 (NIM_MODIFY); 失败则强制重加
    bool Reassert(const std::wstring& tip);     // 心跳: 重申图标与可见状态; 失败则强制重加
    // 强制重加: 先无条件 NIM_DELETE 清掉 shell 里可能残留的旧注册, 再 NIM_ADD。
    // 仅靠 NIM_ADD 覆盖不了残留的同 ID 图标, 这是唤醒后卡死的根因。
    bool ReAdd(const std::wstring& tip);

private:
    void FillNid(NOTIFYICONDATAW& nid, const std::wstring& tip);

    HINSTANCE m_hInst = NULL;
    HWND m_hwnd = NULL;
    bool m_added = false;
    DWORD m_lastError = 0;
};
