#pragma once

#include <QtCore>
#include <atomic>
#include <common/common_module_aware.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {

namespace update {struct Package; }

class CommonUpdateInstaller;

struct NX_UPDATE_API UpdateStatus
{
    Q_GADGET

public:
    enum class Code
    {
        idle,
        downloading,
        readyToInstall,
        preparing,
        offline,
        installing,
        error
    };
    Q_ENUM(Code)

    QnUuid serverId;
    Code code = Code::idle;
    QString message;
    double progress = 0.0;

    UpdateStatus() = default;
    UpdateStatus(
        const QnUuid& serverId,
        Code code,
        const QString& message = QString(),
        double progress = 0.0)
        :
        serverId(serverId),
        code(code),
        message(message),
        progress(progress)
    {}
};

#define UpdateStatus_Fields (serverId)(code)(progress)(message)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(UpdateStatus::Code)
QN_FUSION_DECLARE_FUNCTIONS(UpdateStatus::Code, (lexical), NX_UPDATE_API)
QN_FUSION_DECLARE_FUNCTIONS(UpdateStatus, (xml)(csv_record)(ubjson)(json), NX_UPDATE_API)

class NX_UPDATE_API CommonUpdateManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    CommonUpdateManager(QnCommonModule* commonModule);
    void connectToSignals();
    UpdateStatus status();
    void cancel();
    void startUpdate(const QByteArray& content);
    void install();

private:
    std::atomic<bool> m_downloaderFailed = false;

    void onGlobalUpdateSettingChanged();
    void onDownloaderFailed(const QString& fileName);
    void onDownloaderFinished(const QString& fileName);
    bool findPackage(nx::update::Package* outPackage) const;
    bool canDownloadFile(const QString& fileName, UpdateStatus* outUpdateStatus);
    bool statusAppropriateForDownload(nx::update::Package* outPackage, UpdateStatus* outStatus);
    bool installerState(UpdateStatus* outUpdateStatus, const QnUuid& peerId);
    UpdateStatus start();

    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual CommonUpdateInstaller* installer() = 0;
};

} // namespace nx
