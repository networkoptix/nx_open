// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_user_inactivity_watcher.h"

#include <chrono>

#include <QtCore/QEvent>

#include <QtWidgets/QWidget>

#include "client/client_settings.h"     // conflicts with <X11/extensions/scrnsaver.h>

#if defined(Q_OS_WIN)
    #include <windows.h>
#elif defined(Q_OS_MACOS)
    #include <CoreGraphics/CGEventSource.h>
#elif defined(Q_OS_LINUX)
    #include <QtGui/private/qtx11extras_p.h>
    #include <X11/extensions/scrnsaver.h>
#endif

using namespace std::chrono;

namespace {
    const int timerIntervalMs = 1000; // check user inactivity every second
}

QnWorkbenchUserInactivityWatcher::QnWorkbenchUserInactivityWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_timerId(-1),
    m_userIsInactive(false),
    m_idleTimeout(0),
    m_mainWindow(0)
{
    connect(qnSettings->notifier(QnClientSettings::USER_IDLE_TIMEOUT_MSECS),    SIGNAL(valueChanged(int)),  this,   SLOT(updateTimeout()));
    updateTimeout();
}

QnWorkbenchUserInactivityWatcher::~QnWorkbenchUserInactivityWatcher() {
    if (m_timerId != -1)
        killTimer(m_timerId);

    if (m_mainWindow)
        m_mainWindow->removeEventFilter(this);
}

quint64 QnWorkbenchUserInactivityWatcher::idlePeriodMSecs() const {
    quint64 idle = 0;

#if defined(Q_OS_WIN)

    LASTINPUTINFO inputInfo;
    inputInfo.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&inputInfo))
        idle = GetTickCount() - inputInfo.dwTime;

#elif defined(Q_OS_MAC)

    const CFTimeInterval interval = CGEventSourceSecondsSinceLastEventType(
        kCGEventSourceStateCombinedSessionState,
        kCGAnyInputEventType);

    const duration<double> seconds(interval);

    idle = duration_cast<milliseconds>(seconds).count();

#elif defined (Q_OS_LINUX)

    Display *display = QX11Info::display();
    if (display) {
        XScreenSaverInfo *info = XScreenSaverAllocInfo();
        XScreenSaverQueryInfo(display, DefaultRootWindow(display), info);
        idle = info->idle;
        XFree(info);
    }

#endif

    quint64 msecsSinceMainWindowMinimized = 0;
    if (!m_mainWindowMinimizedTime.isNull())
        msecsSinceMainWindowMinimized = m_mainWindowMinimizedTime.msecsTo(QDateTime::currentDateTime());

    return qMax(idle, msecsSinceMainWindowMinimized);
}

bool QnWorkbenchUserInactivityWatcher::state() const {
    return m_userIsInactive;
}

void QnWorkbenchUserInactivityWatcher::setState(bool userIsInactive) {
    if (m_userIsInactive == userIsInactive)
        return;

    m_userIsInactive = userIsInactive;
    emit stateChanged(userIsInactive);
}

bool QnWorkbenchUserInactivityWatcher::isEnabled() const {
    return m_timerId != -1;
}

void QnWorkbenchUserInactivityWatcher::setEnabled(bool enable) {
    if (enable == isEnabled())
        return;

    if (enable) {
        m_timerId = startTimer(timerIntervalMs);
    } else {
        killTimer(m_timerId);
        m_timerId = -1;
    }
}

quint64 QnWorkbenchUserInactivityWatcher::idleTimeoutMSecs() const {
    return m_idleTimeout;
}

void QnWorkbenchUserInactivityWatcher::setIdleTimeoutMSecs(quint64 msecs) {
    m_idleTimeout = msecs;
    if (isEnabled())
        checkInactivity();
}

void QnWorkbenchUserInactivityWatcher::setMainWindow(QWidget *widget) {
    if (m_mainWindow)
        m_mainWindow->removeEventFilter(this);

    m_mainWindow = widget;
    if (!widget)
        return;

    m_mainWindowMinimizedTime = m_mainWindow->isMinimized()
                                ? QDateTime::currentDateTime()
                                : QDateTime();
    m_mainWindow->installEventFilter(this);
}

bool QnWorkbenchUserInactivityWatcher::eventFilter(QObject *object, QEvent *event) {
    if (!m_mainWindow || m_mainWindow != object)
        return false;

    if (event->type() != QEvent::WindowStateChange)
        return false;

    m_mainWindowMinimizedTime = m_mainWindow->isMinimized()
                                ? QDateTime::currentDateTime()
                                : QDateTime();

    return false;
}

void QnWorkbenchUserInactivityWatcher::checkInactivity() {
    setState(idlePeriodMSecs() >= m_idleTimeout);
}

void QnWorkbenchUserInactivityWatcher::timerEvent(QTimerEvent *event) {
    Q_UNUSED(event)
    checkInactivity();
}

void QnWorkbenchUserInactivityWatcher::updateTimeout() {
    m_idleTimeout = qnSettings->userIdleTimeoutMSecs();
    setEnabled(m_idleTimeout > 0);
}
