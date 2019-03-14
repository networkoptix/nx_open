#pragma once

#include <atomic>

#include <common/common_module_aware.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/update_information.h>
#include <nx/update/update_check.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/uuid.h>

struct QnAuthSession;

namespace nx {

namespace update { struct Package; }

class CommonUpdateInstaller;

class CommonUpdateManager: public QObject, public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT

public:
    CommonUpdateManager(QnCommonModule* commonModule);
    void connectToSignals();
    update::Status status();
    void cancel();
    void startUpdate(const QByteArray& content);
    void install(const QnAuthSession& authInfo);
    bool participants(QList<QnUuid>* outParticipants) const;
    bool setParticipants(const QList<QnUuid>& participants);
    bool updateLastInstallationRequestTime();
    void finish();
    vms::api::SoftwareVersion targetVersion() const;

private:
    enum class DownloaderFailDetail
    {
        noError,
        internalError,
        noFreeSpace,
        downloadFailed
    };

    std::atomic<DownloaderFailDetail> m_downloaderFailDetail = DownloaderFailDetail::noError;

    void onGlobalUpdateSettingChanged();
    void onDownloaderFailed(const QString& fileName);
    void onDownloaderFinished(const QString& fileName);
    void onDownloaderFileStatusChanged(
        const vms::common::p2p::downloader::FileInformation& fileInformation);
    update::FindPackageResult findPackage(
        nx::update::Package* outPackage,
        QString* outMessage = nullptr) const;
    bool canDownloadFile(const QString& fileName, update::Status* outUpdateStatus);
    bool statusAppropriateForDownload(nx::update::Package* outPackage, update::Status* outStatus);
    bool installerState(update::Status* outUpdateStatus, const QnUuid& peerId);
    update::Status start();
    bool deserializedUpdateInformation(update::Information* outUpdateInformation,
        const QString& caller) const;
    void setUpdateInformation(const update::Information& updateInformation);

    virtual vms::common::p2p::downloader::Downloader* downloader() = 0;
    virtual CommonUpdateInstaller* installer() = 0;
};

} // namespace nx
