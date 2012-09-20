#ifndef QN_LINUX_MONITOR_H
#define QN_LINUX_MONITOR_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_LINUX
#include "sigar_monitor.h"

class QnLinuxMonitorPrivate;

class QnLinuxMonitor: public QnSigarMonitor {
    Q_OBJECT;

    typedef QnSigarMonitor base_type;

public:
    QnLinuxMonitor(QObject *parent = NULL);
    virtual ~QnLinuxMonitor();

    virtual QList<HddLoad> totalHddLoad() override;

private:
    Q_DECLARE_PRIVATE(QnLinuxMonitor);
    QScopedPointer<QnLinuxMonitorPrivate> d_ptr;
};
#endif

#endif // QN_LINUX_MONITOR_H
