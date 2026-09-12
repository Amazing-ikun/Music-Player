#include "LyricWindow.h"
#include <gdiplus.h>

using namespace Gdiplus;

namespace {
    const wchar_t* kLyricClass  = L"MusicPlayerLyricWindow";
    const wchar_t* kFontFamily  = L"Microsoft YaHei";
    const UINT_PTR kFadeTimerId = 1;
    const UINT     kFadeDelayMs = 5000;
    const UINT_PTR kScrollTimerId = 2;
    const UINT     kScrollTickMs  = 40;
    const int      kScrollStep    = 2;
    const int      kScrollPausePx = 40;
    const int      kMinFontSize = 12;
    const int      kMaxFontSize = 72;
    const int      kPadX        = 24;
    const int      kMinWidth    = 200;   // 最小宽度(像素)
    const int      kHeightPad   = 8;     // 高度公式中的固定边距
    const int      kBorder      = 8;     // 边缘缩放热区宽度

    int HeightForFontSize(int first, int second) { return (int)(first * 1.5 + second * 1.25) + kHeightPad; }

    ULONG_PTR s_gdiplusToken = 0;
    void EnsureGdiplus() {
        if (s_gdiplusToken) return;
        GdiplusStartupInput input;
        GdiplusStartup(&s_gdiplusToken, &input, NULL);
    }

    // 绘制带阴影的文字 (阴影偏移 1px, 保证在任意桌面背景上可读)
    void DrawTextShadowed(Graphics& g, const std::wstring& text, Font& font,
                          const Color& color, StringFormat* fmt,
                          REAL x, REAL y, REAL w, REAL h) {
        if (text.empty()) return;
        RectF rc(x, y, w, h);
        RectF rcShadow(x + 1.0f, y + 1.0f, w, h);
        SolidBrush shadow(Color(180, 0, 0, 0));
        g.DrawString(text.c_str(), -1, &font, rcShadow, fmt, &shadow);
        SolidBrush brush(color);
        g.DrawString(text.c_str(), -1, &font, rc, fmt, &brush);
    }

    // 填充文字的命中矩形 (alpha=1, 肉眼不可见但可命中), 用于锁定态把可点击范围收紧到文字区域
    void FillHitRect(Graphics& g, const std::wstring& text, Font& font,
                     REAL bandX, REAL bandY, REAL bandW, REAL bandH) {
        if (text.empty()) return;
        RectF bound;
        g.MeasureString(text.c_str(), -1, &font, PointF(0.0f, 0.0f), &bound);
        REAL contentW = bandW - 2 * kPadX;
        REAL w = (bound.Width < contentW) ? bound.Width : contentW;
        REAL cx = bandX + (bandW - w) / 2.0f;
        SolidBrush hit(Color(1, 0, 0, 0));
        g.FillRectangle(&hit, cx, bandY, w, bandH);
    }
}

LyricWindow::LyricWindow()
    : m_hwnd(NULL), m_hInst(NULL), m_visible(false), m_currentIndex(-1),
      m_emptyText(L"暂无歌词"), m_color(RGB(255, 255, 255)), m_nextColor(RGB(150, 150, 150)), m_fontSize(30), m_secondFontSize(20),
      m_bgVisible(true), m_mouseTracking(false),
      m_scrolling(false), m_scrollOffset(0), m_scrollMax(0),
      m_locked(false), m_showTranslation(false),
      m_dib(NULL), m_dibBits(NULL), m_dibDC(NULL), m_dibOldBmp(NULL),
      m_dibW(0), m_dibH(0), m_listener(nullptr) {}

LyricWindow::~LyricWindow() {
    Destroy();
}

bool LyricWindow::Create(HINSTANCE hInst) {
    if (m_hwnd) return true;
    m_hInst = hInst;
    EnsureGdiplus();

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = kLyricClass;
    RegisterClassExW(&wc);   // 重复注册返回 0, 可忽略

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int w  = 700;
    int h  = HeightForFontSize(m_fontSize, m_secondFontSize);
    int x  = (sw - w) / 2;
    int y  = sh - h - 120;

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kLyricClass, L"", WS_POPUP,
        x, y, w, h, NULL, NULL, hInst, this);
    if (!m_hwnd) return false;

    RecalcSize();
    return true;
}

