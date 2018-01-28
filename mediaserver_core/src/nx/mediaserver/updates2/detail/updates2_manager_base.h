#pragma once

#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/mediaserver/updates2/detail/updates2_status_data_ex.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/common/p2p/downloader/downloader.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

class Updates2ManagerBase: public QObject
{
    Q_OBJECT

public:
    Updates2ManagerBase();
    api::Updates2StatusData status();
    api::Updates2StatusData download();
    void atServerStart();
    /** After this method is called manager is not operational and should be destroyed */
    void stopAsyncTasks();

protected:
    detail::Updates2StatusDataEx m_currentStatus;
    QnMutex m_mutex;
    update::info::AbstractUpdateRegistryPtr m_updateRegistry;
    utils::TimerManager m_timerManager;

    void checkForRemoteUpdate(utils::TimerId timerId);
    void checkForGlobalDictionaryUpdate();
    void swapRegistries(update::info::AbstractUpdateRegistryPtr otherRegistry);
    void refreshStatusAfterCheck();
    void setStatusUnsafe(api::Updates2StatusData::StatusCode code, const QString& message);
    void onDownloadFinished(const QString& fileName);
    void onDownloadFailed(const QString& fileName);
    void onFileAdded(const vms::common::p2p::downloader::FileInformation& fileInformation);
    void onFileDeleted(const QString& fileName);
    void onFileInformationChanged(
        const vms::common::p2p::downloader::FileInformation& fileInformation);
    void onFileInformationStatusChanged(
        const vms::common::p2p::downloader::FileInformation& fileInformation);
    void onChunkDownloadFailed(const QString& fileName);

    virtual qint64 refreshTimeout() = 0;
    virtual void loadStatusFromFile() = 0;
    virtual void connectToSignals() = 0;
    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() = 0;
    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() = 0;
    virtual QnUuid moduleGuid() = 0;
    virtual void updateGlobalRegistry(const QByteArray& serializedRegistry) = 0;
    virtual void writeStatusToFile(const detail::Updates2StatusDataEx& statusData) = 0;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual void remoteUpdateCompleted() = 0;
};

} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
