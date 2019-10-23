#include "platform_abstraction.h"

#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>

#if defined(Q_OS_WIN)
#   include "monitoring/monitor_win.h"
#   define QnMonitorImpl QnWindowsMonitor
#elif defined(Q_OS_LINUX)
#   include "monitoring/monitor_linux.h"
#   define QnMonitorImpl QnLinuxMonitor
#elif defined(Q_OS_MACX)
#   include "monitoring/monitor_mac.h"
#   define QnMonitorImpl QnMacMonitor
#else
#   include "monitoring/sigar_monitor.h"
#   define QnMonitorImpl QnSigarMonitor
#endif

QnPlatformAbstraction::QnPlatformAbstraction(nx::vms::server::PlatformMonitor* monitor, QObject *parent):
    base_type(parent)
{
    if (!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    if (!monitor)
        monitor = new nx::vms::server::GlobalMonitor(new QnMonitorImpl(this), this);
    else
        monitor->setParent(this);

    m_monitor = monitor;
}
