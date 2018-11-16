#pragma once

#include <functional>

#include <QtCore>

#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <nx/update/common_update_manager.h>
#include <nx/update/update_check.h>
#include <nx/update/update_information.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/system_information.h>

#include <utils/common/connective.h>
#include <api/server_rest_connection.h>
#include <utils/update/zip_utils.h>
#include <nx/vms/client/desktop/utils/upload_state.h>

#include "update_contents.h"

namespace nx::vms::client::desktop {

class UploadManager;
class ServerUpdatesModel;

using Downloader = vms::common::p2p::downloader::Downloader;
using FileInformation = vms::common::p2p::downloader::FileInformation;

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
class ServerUpdateTool:
        public Connective<QObject>,
        public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    ServerUpdateTool(QObject* parent = nullptr);
    ~ServerUpdateTool();

    void setResourceFeed(QnResourcePool* pool);

    // Check if we should sync UI and data from here.
    bool hasRemoteChanges() const;
    bool hasOfflineUpdateChanges() const;

    /**
     * Sends GET https://localhost:7001/ec2/updateInformation and stores response in an internal
     * cache. Data from consequent requests is integrated into this cache, for each server.
     * This data can be obtained by getServersStatusChanges method.
     */
    void requestRemoteUpdateState();

    using RemoteStatus = std::map<QnUuid, nx::update::Status>;
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
    void requestStartUpdate(const nx::update::Information& info);

    /**
     * Asks mediaservers to stop the update process.
     * Client expects all mediaservers to return to nx::update::Status::Code::idle state.
     */
    void requestStopAction();

    /**
     * Asks mediaservers to start installation process.
     */
    void requestInstallAction(QSet<QnUuid> targets);

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

    std::future<UpdateContents> checkUpdateFromFile(QString file);
    std::future<UpdateContents> checkRemoteUpdateInfo();
    // It is used to obtain future to update check that was started
    // inside loadInternalState method
    // TODO: move all state restoration logic to widget.
    std::future<UpdateContents> getUpdateCheck();

    // Check if update info contains all the packages necessary to update the system.
    // @param contents - current update contents.
    bool verifyUpdateManifest(UpdateContents& contents) const;

    // Start uploading local update packages to the server(s).
    bool startUpload(const UpdateContents& contents);
    void stopUpload();

    struct ProgressInfo
    {
        int current = 0;
        int max = 0;
        // This flags help UI to pick proper caption for a progress.
        bool downloadingClient = false;
        bool downloadingServers = false;
        bool uploading = false;
        bool installingServers = false;
        bool installingClient = false;
    };

    // Calculates a progress for uploading files to the server.
    ProgressInfo calculateUploadProgress();

    OfflineUpdateState getUploaderState() const;

    // Get information about current update from mediaservers.
    UpdateContents getRemoteUpdateContents() const;

    bool haveActiveUpdate() const;

    // Get current set of servers.
    QSet<QnUuid> getAllServers() const;

    std::map<QnUuid, nx::update::Status::Code> getAllServerStates() const;

    // Get servers with specified update status.
    QSet<QnUuid> getServersInState(nx::update::Status::Code status) const;

    // Get servers that are offline right now.
    QSet<QnUuid> getOfflineServers() const;

    // Get servers that are incompatible with the new update system.
    QSet<QnUuid> getLegacyServers() const;

    // Get servers that have completed installation process.
    QSet<QnUuid> getServersCompleteInstall() const;

    // Get servers that have started update installation.
    // Note: only client, that have sent an 'install' command can know about this state.
    QSet<QnUuid> getServersInstalling() const;

    // Get servers, which are installing updates for too long.
    QSet<QnUuid> getServersWithStalledUpdate() const;

    // Get servers with updated protocol.
    QSet<QnUuid> getServersWithChangedProtocol() const;

    std::shared_ptr<ServerUpdatesModel> getModel();

    // These are debug functions that return URL to appropriate mediaserver API calls.
    // This URLs are clickable at MultiServerUpdateWidget. This allows testers to
    // check current update state easily and fill in a meaningful bug report.
    QString getUpdateStateUrl() const;
    QString getUpdateInformationUrl() const;
    QString getInstalledUpdateInfomationUrl() const;

private:
    // Handlers for resource updates.
    void atResourceAdded(const QnResourcePtr& resource);
    void atResourceRemoved(const QnResourcePtr& resource);
    void atResourceChanged(const QnResourcePtr& resource);

    void atUpdateStatusResponse(bool success, rest::Handle handle, const std::vector<nx::update::Status>& response);
    void atUploadWorkerState(QnUuid serverId, const nx::vms::client::desktop::UploadState& state);
    // Called by QnZipExtractor when the offline update package is unpacked.
    void atExtractFilesFinished(int code);

    void atPingTimerTimeout();

    // Wrapper to get REST connection to specified server.
    // For testing purposes. We can switch there to a dummy http server.
    rest::QnConnectionPtr getServerConnection(const QnMediaServerResourcePtr& server) const;
    static void readUpdateManifest(QString path, UpdateContents& result);
    QnMediaServerResourceList getServersForUpload();

    void markUploadCompleted(QString uploadId);
    void saveInternalState();
    void loadInternalState();
    void changeUploadState(OfflineUpdateState newState);

private:
    OfflineUpdateState m_offlineUpdaterState = OfflineUpdateState::initial;
    bool m_offlineUpdateStateChanged = false;

    QString m_offlineUpdateFile;
    std::promise<UpdateContents> m_offlineUpdateCheckResult;

    std::shared_ptr<QnZipExtractor> m_extractor;

    // Container for remote state.
    // We keep temporary state updates here. Widget will pull this data periodically.
    RemoteStatus m_remoteUpdateStatus;
    bool m_checkingRemoteUpdateStatus = false;
    mutable std::recursive_mutex m_statusLock;

    // Servers we do work with.
    std::map<QnUuid, QnMediaServerResourcePtr> m_activeServers;

    // Explicit connections to resource pool events.
    QMetaObject::Connection m_onAddedResource, m_onRemovedResource, m_onUpdatedResource;

    // For pushing update package to the server swarm. Will be replaced by a p2p::Downloader.
    std::unique_ptr<UploadManager> m_uploadManager;
    std::set<QString> m_activeUploads;
    std::set<QString> m_completedUploads;
    std::map<QString, nx::vms::client::desktop::UploadState> m_uploadStateById;

    // Current update manifest. We get it from mediaservers, or overriding it by calling
    // requestStartUpdate(...) method.
    nx::update::Information m_updateManifest;

    std::future<UpdateContents> m_updateCheck;

    // Path to a remote folder with update packages.
    QString m_uploadDestination;
    // Cached path to file with offline updates.
    QString m_localUpdateFile;
    QDir m_outputDir;

    QSet<rest::Handle> m_activeRequests;
    QSet<rest::Handle> m_skippedRequests;

    std::shared_ptr<ServerUpdatesModel> m_updatesModel;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    // Time at which install command was issued.
    TimePoint m_timeStartedInstall;
    bool m_protoProblemDetected = false;
    QSet<rest::Handle> m_requestingInstall;
};

/**
 * Generates URL for upcombiner.
 * Upcombiner is special server utility, that combines several update packages
 * to a single zip archive.
 */
QUrl generateUpdatePackageUrl(const UpdateContents& contents, const QSet<QnUuid>& targets, QnResourcePool* resourcePool);

} // namespace nx::vms::client::desktop
