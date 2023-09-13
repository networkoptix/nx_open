// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>

#include <api/server_rest_connection.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/software_version.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/vms/client/desktop/utils/upload_state.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/common/update/update_check_params.h>
#include <nx/vms/common/update/update_information.h>
#include <nx/zip/extractor.h>

namespace nx::vms::client::desktop {

class UploadManager;
class PeerStateTracker;
struct UpdateItem;

/**
 * A tool to interact with remote server state.
 * Wraps up the following requests:
 *   /ec2/updateInformation
 *   /api/installUpdate
 *   /ec2/cancelUpdate
 *   /ec2/updateStatus
 * It also deals with uploading offline update files to the server.
 * Note: this class should survive some time until its internal threads are dead.
 */
class NX_VMS_CLIENT_DESKTOP_API ServerUpdateTool:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using Downloader = vms::common::p2p::downloader::Downloader;
    using FileInformation = vms::common::p2p::downloader::FileInformation;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

public:
    ServerUpdateTool(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~ServerUpdateTool() override;

    // Check if we should sync UI and data from here.
    bool hasRemoteChanges() const;
    bool hasOfflineUpdateChanges() const;

    void onConnectToSystem(QnUuid systemId);
    void onDisconnectFromSystem();

    bool hasInitiatedThisUpdate() const;

    /**
     * Sends GET https://localhost:7001/ec2/updateInformation and stores response in an internal
     * cache. Data from consequent requests is integrated into this cache, for each server.
     * This data can be obtained by getServersStatusChanges method.
     */
    void requestRemoteUpdateStateAsync();

    using RemoteStatus = std::map<QnUuid, nx::vms::common::update::Status>;

    std::future<RemoteStatus> requestRemoteUpdateState();

    /**
     * Tries to get status changes from the server.
     * @param status storage for remote status
     * @returns true if there were some new data.
     * Consequent calls to this function will return false until new data arrives.
     */
    bool getServersStatusChanges(RemoteStatus& status);

    /**
     * Asks mediaservers to start the update process.
     * @param info - update manifest
     */
    bool requestStartUpdate(
        const nx::vms::common::update::Information& info, const QSet<QnUuid>& targets);

    /**
     * Asks mediaservers to stop the update process.
     * Client expects all mediaservers to return to nx::vms::common::update::Status::Code::idle state.
     */
    bool requestStopAction();

    /**
     * Asks mediaservers to retry current update action.
     */
    bool requestRetryAction();

    /**
     * Asks mediaservers to finish update process.
     * @param skipActivePeers - will force update completion even if there are
     *     some servers installing.
     */
    bool requestFinishUpdate(bool skipActivePeers);

    /**
     * Asks mediaservers to start installation process.
     */
    bool requestInstallAction(const QSet<QnUuid>& targets);

    /**
     * Send request for moduleInformation from the server.
     * Response arrives at moduleInformationReceived signal.
     */
    void requestModuleInformation();

    // State for uploading offline update package.
    enum class OfflineUpdateState
    {
        initial,
        // Unpacking all the packages and checking the contents.
        unpack,
        // Data is unpacked and we are ready to push packages to the servers.
        ready,
        // Pushing to the servers.
        push,
        // All update contents are pushed to the servers. They can start the update.
        done,
        // Failed to push all the data.
        error,
    };

    Q_ENUM(OfflineUpdateState);

    std::future<UpdateContents> checkForUpdate(const common::update::UpdateInfoParams& infoParams);

    std::future<UpdateContents> checkUpdateFromFile(const QString& file);
    std::future<UpdateContents> checkMediaserverUpdateInfo();
    std::future<UpdateContents> takeUpdateCheckFromFile();

    /**
     * Check if update info contains all the packages necessary to update the system.
     * @param contents - current update contents.
     * @param clientVersions - current client versions installed.
     * @param checkClient - check if we should check client update.
     */
    bool verifyUpdateManifest(
        UpdateContents& contents,
        const std::set<nx::utils::SoftwareVersion>& clientVersions, bool checkClient = true) const;

    /**
     * Checks if there are any uploads to specified server
     * @param id server uid
     * @return true if there are active uploads
     */
    bool hasActiveUploadsTo(const QnUuid& id) const;

    /**
     * Start uploading local update packages to the servers.
     * Note: The parameter `force` is a temporary workaround for retrying update uploads. It will
     * be removed as soon as the Client-side logic is rewritten.
     */
    bool startUpload(const UpdateContents& contents, bool cleanExisting, bool force = false);

    /** Stops all uploads. */
    void stopAllUploads();

    void saveInternalState();

    /**
     * Get a path to a folder with downloads.
     * ServerUpdateTool can download server packages for the servers without internet.
     */
    QDir getDownloadDir() const;

    /** Get available space in downloads folder. */
    uint64_t getAvailableSpace() const;

    /**
     * Report-like object to describe a progress for a current action.
     */
    struct ProgressInfo
    {
        int current = 0;
        int max = 0;
        int active = 0;
        int done = 0;
        /** This flags help UI to pick proper caption for a progress. */
        bool downloadingClient = false;
        bool downloadingServers = false;
        /** Client is downloading files for the servers without internet. */
        bool downloadingForServers = false;
        bool uploadingOfflinePackages = false;
        bool installingServers = false;
        bool installingClient = false;
    };

    enum class InternalError
    {
        noError,
        noConnection,
        networkError,
        serverError,
    };

    void calculateManualDownloadProgress(ProgressInfo& progress);

    OfflineUpdateState getUploaderState() const;

    void startUploadsToServer(const UpdateContents& contents, const QnUuid& peer);
    void stopUploadsToServer(const QnUuid& peer);

    bool haveActiveUpdate() const;

    /** Get recipients for upload for specified update package. */
    QSet<QnUuid> getTargetsForPackage(const nx::vms::update::Package& package) const;

    std::shared_ptr<PeerStateTracker> getStateTracker();

    // These are debug functions that return URL to appropriate mediaserver API calls.
    // This URLs are clickable at MultiServerUpdateWidget. This allows testers to
    // check current update state easily and fill in a meaningful bug report.
    QString getUpdateStateUrl() const;
    QString getUpdateInformationUrl() const;
    QString getInstalledUpdateInfomationUrl() const;

    /** Starts downloading the packages for the servers without internet. */
    void startManualDownloads(const UpdateContents& contents);

    /** Checks if there are any manual downloads. */
    bool hasManualDownloads() const;

    /**
     * Starts uploading package to the servers.
     * It will upload package to servers, mentioned in package.targets.
     * This field is set when we verify update contents.
     * @param package Package to be uploaded
     * @param sourceDir Directory that contains this package
     * @returns number of recipients for this package.
     */
    int uploadPackageToRecipients(const nx::vms::update::Package& package, const QDir& sourceDir);

    TimePoint::duration getInstallDuration() const;

    /**
     * Get a set of servers participating in the installation process.
     * This data is extracted from /ec2/updateInformation
     */
    QSet<QnUuid> getServersInstalling() const;

    static QString toString(InternalError errorCode);

signals:
    void packageDownloaded(const nx::vms::update::Package& package);
    void packageDownloadFailed(const nx::vms::update::Package& package, const QString& error);
    void moduleInformationReceived(const QList<nx::vms::api::ModuleInformation>& moduleInformation);

    /** Called when /ec2/startUpdate request is complete. */
    void startUpdateComplete(bool success, const QString& error);
    /** Called when /ec2/cancelUpdate request is complete. */
    void cancelUpdateComplete(bool success, const QString& error);
    /** Called when /ec2/finishUpdate request is complete. */
    void finishUpdateComplete(bool success, const QString& error);
    /** Called when /ec2/installUpdate request is complete. */
    void startInstallComplete(bool success, const QString& error);

private:
    void atUpdateStatusResponse(bool success, rest::Handle handle, const std::vector<nx::vms::common::update::Status>& response);
    void atUploadWorkerState(QnUuid serverId, const nx::vms::client::desktop::UploadState& state);
    // Called by nx::zip::Extractor when the offline update package is unpacked.
    void atExtractFilesFinished(nx::zip::Extractor::Error code);

    void atRequestServerFreeSpaceResponse(bool success, rest::Handle handle,
        const QnUpdateFreeSpaceReply& reply);

    // Wrapper to get REST connection to specified server.
    // For testing purposes. We can switch there to a dummy http server.
    rest::ServerConnectionPtr getServerConnection(const QnMediaServerResourcePtr& server) const;
    static void readUpdateManifest(const QString& path, UpdateContents& result);
    QnMediaServerResourceList getServersForUpload();

    void markUploadCompleted(const QString& uploadId);
    void loadInternalState();
    void changeUploadState(OfflineUpdateState newState);

    /** Callbacks for m_downloader. */
    void atDownloaderStatusChanged(const FileInformation& fileInformation);
    void atChunkDownloadFailed(const QString& fileName);
    void atDownloadFailed(const QString& fileName);
    void atDownloadFinished(const QString& fileName);
    void atDownloadStalled(const QString& fileName, bool stalled);

    const nx::vms::update::Package* findPackageForFile(const QString& fileName) const;

    void dropAllRequests(const QString& reason);
    bool uploadPackageToServer(const QnUuid& serverId,
        const nx::vms::update::Package& package, QDir storageDir);

private:
    OfflineUpdateState m_offlineUpdaterState = OfflineUpdateState::initial;
    bool m_offlineUpdateStateChanged = false;
    bool m_initiatedUpdate = false;

    QString m_offlineUpdateFile;
    std::promise<UpdateContents> m_offlineUpdateCheckResult;

    std::shared_ptr<nx::zip::Extractor> m_extractor;

    // Container for remote state.
    // We keep temporary state updates here. Widget will pull this data periodically.
    RemoteStatus m_remoteUpdateStatus;
    mutable std::recursive_mutex m_statusLock;

    // For pushing update package to the server swarm. Will be replaced by a p2p::Downloader.
    std::unique_ptr<UploadManager> m_uploadManager;
    std::set<QString> m_activeUploads;
    std::set<QString> m_completedUploads;

    // It helps to track downloading progress for files.
    std::map<QString, int> m_activeDownloads;
    std::set<QString> m_completeDownloads;
    std::set<QString> m_issuedDownloads;

    std::map<QString, UploadState> m_uploadStateById;

    // Current update manifest. We get it from mediaservers, or overriding it by calling
    // requestStartUpdate(...) method.
    nx::vms::common::update::Information m_remoteUpdateManifest;

    std::future<UpdateContents> m_checkFileUpdate;

    // Cached path to file with offline updates.
    QString m_localUpdateFile;
    QDir m_outputDir;

    QSet<rest::Handle> m_activeRequests;
    QSet<rest::Handle> m_skippedRequests;
    /** Expected handle for /ec2/updateStatus response. We will ignore any other responses. */
    rest::Handle m_expectedStatusHandle = 0;
    std::atomic_bool m_requestingStop = false;
    std::atomic_bool m_requestingFinish = false;

    std::shared_ptr<PeerStateTracker> m_stateTracker;

    // Time at which install command was issued.
    std::chrono::milliseconds m_timeStartedInstall{0};
    QSet<rest::Handle> m_requestingInstall;

    /** We use this downloader when client downloads updates for server without internet. */
    std::unique_ptr<Downloader> m_downloader;
    /** List of packages to be downloaded and uploaded by the client. */
    QSet<nx::vms::update::Package> m_manualPackages;
    /** Additional package properties. */
    QHash<QString, UpdateContents::PackageProperties> m_packageProperties;
    /** Direct connection to the mediaserver. */
    rest::ServerConnectionPtr m_serverConnection;

    /**
     * A set of servers that were participating in update.
     * This information is extracted from ec2/updateInformation request.
     */
    QSet<QnUuid> m_serversAreInstalling;
    QnUuid m_systemId;

    rest::Handle m_settingsRequest = 0;
};

} // namespace nx::vms::client::desktop
