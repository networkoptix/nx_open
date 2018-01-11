#pragma once

#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/abstract_update_registry.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/mediaserver/updates2/detail/updates2_status_data_ex.h>

namespace nx {
namespace mediaserver {
namespace updates2 {

class Updates2Manager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    Updates2Manager(QnCommonModule* commonModule);
    api::Updates2StatusData status();
    void startTimers();

private:
    detail::Updates2StatusDataEx m_currentStatus;
    QnMutex m_mutex;
    update::info::AbstractUpdateRegistryPtr m_updateRegistry;
    utils::TimerManager m_timerManager;


    void checkForUpdate(utils::TimerId timerId);
    void writeStatusToFile();
    void loadStatusFromFile();
};

} // namespace updates2
} // namespace mediaserver
} // namespace nx
