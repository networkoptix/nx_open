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

class CommonUpdateManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    CommonUpdateManager(QnCommonModule* commonModule);
    void connectToSignals();
    update::Status status();
    void cancel();
    void startUpdate(const QByteArray& content);
    void install(const QnAuthSession& authInfo);
    QList<QnUuid> participants() const;

private:
    std::atomic<bool> m_downloaderFailed = false;

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

    virtual vms::common::p2p::downloader::Downloader* downloader() = 0;
    virtual CommonUpdateInstaller* installer() = 0;
};

} // namespace nx