void LyricWindow::Destroy() {
    if (m_hwnd) { KillTimer(m_hwnd, kFadeTimerId); KillTimer(m_hwnd, kScrollTimerId); DestroyWindow(m_hwnd); m_hwnd = NULL; }
    FreeDib();
    m_visible = false;
    m_scrolling = false;
    m_scrollOffset = 0;
    m_lines.clear();
    m_currentIndex = -1;
}

void LyricWindow::Show() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    m_visible = true;
    m_bgVisible = true;
    SetTimer(m_hwnd, kFadeTimerId, kFadeDelayMs, NULL);
    Redraw();
}

void LyricWindow::Hide() {
    if (!m_hwnd) return;
    KillTimer(m_hwnd, kFadeTimerId);
    KillTimer(m_hwnd, kScrollTimerId);
    m_scrolling = false;
    m_scrollOffset = 0;
    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;
}

void LyricWindow::SetLyrics(const LyricParser& parser) {
    m_lines = parser.Lines();
    m_currentIndex = -1;
    m_scrollOffset = 0;
    if (m_hwnd && m_visible) Redraw();
}

void LyricWindow::SetCurrentIndex(int index) {
    if (index == m_currentIndex) return;
    m_currentIndex = index;
    m_scrollOffset = 0;   // 换行时重置滚动
    if (m_hwnd && m_visible) Redraw();
}

void LyricWindow::SetColor(COLORREF color) {
    m_color = color;
    if (m_hwnd && m_visible) Redraw();
}

void LyricWindow::SetNextColor(COLORREF color) {
    m_nextColor = color;
    if (m_hwnd && m_visible) Redraw();
}

void LyricWindow::SetFontSize(int px) {
    if (px < kMinFontSize) px = kMinFontSize;
    if (px > kMaxFontSize) px = kMaxFontSize;
    m_fontSize = px;
    if (m_hwnd) {
        RecalcSize();
        Redraw();
    }
}

void LyricWindow::SetSecondFontSize(int px) {
    if (px < kMinFontSize) px = kMinFontSize;
    if (px > kMaxFontSize) px = kMaxFontSize;
    m_secondFontSize = px;
    if (m_hwnd) {
        RecalcSize();
        Redraw();
    }
}

void LyricWindow::SetLocked(bool locked) {
    m_locked = locked;
    if (m_hwnd && m_visible) Redraw();
}

void LyricWindow::SetShowTranslation(bool show) {
    m_showTranslation = show;
    if (m_hwnd && m_visible) Redraw();
}

