#include "platform_monitor.h"

#ifdef Q_OS_WIN
#include "windows_monitor.h"
#elif defined(Q_OS_LINUX)
#include "linux_monitor.h"
#else
#include "sigar_monitor.h"
#endif

QnPlatformMonitor *QnPlatformMonitor::newInstance(QObject *parent) {
#if defined(Q_OS_WIN)
    return new QnWindowsMonitor(parent);
#elif defined(Q_OS_LINUX)
    return new QnLinuxMonitor(parent);
#else
    return new QnSigarMonitor(parent);
#endif
}
