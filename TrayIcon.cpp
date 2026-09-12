#include "TrayIcon.h"
#include "Resource.h"

namespace {
constexpr UINT kTrayId = 1;

// 记录 Shell_NotifyIcon 的结果与错误码
bool InvokeTray(DWORD message, NOTIFYICONDATAW& nid, DWORD& lastError) {
    BOOL ok = Shell_NotifyIconW(message, &nid);
    lastError = ok ? 0 : GetLastError();
    return ok != FALSE;
}

void FillDelete(NOTIFYICONDATAW& nid, HWND hwnd) {
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
}
}  // namespace

void TrayIcon::FillNid(NOTIFYICONDATAW& nid, const std::wstring& tip) {
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE;
    nid.dwState = 0;
    nid.dwStateMask = NIS_HIDDEN;   // 清除隐藏位, 强制显示在通知区而非被折叠隐藏
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    lstrcpynW(nid.szTip, tip.c_str(), 128);
}

void TrayIcon::Remove() {
    NOTIFYICONDATAW nid = {};
    FillDelete(nid, m_hwnd);
    InvokeTray(NIM_DELETE, nid, m_lastError);
    m_added = false;
}

bool TrayIcon::ReAdd(const std::wstring& tip) {
    // 先删掉 shell 里可能残留的同 ID 注册: NIM_ADD 无法覆盖它, 这是唤醒后卡死的关键
    NOTIFYICONDATAW del = {};
    FillDelete(del, m_hwnd);
    InvokeTray(NIM_DELETE, del, m_lastError);

    NOTIFYICONDATAW nid = {};
    FillNid(nid, tip);
    m_added = InvokeTray(NIM_ADD, nid, m_lastError);
    return m_added;
}

bool TrayIcon::UpdateTip(const std::wstring& tip) {
    // 先按"图标仍在"尝试更新; 不因 m_added 为 false 就跳过 —— shell 里可能仍有图标,
    // 而 NIM_MODIFY 才是恢复后最可能直接成功的路径
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = kTrayId;
    nid.uFlags = NIF_TIP;
    lstrcpynW(nid.szTip, tip.c_str(), 128);
    if (InvokeTray(NIM_MODIFY, nid, m_lastError)) {
        m_added = true;
        return true;
    }
    return ReAdd(tip);
}

bool TrayIcon::Reassert(const std::wstring& tip) {
    NOTIFYICONDATAW nid = {};
    FillNid(nid, tip);
    if (InvokeTray(NIM_MODIFY, nid, m_lastError)) {
        m_added = true;
        return true;
    }
    return ReAdd(tip);
}