void LyricWindow::RecalcSize() {
    if (!m_hwnd) return;
    // 高度随字号变化; 宽度保持不变(尊重用户拖拽的宽度)
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right - rc.left;
    if (w < kMinWidth) w = kMinWidth;
    int maxW = GetSystemMetrics(SM_CXSCREEN);
    if (w > maxW) w = maxW;
    int h = HeightForFontSize(m_fontSize, m_secondFontSize);
    SetWindowPos(m_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void LyricWindow::EnsureDib(int w, int h) {
    if (m_dib && m_dibW == w && m_dibH == h) return;
    FreeDib();

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_dib = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &m_dibBits, NULL, 0);
    m_dibDC = CreateCompatibleDC(NULL);
    m_dibOldBmp = (HBITMAP)SelectObject(m_dibDC, m_dib);
    m_dibW = w;
    m_dibH = h;
}

void LyricWindow::FreeDib() {
    if (m_dibDC) {
        SelectObject(m_dibDC, m_dibOldBmp);
        DeleteDC(m_dibDC);
        m_dibDC = NULL;
    }
    if (m_dib) { DeleteObject(m_dib); m_dib = NULL; }
    m_dibBits = NULL;
    m_dibOldBmp = NULL;
    m_dibW = m_dibH = 0;
}

void LyricWindow::Redraw() {
    if (!m_hwnd || !m_visible) return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    EnsureDib(w, h);

    // 清空为全透明, 避免上一帧的文字残留在当前帧上形成重叠
    ZeroMemory(m_dibBits, (SIZE_T)w * h * 4);

    {
        Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)m_dibBits);
        Graphics g(&bmp);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        // 背景: 悬停时显示整窗半透明卡片; 锁定或淡出时背景透明(点击穿透), 仅文字区域可命中
        bool showCard = !m_locked && m_bgVisible;
        if (showCard) {
            SolidBrush bg(Color(120, 0, 0, 0));
            g.FillRectangle(&bg, 0, 0, w, h);
        }

        Color mainCol(255, GetRValue(m_color), GetGValue(m_color), GetBValue(m_color));
        Color nextCol(255, GetRValue(m_nextColor), GetGValue(m_nextColor), GetBValue(m_nextColor));

        FontFamily family(kFontFamily);
        Font fontCur(&family, (REAL)m_fontSize, FontStyleBold, UnitPixel);
        Font fontSecond(&family, (REAL)m_secondFontSize, FontStyleRegular, UnitPixel);
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        if (m_lines.empty()) {
            if (!showCard)
                FillHitRect(g, m_emptyText, fontCur, 0.0f, 0.0f, (REAL)w, (REAL)h);
            DrawTextShadowed(g, m_emptyText, fontCur, nextCol, &fmt, 0.0f, 0.0f, (REAL)w, (REAL)h);
        } else {
            int n = (int)m_lines.size();
            int cur = (m_currentIndex < 0) ? 0 : m_currentIndex;
            if (cur >= n) cur = n - 1;

            // 当前"歌词单元" = 同一时间戳的连续行 (第1行=原文, 第2行=译文)
            int unitStart = cur;
            while (unitStart > 0 && m_lines[unitStart - 1].timeMs == m_lines[cur].timeMs) unitStart--;
            int unitEnd = cur;
            while (unitEnd + 1 < n && m_lines[unitEnd + 1].timeMs == m_lines[cur].timeMs) unitEnd++;

            std::wstring original = m_lines[unitStart].text;
            std::wstring translation = (m_showTranslation && unitEnd > unitStart) ? m_lines[unitStart + 1].text : L"";
            std::wstring next = (unitEnd + 1 < n) ? m_lines[unitEnd + 1].text : L"";

            // 空行(间奏/结尾)占位, 避免窗口完全透明消失
            if (original.empty()) original = L"···";

            // 第二行: 显示译文时为译文, 否则为下一句(仅原文)
            std::wstring secondLine = !translation.empty() ? translation : next;

            int origH = (int)(m_fontSize * 1.5);
            int secondH = (int)(m_secondFontSize * 1.25);

            int contentW = w - 2 * kPadX;

            // 未显示卡片(锁定或淡出态): 填充文字命中矩形, 把可点击/悬停范围收紧到文字区域
            if (!showCard) {
                FillHitRect(g, original, fontCur, 0.0f, 0.0f, (REAL)w, (REAL)origH);
                if (!secondLine.empty())
                    FillHitRect(g, secondLine, fontSecond, 0.0f, (REAL)origH, (REAL)w, (REAL)secondH);
            }

            // 原文(当前行) 超宽则横向滚动(走马灯)
            RectF bound;
            g.MeasureString(original.c_str(), -1, &fontCur, PointF(0.0f, 0.0f), &bound);
            bool overflow = (bound.Width > (REAL)contentW);

            if (overflow) {
                if (!m_scrolling) {
                    m_scrolling = true;
                    m_scrollOffset = 0;
                    SetTimer(m_hwnd, kScrollTimerId, kScrollTickMs, NULL);
                }
                m_scrollMax = (int)(bound.Width - contentW) + kScrollPausePx;
                if (m_scrollOffset > m_scrollMax) m_scrollOffset = 0;

                StringFormat fmtLeft;
                fmtLeft.SetAlignment(StringAlignmentNear);
                fmtLeft.SetLineAlignment(StringAlignmentCenter);
                fmtLeft.SetFormatFlags(StringFormatFlagsNoWrap);

                g.SetClip(RectF(0.0f, 0.0f, (REAL)w, (REAL)origH));
                DrawTextShadowed(g, original, fontCur, mainCol, &fmtLeft,
                                 (REAL)(kPadX - m_scrollOffset), 2.0f, (REAL)bound.Width, (REAL)origH);
                g.ResetClip();
            } else {
                if (m_scrolling) {
                    m_scrolling = false;
                    m_scrollOffset = 0;
                    KillTimer(m_hwnd, kScrollTimerId);
                }
                DrawTextShadowed(g, original, fontCur, mainCol, &fmt, 0.0f, 2.0f, (REAL)w, (REAL)origH);
            }

            // 第二行 (译文或下一句)
            if (!secondLine.empty())
                DrawTextShadowed(g, secondLine, fontSecond, nextCol, &fmt, 0.0f, (REAL)origH, (REAL)w, (REAL)secondH);
        }
    }

    POINT ptSrc = { 0, 0 };
    RECT wr;
    GetWindowRect(m_hwnd, &wr);
    POINT ptDst = { wr.left, wr.top };
    SIZE size = { w, h };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    HDC screenDC = GetDC(NULL);
    UpdateLayeredWindow(m_hwnd, screenDC, &ptDst, &size, m_dibDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(NULL, screenDC);
}

