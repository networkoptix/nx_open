#pragma once

#include <functional>

#include <QtCore>

#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <nx/update/common_update_manager.h>
#include <nx/update/update_check.h>
#include <nx/update/update_information.h>

#include <update/updates_common.h>
#include <update/update_process.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/system_information.h>

#include <utils/common/connective.h>
#include <api/server_rest_connection.h>
#include <utils/update/zip_utils.h>
#include <nx/client/desktop/utils/upload_state.h>

namespace nx {
namespace client {
namespace desktop {

class UploadManager;
//using Downloader = vms::common::p2p::downloader::Downloader;
//using FileInformation = vms::common::p2p::downloader::FileInformation;

/**
 * A tool to interact with remote server state.
 * Wraps up the following requests:
 *   /ec2/updateInformation
 *   /api/installUpdate
 *   /ec2/cancelUpdate
 *   /ec2/updateStatus
 * Note: this class should survive some time until its internal threads are dead.
 */
class ServerUpdateTool:
        public Connective<QObject>,
        public QnConnectionContextAware,
        public std::enable_shared_from_this<ServerUpdateTool>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    ServerUpdateTool(QObject* parent = nullptr);
    ~ServerUpdateTool();

    void setResourceFeed(QnResourcePool* pool);

    /** Generate url for download update file, depending on actual targets list. */
    static QUrl generateUpdatePackageUrl(const nx::utils::SoftwareVersion &targetVersion,
        const QString& targetChangeset, const QSet<QnUuid>& targets, QnResourcePool* resourcePool);

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
     * Tries to get status changes from the server
     * @param status storage for remote status
     * @returns true if there were some new data.
     * Consequent calls to this function will return false until new data arrives.
     */
    bool getServersStatusChanges(RemoteStatus& status);

    /**
     * Asks mediaservers to start the update process
     * @param info - update manifest
     */
    void requestStartUpdate(const nx::update::Information& info);

    /**
     * Asks mediaservers to stop the update process
     * Client expects all mediaservers to return to nx::update::Status::Code::idle state.
     */
    void requestStopAction();

    /**
     * Asks mediaservers to start installation process
     */
    void requestInstallAction(QSet<QnUuid> targets);

    // State for uploading offline update package
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

    enum class UpdateCheckMode
    {
        // Got update info from the internet.
        internet,
        // Got update info from the internet for specific build.
        internetSpecific,
        // Got update info from offline update package.
        file,
        // Got update info from mediaserver swarm.
        mediaservers,
    };

    /**
     * Wraps up update info and summary for its verification.
     */
    struct UpdateContents
    {
        UpdateCheckMode mode = UpdateCheckMode::internet;
        QString source;

        // A set of servers without proper update file.
        QSet<QnMediaServerResourcePtr> missingUpdate;
        // A set of servers that can not accept update version.
        QSet<QnMediaServerResourcePtr> invalidVersion;

        QString eulaPath;

        // A list of files to be uploaded.
        QStringList filesToUpload;
        // Information for the clent update.
        nx::update::Package clientPackage;

        nx::update::Information info;
        nx::update::InformationError error = nx::update::InformationError::noError;

        // Check if we can apply this update.
        bool isValid() const;
    };

    std::future<UpdateContents> checkLatestUpdate();
    std::future<UpdateContents> checkSpecificChangeset(QString build, QString password);
    std::future<UpdateContents> checkUpdateFromFile(QString file);

    // Check if update info contains all the packages necessary to update the system.
    // @param contents - current update contents.
    // @param root - path to directory with update package.
    bool verifyUpdateManifest(UpdateContents& contents, QDir root = QDir("")) const;

    // Start uploading local update packages to the server(s).
    void startUpload(const UpdateContents& contents);
    void stopUpload();

    struct ProgressInfo
    {
        int current = 0;
        int max = 0;
    };

    // Calculates a progress for uploading files to the server.
    ProgressInfo calculateUploadProgress();
    // Calculates a progress for remote downloading.
    ProgressInfo calculateRemoteDownloadProgress();
    // Calculates a progress for remote downloading.
    ProgressInfo calculateInstallProgress();

    OfflineUpdateState getUploaderState() const;

    // Get information about current update
    nx::update::Information getActiveUpdateInformation() const;
    bool haveActiveUpdate() const;

private:
    // Handlers for resource updates.
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resourceChanged(const QnResourcePtr& resource);

    void at_updateStatusResponse(bool success, rest::Handle handle, const std::vector<nx::update::Status>& response);
    // Handler for status update from a single server.
    void at_updateStatusResponse(bool success, rest::Handle handle, const nx::update::Status& response);

    void at_uploadWorkerState(QnUuid serverId, const nx::client::desktop::UploadState& state);

    // Werapper to get REST connection to specified server.
    // For testing purposes. We can switch there to a dummy http server.
    rest::QnConnectionPtr getServerConnection(const QnMediaServerResourcePtr& server);

    void stopOfflineUpdate();

    // Called by QnZipExtractor when the offline update package is unpacked.
    void at_extractFilesFinished(int code);

    static void readUpdateManifest(QString path, UpdateContents& result);
    QnMediaServerResourceList getServersForUpload();

    void markUploadCompleted(QString uploadId);

private:

    OfflineUpdateState m_offlineUpdaterState = OfflineUpdateState::initial;
    bool m_offlineUpdateStateChanged = false;

    QString m_offlineUpdateFile;
    std::promise<UpdateContents> m_offlineUpdateCheckResult;

    std::shared_ptr<QnZipExtractor> m_extractor;

    // Container for remote state
    // We keep temporary state updates here. Client will pull this data periodically
    RemoteStatus m_remoteUpdateStatus;
    bool m_checkingRemoteUpdateStatus = false;
    mutable std::recursive_mutex m_statusLock;

    // Servers we do work with.
    std::map<QnUuid, QnMediaServerResourcePtr> m_activeServers;

    // Explicit connections to resource pool events
    QMetaObject::Connection m_onAddedResource, m_onRemovedResource, m_onUpdatedResource;

    // For pushing update package to the server swarm. Will be replaced by a p2p::Downloader.
    std::unique_ptr<UploadManager> m_uploadManager;
    std::set<QString> m_activeUploads;
    std::set<QString> m_completedUploads;
    std::map<QString, nx::client::desktop::UploadState> m_uploadState;

    // Current update manifest.
    nx::update::Information m_updateManifest;

    QString m_uploadDestination;
    // Cached path to file with offline updates.
    QString m_localUpdateFile;
    QDir m_outputDir;
    QString m_changeset;

    //std::unique_ptr<Downloader> m_downloader;
    // Downloader needs this strange thing.
    //std::unique_ptr<vms::common::p2p::downloader::AbstractPeerManagerFactory> m_peerManagerFactory;
};

} // namespace desktop
} // namespace client
} // namespace nx
