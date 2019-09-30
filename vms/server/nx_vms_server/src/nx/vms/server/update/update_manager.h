#pragma once

#include <atomic>
#include <optional>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/update_information.h>
#include <nx/update/update_check.h>
#include <nx/update/persistent_update_storage.h>
#include <nx/vms/server/server_module_aware.h>
#include <api/model/api_model_fwd.h>
#include <api/model/storage_status_reply.h>

#include "update_installer.h"

class QTimer;

struct QnAuthSession;

namespace nx::vms::discovery { struct ModuleEndpoint; }

namespace nx::vms::server {

class UpdateManager: public QObject, public ServerModuleAware
{
    Q_OBJECT

public:
    enum class InformationCategory
    {
        target,
        installed
    };

    UpdateManager(QnMediaServerModule* serverModule);
    virtual ~UpdateManager() override;

    void connectToSignals();
    update::Status status();
    void cancel();
    void retry(bool forceRedownload = false);
    void startUpdate(const QByteArray& content);
    bool startUpdateInstallation(const QList<QnUuid>& participants);
    void install(const QnAuthSession& authInfo);

    std::optional<update::Information> updateInformation(InformationCategory category) const;

    void setUpdatePersistentStorageServers(
        const QList<QnUuid>& serverList, const QString& version, bool manuallySet);
    update::PersistentUpdateStorage updatePersistentStorageServers(const QString& version) const;

    void finish();

private:
    void onGlobalUpdateSettingChanged();
    void onDownloaderFailed(const QString& fileName);
    void onDownloaderFinished(const QString& fileName);
    void onDownloaderFileStatusChanged(
        const common::p2p::downloader::FileInformation& fileInformation);
    void processFoundEndpoint(const nx::vms::discovery::ModuleEndpoint& endpoint);
    update::FindPackageResult findPackage(
        nx::update::Package* outPackage,
        QString* outMessage = nullptr) const;
    void detectStartedInstallation();
    void setInstallationDetected(bool detected = true);
    void extract();
    void clearDownloader(bool force);
    bool canDownloadFile(const nx::update::Package& package, update::Status* outUpdateStatus);
    bool statusAppropriateForDownload(nx::update::Package* outPackage, update::Status* outStatus);
    bool installerState(update::Status* outUpdateStatus, const QnUuid& peerId);
    update::Status start();
    void managePersistentDownloads();
    vms::common::p2p::downloader::ResultCode addDownload(
        const nx::update::Package& package, const QString& downloadsDirPath = {});

    common::p2p::downloader::Downloader* downloader();
    int64_t freeSpace(const QString& path) const;
    QnStorageSpaceDataList availableStorages() const;

    void setTargetUpdateInformation(const update::Information& information);
    void checkUpdateInfo(const QnMediaServerResourcePtr& server, InformationCategory infoCategory);

private:
    enum class DownloaderFailDetail
    {
        noError,
        internalError,
        noFreeSpace,
        downloadFailed
    };
    std::atomic<DownloaderFailDetail> m_downloaderFailDetail = DownloaderFailDetail::noError;
    UpdateInstaller m_installer;
    update::Information m_targetUpdateInfo;
    QList<::rest::Handle> m_pendingUpdateInformationRequests;
    QTimer* m_autoRetryTimer = nullptr;
    bool m_installationDetected = false;
};

} // namespace nx::vms::server
