#include "qtbug_workaround.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

#include <nx/utils/app_info.h>
#include <utils/common/delayed.h>

#ifdef Q_OS_WIN
#   include <Windows.h>

#include <QtCore/QCoreApplication>
#include <QtWidgets/QWidget>

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
    Q_UNUSED(result);
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

QnQtbugWorkaround::QnQtbugWorkaround(QObject *parent):
    QObject(parent)
{
    if (nx::utils::AppInfo::isMacOsX())
    {
        // Workaround of QTBUG-34767
        QObject::connect(qApp, &QGuiApplication::focusWindowChanged, qApp,
             []()
             {
                 const auto modalWindow = qApp->modalWindow();
                 if (modalWindow && !qApp->focusWindow())
                     modalWindow->requestActivate();
             });

        QObject::connect(qApp, &QGuiApplication::applicationStateChanged, qApp,
            []()
            {
                const auto raiseModelWindow =
                    []()
                    {
                        const auto modalWindow = qApp->modalWindow();
                        if (modalWindow && qApp->applicationState() == Qt::ApplicationActive)
                            modalWindow->raise();
                    };

                static constexpr int kRaiseDelay = 1000;
                executeDelayed(raiseModelWindow, kRaiseDelay);
            });
    }

}

QnQtbugWorkaround::~QnQtbugWorkaround() {
    return;
}

bool QnQtbugWorkaround::nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
}

#endif

