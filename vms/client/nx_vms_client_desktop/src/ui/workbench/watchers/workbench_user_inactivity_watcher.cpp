#include "workbench_user_inactivity_watcher.h"

#include <QtCore/QEvent>

#include <QtWidgets/QWidget>

#include "client/client_settings.h"     // conflicts with <X11/extensions/scrnsaver.h>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACX)
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFNumber.h>
#elif defined(Q_OS_LINUX)
#include <QX11Info>
#include <X11/extensions/scrnsaver.h>
#endif

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

    io_iterator_t iter = 0;
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOHIDSystem"), &iter) == KERN_SUCCESS) {
        io_registry_entry_t entry = IOIteratorNext(iter);
        if (entry)  {
            CFMutableDictionaryRef dict = 0;
            if (IORegistryEntryCreateCFProperties(entry, &dict, kCFAllocatorDefault, 0) == KERN_SUCCESS) {
                CFNumberRef obj = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, CFSTR("HIDIdleTime")));
                if (obj) {
                    qint64 nanoseconds = 0;
                    if (CFNumberGetValue(obj, kCFNumberSInt64Type, &nanoseconds))
                        idle = static_cast<quint64>(nanoseconds / 1000000); // nanoseconds to milliseconds
                }
                CFRelease(dict);
            }
            IOObjectRelease(entry);
        }
        IOObjectRelease(iter);
    }

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
