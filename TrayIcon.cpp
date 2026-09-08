#include "TrayIcon.h"
#include "Resource.h"

void TrayIcon::FillNid(NOTIFYICONDATAW& nid, const std::wstring& tip) {
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE;
    nid.dwState = 0;
    nid.dwStateMask = NIS_HIDDEN;   // 清除隐藏位, 强制显示在通知区而非被折叠隐藏
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    lstrcpynW(nid.szTip, tip.c_str(), 128);
}

void TrayIcon::Add(const std::wstring& tip) {
    if (m_added) return;
    NOTIFYICONDATAW nid = {};
    FillNid(nid, tip);
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) return;
    m_added = true;
}

void TrayIcon::Remove() {
    if (!m_added) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_added = false;
}

void TrayIcon::UpdateTip(const std::wstring& tip) {
    if (!m_added) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    lstrcpynW(nid.szTip, tip.c_str(), 128);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::Reassert(const std::wstring& tip) {
    if (!m_added) { Add(tip); return; }
    NOTIFYICONDATAW nid = {};
    FillNid(nid, tip);
    if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) {
        m_added = false;
        Add(tip);
    }
}
