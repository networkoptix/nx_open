#include "platform_monitor.h"

#include "sigar_monitor.h"
#include "windows_monitor.h"
#include "linux_monitor.h"

QnPlatformMonitor *QnPlatformMonitor::newInstance(QObject *parent) {
#if defined(Q_OS_WIN)
    return new QnWindowsMonitor(parent);
#elif defined(Q_OS_LINUX)
    return new QnLinuxMonitor(parent);
#else
    return new QnSigarMonitor(parent);
#endif
}
