#ifndef QN_WINDOWS_MONITOR_H
#define QN_WINDOWS_MONITOR_H

#include "sigar_monitor.h"

class QnWindowsMonitorPrivate;

class QnWindowsMonitor: public QnSigarMonitor {
    Q_OBJECT
    typedef QnSigarMonitor base_type;

public:
    QnWindowsMonitor(QObject *parent = NULL);
    virtual ~QnWindowsMonitor();

    //!Implementation of \a QnPlatformMonitor::totalPartitionSpaceInfo
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    //!Implementation of \a QnPlatformMonitor::totalHddLoad
    virtual QList<HddLoad> totalHddLoad() override;
    //!Implementation of \a QnPlatformMonitor::totalNetworkLoad
    virtual QList<NetworkLoad> totalNetworkLoad() override;

private:
    Q_DECLARE_PRIVATE(QnWindowsMonitor);
    QScopedPointer<QnWindowsMonitorPrivate> d_ptr;
};

#endif // QN_WINDOWS_MONITOR_H
