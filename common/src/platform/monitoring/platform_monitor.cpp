
#include "platform_monitor.h"

#include "sys_dependent_monitor.h"
//#include "monitor_win.h"
//#include "monitor_unix.h"
//#include "sigar_monitor.h"


QnPlatformMonitor *QnPlatformMonitor::newInstance(QObject *parent)
{
    return new QnSysDependentMonitor(parent);

//#if defined(Q_OS_WIN)
//    return new QnWindowsMonitor(parent);
//#elif defined(Q_OS_LINUX)
//    return new QnLinuxMonitor(parent);
//#else
//    return new QnSigarMonitor(parent);
//#endif
}
