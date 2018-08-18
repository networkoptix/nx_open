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

QnPlatformAbstraction::QnPlatformAbstraction(QObject *parent):
    base_type(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnPlatformAbstraction.");

    m_monitor = new QnGlobalMonitor(new QnMonitorImpl(this), this);
}

void QnPlatformAbstraction::setRootTool(nx::mediaserver::RootFileSystem* rootTool)
{
    m_monitor->setRootTool(rootTool);
}

void QnPlatformAbstraction::setUpdatePeriodMs(int value) { m_monitor->setUpdatePeriodMs(value); }

QnPlatformAbstraction::~QnPlatformAbstraction()
{
    return;
}
