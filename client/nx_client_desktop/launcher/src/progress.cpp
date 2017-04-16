#include "progress.h"
#include <Commctrl.h>

#pragma comment(lib, "comctl32")

namespace {

static const int kWidth = 400;
static const int kHeight = 80;

static const int kMargin = 8;

static constexpr WORD kResolution = 10000;

} // namespace

QnLauncherProgress::QnLauncherProgress(const std::wstring& caption):
    m_caption(caption)
{
    WNDCLASSEX cls;
    memset(&cls, 0, sizeof(cls));
    cls.cbSize = sizeof(cls);
    cls.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    cls.hCursor = LoadCursor(0, IDC_WAIT);
    cls.lpfnWndProc = DefWindowProc;
    cls.lpszClassName = L"LauncherProgressWindow";
    const ATOM wndClass = RegisterClassEx(&cls);

    m_mainWnd = CreateWindowEx(WS_EX_DLGMODALFRAME, (LPCTSTR)wndClass, NULL,
        WS_OVERLAPPED | WS_BORDER | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - kWidth) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - kHeight) / 2,
        kWidth, kHeight,
        0, 0, 0, NULL);

    RECT rc;
    GetClientRect(m_mainWnd, &rc);

    m_progressWnd = CreateWindowEx(0, PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        kMargin, kMargin,
        (rc.right - rc.left) - kMargin * 2, (rc.bottom - rc.top) - kMargin * 2,
        m_mainWnd, 0, 0, NULL);

    SendMessage(m_progressWnd, PBM_SETRANGE, 0, MAKELPARAM(0, kResolution));

    updateCaption(0);
}

QnLauncherProgress::~QnLauncherProgress()
{
    DestroyWindow(m_mainWnd);
}

void QnLauncherProgress::setRange(long long minPos, long long maxPos)
{
    m_minPos = minPos;
    m_scale = double(kResolution) / max(maxPos - minPos, 1);
    processMessages();
}

void QnLauncherProgress::setPos(long long pos)
{
    const int scaledPos = int((pos - m_minPos) * m_scale);
    SendMessage(m_progressWnd, PBM_SETPOS, scaledPos, 0);
    updateCaption(scaledPos);
    processMessages();
}

void QnLauncherProgress::updateCaption(int value)
{
    static const double kValueToPercentage = 100.0 / kResolution;
    const int percentage = min(max(0, (int)(kValueToPercentage * value)), 100);

    enum { BUFFER_SIZE = 5 };
    wchar_t buffer[BUFFER_SIZE + 1];
    swprintf(buffer, BUFFER_SIZE, L" %d%%", percentage);

    SetWindowText(m_mainWnd, (m_caption + buffer).c_str());
}

void QnLauncherProgress::processMessages()
{
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
