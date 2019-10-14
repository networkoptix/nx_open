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
    QnPlatformAbstraction(nx::vms::server::PlatformMonitor* monitor = nullptr, QObject *parent = nullptr);
    nx::vms::server::PlatformMonitor *monitor() const { return m_monitor; }

private:
    nx::vms::server::PlatformMonitor* m_monitor = nullptr;
};
