
#include "platform_monitor.h"

#include "sys_dependent_monitor.h"


QnPlatformMonitor *QnPlatformMonitor::newInstance(QObject *parent)
{
    return new QnSysDependentMonitor(parent);
}
