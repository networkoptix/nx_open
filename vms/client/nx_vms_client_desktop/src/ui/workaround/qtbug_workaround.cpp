// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qtbug_workaround.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

#include <utils/common/delayed.h>

#include <nx/build_info.h>

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

bool QnQtbugWorkaround::nativeEventFilter(const QByteArray&, void* message, qintptr* result) {
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
                /* It is important not to return true here as it may brake the QKeyMapper. */
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
    if (!nx::build_info::isMacOsX())
        return;

    // Workaround for QTBUG-34767.
    const auto tryRaiseModalWindow =
        []()
        {
            static constexpr int kRaiseDelay = 100;
            const auto updateWindow =
                []()
                {
                    const auto modalWindow = qApp->modalWindow();
                    if (modalWindow && modalWindow != qApp->focusWindow()
                        && qApp->applicationState() == Qt::ApplicationActive)
                    {
                        modalWindow->raise();
                        modalWindow->requestActivate();
                    }
                };

            executeDelayedParented(updateWindow, kRaiseDelay, qApp);
        };

    QObject::connect(qApp, &QGuiApplication::focusWindowChanged, qApp, tryRaiseModalWindow);
    QObject::connect(qApp, &QGuiApplication::applicationStateChanged, qApp, tryRaiseModalWindow);
}

QnQtbugWorkaround::~QnQtbugWorkaround() {
    return;
}

bool QnQtbugWorkaround::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
}

#endif