LRESULT CALLBACK LyricWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LyricWindow* self = (LyricWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        self = (LyricWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT LyricWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            Redraw();
            ValidateRect(m_hwnd, NULL);
            return 0;

        case WM_SIZE:
            // 拖拽缩放过程中让内容跟随窗口尺寸 (字号不变, 仅在松手时联动)
            Redraw();
            return 0;

        case WM_GETMINMAXINFO: {
            // 约束拖拽缩放范围: 等比缩放, 拖到某一行字号先触及 12/72 即停止
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = kMinWidth;
            mmi->ptMaxTrackSize.x = GetSystemMetrics(SM_CXSCREEN);

            double ratio = (double)m_secondFontSize / (double)m_fontSize;
            int minFirst = kMinFontSize, minSecond = (int)(kMinFontSize * ratio + 0.5);
            if (minSecond < kMinFontSize) { minSecond = kMinFontSize; minFirst = (int)(kMinFontSize / ratio + 0.5); }
            int maxFirst = kMaxFontSize, maxSecond = (int)(kMaxFontSize * ratio + 0.5);
            if (maxSecond > kMaxFontSize) { maxSecond = kMaxFontSize; maxFirst = (int)(kMaxFontSize / ratio + 0.5); }

            mmi->ptMinTrackSize.y = HeightForFontSize(minFirst, minSecond);
            mmi->ptMaxTrackSize.y = HeightForFontSize(maxFirst, maxSecond);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!m_bgVisible) {
                m_bgVisible = true;
                Redraw();
            }
            KillTimer(m_hwnd, kFadeTimerId);
            if (!m_mouseTracking) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hwnd, 0 };
                TrackMouseEvent(&tme);
                m_mouseTracking = true;
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            m_mouseTracking = false;
            SetTimer(m_hwnd, kFadeTimerId, kFadeDelayMs, NULL);
            return 0;

        case WM_TIMER:
            if (wp == kFadeTimerId) {
                KillTimer(m_hwnd, kFadeTimerId);
                m_bgVisible = false;
                Redraw();
                return 0;
            }
            if (wp == kScrollTimerId) {
                m_scrollOffset += kScrollStep;
                if (m_scrollOffset > m_scrollMax) m_scrollOffset = 0;
                Redraw();
                return 0;
            }
            return 0;

        case WM_NCHITTEST: {
            if (m_locked) return HTCLIENT;   // 锁定: 禁止缩放
            // 无边框窗口: 边缘 8px 热区用于缩放, 中间交给默认(客户端)以便拖动与右键
            POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
            RECT rc;
            GetWindowRect(m_hwnd, &rc);
            bool left   = (pt.x >= rc.left && pt.x < rc.left + kBorder);
            bool right  = (pt.x > rc.right - kBorder && pt.x <= rc.right);
            bool top    = (pt.y >= rc.top && pt.y < rc.top + kBorder);
            bool bottom = (pt.y > rc.bottom - kBorder && pt.y <= rc.bottom);
            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left)   return HTLEFT;
            if (right)  return HTRIGHT;
            if (top)    return HTTOP;
            if (bottom) return HTBOTTOM;
            return DefWindowProcW(m_hwnd, msg, wp, lp);
        }

        case WM_EXITSIZEMOVE: {
            // 拖拽结束: 高度等比缩放首行/次行字号, 宽度独立钳制, 吸附并回写设置
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            int maxW = GetSystemMetrics(SM_CXSCREEN);
            if (w < kMinWidth) w = kMinWidth;
            if (w > maxW) w = maxW;

            double oldTotal = m_fontSize * 1.5 + m_secondFontSize * 1.25;
            double newTotal = h - kHeightPad;
            if (oldTotal > 0 && newTotal > 0) {
                double scale = newTotal / oldTotal;
                int newFirst = (int)(m_fontSize * scale + 0.5);
                int newSecond = (int)(m_secondFontSize * scale + 0.5);
                if (newFirst < kMinFontSize) newFirst = kMinFontSize;
                if (newFirst > kMaxFontSize) newFirst = kMaxFontSize;
                if (newSecond < kMinFontSize) newSecond = kMinFontSize;
                if (newSecond > kMaxFontSize) newSecond = kMaxFontSize;

                bool changed = (newFirst != m_fontSize || newSecond != m_secondFontSize);
                m_fontSize = newFirst;
                m_secondFontSize = newSecond;

                SetWindowPos(m_hwnd, NULL, 0, 0, w, HeightForFontSize(m_fontSize, m_secondFontSize),
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                Redraw();

                if (changed && m_listener)
                    m_listener->OnFontSizeChanged(m_fontSize, m_secondFontSize);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (m_locked) return 0;   // 锁定: 禁止拖拽
            ReleaseCapture();
            SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;

        case WM_RBUTTONUP: {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"使窗口居中");
            AppendMenuW(menu, MF_STRING | (m_locked ? MF_CHECKED : MF_UNCHECKED),
                        2, m_locked ? L"解除歌词锁定" : L"锁定歌词窗口");
            AppendMenuW(menu, MF_STRING | (m_showTranslation ? MF_CHECKED : MF_UNCHECKED),
                        3, m_showTranslation ? L"隐藏译文" : L"显示译文");
            AppendMenuW(menu, MF_STRING, 4, L"上一首");
            AppendMenuW(menu, MF_STRING, 5, L"下一首");
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(menu, MF_STRING, 6, L"隐藏桌面歌词");

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(m_hwnd);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, NULL);
            DestroyMenu(menu);

            switch (cmd) {
                case 1: {  // 使窗口居中 (水平居中, 保持垂直位置)
                    RECT wr;
                    GetWindowRect(m_hwnd, &wr);
                    int x = (GetSystemMetrics(SM_CXSCREEN) - (wr.right - wr.left)) / 2;
                    SetWindowPos(m_hwnd, NULL, x, wr.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    break;
                }
                case 2:
                    SetLocked(!m_locked);
                    if (m_listener) m_listener->OnLockedChanged(m_locked);
                    break;
                case 3:
                    SetShowTranslation(!m_showTranslation);
                    if (m_listener) m_listener->OnTranslationChanged(m_showTranslation);
                    break;
                case 4:
                    if (m_listener) m_listener->OnPrevNext(false);
                    break;
                case 5:
                    if (m_listener) m_listener->OnPrevNext(true);
                    break;
                case 6:
                    if (m_listener) m_listener->OnHidden();
                    break;
            }
            return 0;
        }
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}
