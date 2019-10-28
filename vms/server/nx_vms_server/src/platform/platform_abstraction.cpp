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

void QnPlatformAbstraction::setCustomMonitor(std::unique_ptr<nx::vms::server::PlatformMonitor> monitor)
{
    m_monitor = std::move(monitor);
}

QnPlatformAbstraction::QnPlatformAbstraction(
    nx::vms::server::RootFileSystem* rootFs,
    nx::utils::TimerManager* timerManager)
    :
    base_type()
{
    if (!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    m_monitor.reset(new nx::vms::server::GlobalMonitor(
        std::make_unique<QnMonitorImpl>(), timerManager));
    m_monitor->setRootFileSystem(rootFs);
}
