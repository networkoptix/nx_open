#pragma once

#include <QtCore/QObject>

#include <platform/core_platform_abstraction.h>
#include "monitoring/global_monitor.h"

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
    void setRootTool(nx::mediaserver::RootFileSystem* rootTool);

private:
    QnGlobalMonitor *m_monitor;
};

#undef  qnPlatform
#define qnPlatform (static_cast<QnPlatformAbstraction *>(QnCorePlatformAbstraction::instance()))
