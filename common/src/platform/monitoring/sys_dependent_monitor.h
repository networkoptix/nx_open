/**********************************************************
* 17 may 2013
* a.kolesnikov
***********************************************************/

#ifndef SYSDEPENDENTMONITOR_H
#define SYSDEPENDENTMONITOR_H

#include "sigar_monitor.h"


class QnSysDependentMonitorPrivate;

/*!
    This class introduced to simplify multi-platform implementation: move from "multiple header - multiple cpp" to "single header - multiple cpp"
*/
class QnSysDependentMonitor
:
    public QnSigarMonitor
{
    typedef QnSigarMonitor base_type;

public:
    QnSysDependentMonitor(QObject *parent = NULL);
    virtual ~QnSysDependentMonitor();

    virtual QList<HddLoad> totalHddLoad() override;

private:
    Q_DECLARE_PRIVATE(QnSysDependentMonitor);
    QScopedPointer<QnSysDependentMonitorPrivate> d_ptr;
};

#endif  //SYSDEPENDENTMONITOR_H
