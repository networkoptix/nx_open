#include "windows_notifier.h"

#ifdef Q_OS_WIN

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtGui/QWidget>

#include <utils/common/warnings.h>
#include <utils/common/invocation_event.h>

#include <Windows.h>

enum QnWindowsNotifierInvocation {
    UpdateTimeInvocation = 0x8954
};

LPCWSTR qn_windowsNotifierWindowClassName = L"QnWindowsNotifierWidget";
LRESULT CALLBACK qn_windowsNotifierWidgetProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// -------------------------------------------------------------------------- //
// QnWindowsNotifierWindow
// -------------------------------------------------------------------------- //
class QnWindowsNotifierWindow: public QObject {
public:
    QnWindowsNotifierWindow(QObject *parent = NULL): 
        QObject(parent),
        m_wndClass(NULL),
        m_hwnd(NULL) 
    {
        WNDCLASSEXW wcex;
        wcex.cbSize         = sizeof(WNDCLASSEXW);
        wcex.style          = 0;
        wcex.lpfnWndProc    = qn_windowsNotifierWidgetProc;
        wcex.cbClsExtra     = 0;
        wcex.cbWndExtra     = 0;
        wcex.hInstance      = GetModuleHandle(NULL);
        wcex.hIcon          = NULL;
        wcex.hCursor        = NULL;
        wcex.hbrBackground  = NULL;
        wcex.lpszMenuName   = NULL;
        wcex.lpszClassName  = qn_windowsNotifierWindowClassName;
        wcex.hIconSm        = NULL;
        
        m_wndClass = RegisterClassExW(&wcex);
        if(!m_wndClass) {
            qnWarning("Windows notifier window registration failed.");
            return;
        }

        m_hwnd = CreateWindowExW(
            NULL,
            qn_windowsNotifierWindowClassName, 
            L"",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 
            CW_USEDEFAULT,
            100, 100,
            NULL,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );
        if(!m_hwnd) {
            qnWarning("Windows notifier window creation failed.");
            return;
        }
    }

    virtual ~QnWindowsNotifierWindow() {
        if(m_hwnd)
            DestroyWindow(m_hwnd);

        if(m_wndClass)
            UnregisterClassW(qn_windowsNotifierWindowClassName, GetModuleHandle(NULL));
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
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }
}


// -------------------------------------------------------------------------- //
// QnWindowsNotifier
// -------------------------------------------------------------------------- //
QnWindowsNotifier::QnWindowsNotifier(QObject *parent): 
    base_type(parent)
{
    if(qn_windowsNotifierWindow()) /* It will be NULL if application is being shut down. */
        qn_windowsNotifierWindow()->installEventFilter(this);

    updateTime(false);
}

QnWindowsNotifier::~QnWindowsNotifier() {
    return;
}

void QnWindowsNotifier::updateTime(bool notify) {
    /* Note that we compare time zone offsets, not time zones, which is not 100% correct. 
     * We will still receive update invocations even if timezone was changed without
     * changing the timezone offset (e.g. Moscow+4 -> Tbilisi+4). */
    QDateTime dt1 = QDateTime::currentDateTime();
    QDateTime dt2 = dt1.toUTC();
    dt1.setTimeSpec(Qt::UTC);
    qint64 timeZoneOffset = dt2.msecsTo(dt1);
    
    if(timeZoneOffset != m_timeZoneOffset) {
        m_timeZoneOffset = timeZoneOffset;
        if(notify)
            emit timeZoneChanged();
    }

    if(notify)
        emit timeChanged();
}

bool QnWindowsNotifier::eventFilter(QObject *watched, QEvent *event) {
    if(event->type() == QnInvocationEvent::Invocation && static_cast<QnInvocationEvent *>(event)->id() == UpdateTimeInvocation) {
        updateTime(true);
        return false;
    } else {
        return base_type::eventFilter(watched, event);
    }
}

#endif // Q_OS_WIN
