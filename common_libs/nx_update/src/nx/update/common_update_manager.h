#pragma once

#include <atomic>

#include <common/common_module_aware.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/update_information.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/uuid.h>
#include <api/model/audit/auth_session.h>

namespace nx {

namespace update {struct Package; }

class CommonUpdateInstaller;

class NX_UPDATE_API CommonUpdateManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    CommonUpdateManager(QnCommonModule* commonModule);
    void connectToSignals();
    update::Status status();
    void cancel();
    void startUpdate(const QByteArray& content);
    void install(const QnAuthSession& authInfo);

private:
    std::atomic<bool> m_downloaderFailed = false;

    void onGlobalUpdateSettingChanged();
    void onDownloaderFailed(const QString& fileName);
    void onDownloaderFinished(const QString& fileName);
    bool findPackage(nx::update::Package* outPackage) const;
    bool canDownloadFile(const QString& fileName, update::Status* outUpdateStatus);
    bool statusAppropriateForDownload(nx::update::Package* outPackage, update::Status* outStatus);
    bool installerState(update::Status* outUpdateStatus, const QnUuid& peerId);
    update::Status start();

    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual CommonUpdateInstaller* installer() = 0;
};

} // namespace nx
