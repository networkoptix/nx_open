#include "platform_monitor.h"

#include "windows_monitor.h"
#include "sigar_monitor.h"

QnPlatformMonitor *QnPlatformMonitor::newInstance(QObject *parent) {
#ifdef Q_OS_WIN
    return new QnWindowsMonitor(parent);
#else
    return new QnSigarMonitor(parent);
#endif
}
