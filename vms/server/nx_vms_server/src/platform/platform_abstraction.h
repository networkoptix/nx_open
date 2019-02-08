#pragma once

#include <QtCore/QObject>

#include <platform/core_platform_abstraction.h>
#include "monitoring/global_monitor.h"
#include <nx/utils/singleton.h>

class QnPlatformAbstraction: public QnCorePlatformAbstraction
{
    Q_OBJECT
    typedef QnCorePlatformAbstraction base_type;

public:
    QnPlatformAbstraction(QObject *parent = nullptr);
    virtual ~QnPlatformAbstraction();

    QnGlobalMonitor *monitor() const
    {
        return m_monitor;
    }

    void setUpdatePeriodMs(int value);
    void setServerModule(QnMediaServerModule* serverModule);

private:
    QnGlobalMonitor *m_monitor;
};
