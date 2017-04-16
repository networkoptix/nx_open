#ifndef QN_GLOBAL_MONITOR_H
#define QN_GLOBAL_MONITOR_H

#include "platform_monitor.h"

#include <QtCore/QScopedPointer>

class QnGlobalMonitorPrivate;

/**
 * A proxy monitor object suitable for usage as an application-wide monitor.
 *
 * Updates on regular time intervals and is thread-safe.
 */
class QnGlobalMonitor: public QnPlatformMonitor
{
    Q_OBJECT;

    typedef QnPlatformMonitor base_type;

public:

    static const int kDefaultUpdatePeridMs = 2500;

    /**
     * \param base                      Base platform monitor to get actual data from.
     *                                  Global monitor claims ownership of this object.
     * \param parent                    Parent of this object.
     * \param updatePeriodMs            statistics update period. It's disabled if 0.
     */
    QnGlobalMonitor(QnPlatformMonitor *base, QObject *parent);
    virtual ~QnGlobalMonitor();

    /**
     * \returns                         Update period of this global monitor object, in milliseconds.
     */
    qint64 updatePeriodMs() const;

    /**
     * \param updateTime                New update period for this global monitor object, in milliseconds.
     */
    void setUpdatePeriodMs(qint64 updatePeriod);

    /**
     * Server up time in milliseconds.
     */
    qint64 upTimeMs() const;

    virtual qreal totalCpuUsage() override;
    virtual qreal totalRamUsage() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QString partitionByPath(const QString &path) override;

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    Q_DECLARE_PRIVATE(QnGlobalMonitor)
    QScopedPointer<QnGlobalMonitorPrivate> d_ptr;
};


#endif // QN_GLOBAL_MONITOR_H
