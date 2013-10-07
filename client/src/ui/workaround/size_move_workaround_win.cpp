#include "size_move_workaround_win.h"

#include <Windows.h>

enum {
    WM_QT_SENDPOSTEDEVENTS = WM_USER + 1 /* Copied from qeventdispatcher_win.cpp. */
};

QnSizeMoveWorkaround::QnSizeMoveWorkaround(QObject *parent): 
    QObject(parent),
    m_inSizeMove(false),
    m_ignoreNextPostedEventsMessage(false)
{
    qApp->installNativeEventFilter(this);
}

QnSizeMoveWorkaround::~QnSizeMoveWorkaround() {
    return;
}

bool QnSizeMoveWorkaround::nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
    MSG *msg = static_cast<MSG *>(message);
    switch(msg->message) {
    /* Workaround #1: Qt does not react to system help query. */
    case WM_SYSCOMMAND: 
        if(msg->wParam == SC_CONTEXTHELP) {
            *result = 0; /* An application should return zero if it processes this message. */
            QWhatsThis::enterWhatsThisMode();
            return true;
        }
        return false;

    /* Workaround #2: event loop starvation if event dispatching is done by the system. */
    case WM_ENTERSIZEMOVE:
        m_inSizeMove = true;
        return false;
    case WM_EXITSIZEMOVE:
    case WM_CAPTURECHANGED:
        m_inSizeMove = false;
        return false;
    case WM_QT_SENDPOSTEDEVENTS:
        if(m_inSizeMove) {
            MSG msg;
            bool haveMessage = PeekMessageW(&msg, 0, 0, 0, PM_NOREMOVE | PM_NOYIELD);
            if(haveMessage) {
                return true;
            } else {
                m_ignoreNextPostedEventsMessage = !m_ignoreNextPostedEventsMessage;
                return !m_ignoreNextPostedEventsMessage;
            }
        }
        return false;
    default:
        return false;
    }
}
