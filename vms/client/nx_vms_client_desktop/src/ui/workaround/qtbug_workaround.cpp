// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qtbug_workaround.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

#include <utils/common/delayed.h>

#include <nx/build_info.h>

#ifdef Q_OS_WIN
#include <Windows.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QWindow>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QWidget>
#include <QtWidgets/private/qwidgetwindow_p.h>

#include <utils/common/event_processors.h>

enum {
    WM_QT_SENDPOSTEDEVENTS = WM_USER + 1 /* Copied from qeventdispatcher_win.cpp. */
};

namespace {

QColor windowColor(QWindow* window)
{
    if (const auto quickWindow = qobject_cast<QQuickWindow*>(window))
        return quickWindow->color();

    const auto widgetWindow = dynamic_cast<QWidgetWindow*>(window);
    if (widgetWindow && widgetWindow->widget())
        return widgetWindow->widget()->palette().window().color();

    return qApp->palette().window().color();
}

} // namespace

class QnQtbugWorkaroundPrivate
{
public:
    QnQtbugWorkaroundPrivate() {}

    bool inSizeMove = false;
    bool ignoreNextPostedEventsMessage = false;

    QHash<HWND, QPointer<QWindow>> windows;
};

QnQtbugWorkaround::QnQtbugWorkaround(QObject* parent):
    QObject(parent),
    d_ptr(new QnQtbugWorkaroundPrivate())
{
    qApp->installNativeEventFilter(this);

    /*
     * There is a bug on Windows: white background flickering underneath content of native windows.
     * To fix that we track windows and their native handles and forcefully paint proper background
     * in response to WM_ERASEBKGND (see `nativeEventFilter` below).
     */
    installEventHandler(qApp, QEvent::PlatformSurface, this,
        [this](QObject* watched, QEvent* event)
        {
            if (const auto window = qobject_cast<QWindow*>(watched))
            {
                Q_D(QnQtbugWorkaround);
                const auto hwnd = (HWND) window->winId();
                const auto e = static_cast<QPlatformSurfaceEvent*>(event);
                if (e->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated)
                    d->windows.insert(hwnd, window);
                else
                    d->windows.remove(hwnd);
            }
        });
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

    /*
     * Force background painting for native windows.
     */
    case WM_ERASEBKGND:
    {
        const auto window = d->windows.value(msg->hwnd);
        if (!window)
            return false;
        const auto color = windowColor(window).rgb();
        const auto brush = ::CreateSolidBrush(RGB(qRed(color), qGreen(color), qBlue(color)));
        RECT rc;
        ::GetClientRect(msg->hwnd, &rc);
        ::FillRect(reinterpret_cast<HDC>(msg->wParam), &rc, brush);
        ::DeleteObject(brush);
        return true;
    }

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
