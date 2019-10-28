#pragma once

#include <QtCore/QObject>

#include <platform/core_platform_abstraction.h>
#include "monitoring/global_monitor.h"

namespace nx::utils { class TimerManager; }
namespace nx::vms::server { class RootFileSystem; }

class QnPlatformAbstraction: public QnCorePlatformAbstraction
{
    Q_OBJECT
    typedef QnCorePlatformAbstraction base_type;

public:
    QnPlatformAbstraction(
        nx::vms::server::RootFileSystem* rootFs,
        nx::utils::TimerManager* timerManager);

    nx::vms::server::PlatformMonitor *monitor() const { return m_monitor.get(); }
    void setCustomMonitor(std::unique_ptr<nx::vms::server::PlatformMonitor> monitor);
private:
    std::unique_ptr<nx::vms::server::PlatformMonitor> m_monitor;
};
