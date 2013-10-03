
#ifdef __APPLE__

#include "sys_dependent_monitor.h"


class QnSysDependentMonitorPrivate
{
};


QnSysDependentMonitor::QnSysDependentMonitor(QObject *parent)
:
    base_type(parent),
    d_ptr(new QnSysDependentMonitorPrivate())
{
}

QnSysDependentMonitor::~QnSysDependentMonitor()
{
}

QList<QnPlatformMonitor::HddLoad> QnSysDependentMonitor::totalHddLoad()
{
    return QnSigarMonitor::totalHddLoad();
}

#endif  //__APPLE__
