#include "qtbug_workaround.h"

#ifdef Q_OS_WIN
#   include <Windows.h>

enum {
    WM_QT_SENDPOSTEDEVENTS = WM_USER + 1 /* Copied from qeventdispatcher_win.cpp. */
};

class QnQtbugWorkaroundPrivate {
public:
    QnQtbugWorkaroundPrivate(): inSizeMove(false), ignoreNextPostedEventsMessage(false) {}

    bool inSizeMove;
    bool ignoreNextPostedEventsMessage;
};

QnQtbugWorkaround::QnQtbugWorkaround(QObject *parent): 
    QObject(parent),
    d_ptr(new QnQtbugWorkaroundPrivate())
{
    qApp->installNativeEventFilter(this);
}

QnQtbugWorkaround::~QnQtbugWorkaround() {
    return;
}

bool QnQtbugWorkaround::nativeEventFilter(const QByteArray &, void *message, long *result) {
    Q_D(QnQtbugWorkaround);

    MSG *msg = static_cast<MSG *>(message);
    switch(msg->message) {
    /* QTBUG-806: Alt-Space is not passed to the widget. */
    case WM_SYSKEYDOWN:
        if(msg->wParam == ' ') {
            bool altPressed = (GetKeyState(VK_LMENU) & 0x80) || (GetKeyState(VK_RMENU) & 0x80);
            if(altPressed) {
                QWidget *widget = QWidget::find(reinterpret_cast<WId>(msg->hwnd));
                if(widget)
                    QCoreApplication::postEvent(widget->window(), new QEvent(QnEvent::WinSystemMenu));
                /* It is important not to return true here as it may mess up the QKeyMapper. */
            }
        }
        return false;

    /* QTBUG-32835: Qt does not react to system help query. */
    case WM_SYSCOMMAND: 
        if((msg->wParam & 0xfff0) == SC_CONTEXTHELP) {
            *result = 0; /* An application should return zero if it processes this message. */
            QWhatsThis::enterWhatsThisMode();
            return true;
        }
        return false;

    /* QTBUG-28513: event loop starvation if event dispatching is done by the system. */
    case WM_ENTERSIZEMOVE:
        d->inSizeMove = true;
        return false;
    case WM_EXITSIZEMOVE:
    case WM_CAPTURECHANGED:
        d->inSizeMove = false;
        return false;
    case WM_QT_SENDPOSTEDEVENTS:
        if(d->inSizeMove) {
            MSG msg;
            bool haveMessage = PeekMessageW(&msg, 0, 0, 0, PM_NOREMOVE | PM_NOYIELD);
            if(haveMessage) {
                return true;
            } else {
                d->ignoreNextPostedEventsMessage = !d->ignoreNextPostedEventsMessage;
                return !d->ignoreNextPostedEventsMessage;
            }
        }
        return false;

    default:
        return false;
    }
}

#else

class QnQtbugWorkaroundPrivate {};

QnQtbugWorkaround::QnQtbugWorkaround(QObject *parent = NULL): 
    QObject(parent) 
{}

QnQtbugWorkaround::~QnQtbugWorkaround() {
    return;
}

bool QnQtbugWorkaround::nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
    return false;
}

#endif

