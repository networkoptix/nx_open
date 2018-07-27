#pragma once

#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/utils/software_version.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/update/manager/detail/updates2_status_data_ex.h>
#include <nx/update/installer/detail/abstract_updates2_installer.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/common/p2p/downloader/downloader.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

class NX_UPDATE_API Updates2ManagerBase: public QObject
{
    Q_OBJECT

public:
    Updates2ManagerBase();
    api::Updates2StatusData status();
    api::Updates2StatusData download(const nx::utils::SoftwareVersion& targetVersion);
    api::Updates2StatusData install();
    api::Updates2StatusData cancel();
    api::Updates2StatusData check();
    void atServerStart();
    /**
     * After this method has been called Updates2Manager is not operational and should be
     * destroyed
     */
    void stopAsyncTasks();

protected:
    detail::Updates2StatusDataEx m_currentStatus;
    QnMutex m_mutex;
    update::info::AbstractUpdateRegistryPtr m_updateRegistry;

    void checkForRemoteUpdate(utils::TimerId timerId);
    void checkForGlobalDictionaryUpdate();
    void refreshStatusAfterCheck();
    void setStatus(
        api::Updates2StatusData::StatusCode code,
        const QString& message,
        const QList<api::TargetVersionWithEula> targets = QList<api::TargetVersionWithEula>(),
        const QString& releaseNotesUrl = QString(),
        double progress = 0.0f);
    void addFileToManualDataIfNeeded(const QString& fileName);
    void updateGlobalRegistryIfNeededUnsafe();

    void startPreparing(const QString& updateFilePath);

    void onDownloadFinished(const QString& fileName);
    void onDownloadFailed(const QString& fileName);
    void onFileDeleted(const QString& fileName);
    void onFileInformationChanged(
        const vms::common::p2p::downloader::FileInformation& fileInformation);
    void onFileInformationStatusChanged(
        const vms::common::p2p::downloader::FileInformation& fileInformation);
    void onChunkDownloadFailed(const QString& fileName);

    // 'Real world' communication functions
    // These below should be overridden in CommonUpdates2Manager.
    virtual void loadStatusFromFile() = 0;
    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() = 0;
    virtual QnUuid moduleGuid() const = 0;
    virtual void updateGlobalRegistry(const QByteArray& serializedRegistry) = 0;
    virtual void writeStatusToFile(const detail::Updates2StatusDataEx& statusData) = 0;
    virtual bool isClient() const = 0;
    virtual QnUuid peerId() const = 0;
    // This is for testing purposes only.
    virtual void remoteUpdateCompleted() = 0;

    // These below should be overriden in Server{Client}Updates2Manager.
    virtual qint64 refreshTimeout() const = 0;
    virtual void connectToSignals() = 0;
    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() = 0;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual installer::detail::AbstractUpdates2Installer* installer() = 0;
    virtual QString filePath() const = 0;
};

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
