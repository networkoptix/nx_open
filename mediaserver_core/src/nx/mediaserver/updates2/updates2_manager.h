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
    api::Updates2StatusData download();
    void atServerStart();
    /** After this method is called manager is not operational and should be destroyed */
    void stopAsyncTasks();

private:
    detail::Updates2StatusDataEx m_currentStatus;
    QnMutex m_mutex;
    update::info::AbstractUpdateRegistryPtr m_updateRegistry;
    utils::TimerManager m_timerManager;

    void checkForRemoteUpdate(utils::TimerId timerId);
    void checkForGlobalDictionaryUpdate();
    void loadStatusFromFile();
    void swapRegistries(update::info::AbstractUpdateRegistryPtr otherRegistry);
    void refreshStatusAfterCheck();
    void onDownloadFinished(const QString& fileName);
    void setStatusUnsafe(api::Updates2StatusData::StatusCode code, const QString& message);
};

} // namespace updates2
} // namespace mediaserver
} // namespace nx
