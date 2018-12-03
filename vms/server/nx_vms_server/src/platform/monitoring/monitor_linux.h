#pragma once

#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <boost/optional.hpp>
#include "sigar_monitor.h"
#include <nx/utils/system_error.h>
#include <nx/utils/thread/mutex.h>

class QnLinuxMonitorPrivate;
namespace nx::vms::server { class RootFileSystem; }

class QnLinuxMonitor: public QnSigarMonitor
{
    Q_OBJECT
    typedef QnSigarMonitor base_type;

public:
    QnLinuxMonitor(QObject *parent = NULL);
    virtual ~QnLinuxMonitor();

    virtual qreal totalCpuUsage() override;
    virtual qreal totalRamUsage() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<QnPlatformMonitor::NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;

    virtual void setServerModule(QnMediaServerModule* serverModule) override;

private:
    Q_DECLARE_PRIVATE(QnLinuxMonitor);
    QScopedPointer<QnLinuxMonitorPrivate> d_ptr;
};
