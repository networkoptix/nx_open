#include "size_move_workaround_windows_specific.h"

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
