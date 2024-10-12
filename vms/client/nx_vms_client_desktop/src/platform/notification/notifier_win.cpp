// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notifier_win.h"

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>

#include <utils/common/invocation_event.h>

#include <nx/utils/log/log.h>

#include <Windows.h>

/* The following definition is from <dwmapi.h>.
 * It's here so that everything would compile even if we don't have that include file. */
#define WM_DWMCOMPOSITIONCHANGED        0x031E


enum QnWindowsNotifierInvocation {
    UpdateTimeInvocation = 0x8954,
    CompositionChangeInvocation = 0x6591
};

LPCWSTR qn_windowsNotifierWindowClassName = L"QnWindowsNotifierWidget";
LRESULT CALLBACK qn_windowsNotifierWidgetProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// -------------------------------------------------------------------------- //
// QnWindowsNotifierWindow
// -------------------------------------------------------------------------- //
class QnWindowsNotifierWindow: public QObject {
public:
    QnWindowsNotifierWindow(QObject *parent = nullptr):
        QObject(parent),
        m_wndClass(0),
        m_hwnd(0)
    {
        WNDCLASSEXW wcex;
        wcex.cbSize         = sizeof(WNDCLASSEXW);
        wcex.style          = 0;
        wcex.lpfnWndProc    = qn_windowsNotifierWidgetProc;
        wcex.cbClsExtra     = 0;
        wcex.cbWndExtra     = 0;
        wcex.hInstance      = GetModuleHandle(0);
        wcex.hIcon          = 0;
        wcex.hCursor        = 0;
        wcex.hbrBackground  = 0;
        wcex.lpszMenuName   = 0;
        wcex.lpszClassName  = qn_windowsNotifierWindowClassName;
        wcex.hIconSm        = 0;

        m_wndClass = RegisterClassExW(&wcex);
        if(!m_wndClass) {
            NX_ASSERT(false, "Windows notifier window registration failed.");
            return;
        }

        m_hwnd = CreateWindowExW(
            0,
            qn_windowsNotifierWindowClassName,
            L"",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            100, 100,
            0,
            0,
            GetModuleHandle(0),
            0
        );
        if(!m_hwnd)
        {
            NX_ASSERT(false, "Windows notifier window creation failed.");
            return;
        }
    }

    virtual ~QnWindowsNotifierWindow() {
        if(m_hwnd)
            DestroyWindow(m_hwnd);

        if(m_wndClass)
            UnregisterClassW(qn_windowsNotifierWindowClassName, GetModuleHandle(nullptr));
    }

private:
    ATOM m_wndClass;
    HWND m_hwnd;
};

Q_GLOBAL_STATIC(QnWindowsNotifierWindow, qn_windowsNotifierWindow);

LRESULT CALLBACK qn_windowsNotifierWidgetProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMECHANGE:
        QCoreApplication::postEvent(qn_windowsNotifierWindow(), new QnInvocationEvent(UpdateTimeInvocation));
        return 0;
    case WM_DWMCOMPOSITIONCHANGED:
        QCoreApplication::postEvent(qn_windowsNotifierWindow(), new QnInvocationEvent(CompositionChangeInvocation));
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}


// -------------------------------------------------------------------------- //
// QnWindowsNotifier
// -------------------------------------------------------------------------- //
QnWindowsNotifier::QnWindowsNotifier(QObject *parent):
    base_type(parent),
    m_timeZoneOffset(0)
{
    if(qn_windowsNotifierWindow()) /* It will be nullptr if application is being shut down. */
        qn_windowsNotifierWindow()->installEventFilter(this);

    updateTime(false);
}

QnWindowsNotifier::~QnWindowsNotifier() {
    return;
}

void QnWindowsNotifier::updateTime(bool notify) {
    // TODO: #sivanov Actually check for zone change, not offset change.

    /* Note that we compare time zone offsets, not time zones, which is not 100% correct.
     * We will still receive update invocations even if timezone was changed without
     * changing the timezone offset (e.g. Moscow+4 -> Tbilisi+4). */
    QDateTime dt1 = QDateTime::currentDateTime();
    QDateTime dt2 = dt1.toUTC();
    dt1.setTimeZone(QTimeZone::UTC);
    qint64 timeZoneOffset = dt2.msecsTo(dt1);

    if(timeZoneOffset != m_timeZoneOffset) {
        m_timeZoneOffset = timeZoneOffset;
        if(notify)
            this->notify(TimeZoneValue);
    }

    if(notify)
        this->notify(TimeValue);
}

bool QnWindowsNotifier::eventFilter(QObject *watched, QEvent *event) {
    if(event->type() == QnEvent::Invocation) {
        QnInvocationEvent *e = static_cast<QnInvocationEvent *>(event);
        if(e->id() == UpdateTimeInvocation) {
            updateTime(true);
        } else if(e->id() == CompositionChangeInvocation) {
            notify(CompositionValue);
        }
        return false;
    } else {
        return base_type::eventFilter(watched, event);
    }
}

void QnWindowsNotifier::notify(int value) {
    base_type::notify(value);
    if(value == CompositionValue)
        emit compositionChanged();
}
