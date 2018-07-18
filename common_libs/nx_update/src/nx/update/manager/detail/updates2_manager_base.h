#pragma once

#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/installer/detail/abstract_updates2_installer.h>
#include <nx/update/update_information.h>
#include <nx/utils/software_version.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

class NX_UPDATE_API Updates2ManagerBase : public QObject
{
    Q_OBJECT

  public:
    Updates2ManagerBase();
    api::Updates2StatusData status();
    api::Updates2StatusData start(const Information& information);
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
    QnMutex m_mutex;

    void checkForRemoteUpdate(utils::TimerId timerId);
    void checkForGlobalDictionaryUpdate();
    void refreshStatusAfterCheck();
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
    virtual QnUuid moduleGuid() const = 0;
    virtual void updateGlobalRegistry(const QByteArray& serializedRegistry) = 0;
    virtual bool isClient() const = 0;
    virtual QnUuid peerId() const = 0;
    // This is for testing purposes only.
    virtual void remoteUpdateCompleted() = 0;

    // These below should be overriden in Server{Client}Updates2Manager.
    virtual qint64 refreshTimeout() const = 0;
    virtual void connectToSignals() = 0;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual installer::detail::AbstractUpdates2Installer* installer() = 0;
    virtual QString filePath() const = 0;
};

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
