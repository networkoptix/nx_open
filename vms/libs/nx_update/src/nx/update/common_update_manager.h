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
    enum class InformationCategory
    {
        target,
        installed
    };

    CommonUpdateManager(QnCommonModule* commonModule);
    void connectToSignals();
    update::Status status();
    void cancel();
    void retry(bool forceRedownload = false);
    void startUpdate(const QByteArray& content);
    void install(const QnAuthSession& authInfo);

    update::Information updateInformation(InformationCategory category) const noexcept(false);
    update::Information updateInformation(InformationCategory category, bool* ok) const;

    void setUpdateInformation(
        InformationCategory category,
        const update::Information& information) noexcept(false);

    void setUpdateInformation(
        InformationCategory category,
        const update::Information& information,
        bool* ok);

    void finish();

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
    void extract();
    void clearDownloader(bool force);
    bool canDownloadFile(const nx::update::Package& package, update::Status* outUpdateStatus);
    bool statusAppropriateForDownload(nx::update::Package* outPackage, update::Status* outStatus);
    bool installerState(update::Status* outUpdateStatus, const QnUuid& peerId);
    update::Status start();

    virtual vms::common::p2p::downloader::Downloader* downloader() = 0;
    virtual CommonUpdateInstaller* installer() = 0;
    virtual int64_t freeSpace(const QString& path) const = 0;
};

} // namespace nx
