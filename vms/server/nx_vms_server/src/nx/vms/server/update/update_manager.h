#pragma once

#include <atomic>

#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/update_information.h>
#include <nx/update/update_check.h>
#include <nx/vms/server/server_module_aware.h>

#include "update_installer.h"

struct QnAuthSession;

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

    update::Information updateInformation(InformationCategory category) const noexcept(false);

    void finish();

private:
    void onGlobalUpdateSettingChanged();
    void onDownloaderFailed(const QString& fileName);
    void onDownloaderFinished(const QString& fileName);
    void onDownloaderFileStatusChanged(
        const common::p2p::downloader::FileInformation& fileInformation);
    update::FindPackageResult findPackage(
        nx::update::Package* outPackage,
        QString* outMessage = nullptr) const;
    void extract();
    void clearDownloader(bool force);
    bool canDownloadFile(const nx::update::Package& package, update::Status* outUpdateStatus);
    bool statusAppropriateForDownload(nx::update::Package* outPackage, update::Status* outStatus);
    bool installerState(update::Status* outUpdateStatus, const QnUuid& peerId);
    update::Status start();

    common::p2p::downloader::Downloader* downloader();
    int64_t freeSpace(const QString& path) const;

    void setTargetUpdateInformation(const update::Information& information);

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
};

} // namespace nx::vms::server
