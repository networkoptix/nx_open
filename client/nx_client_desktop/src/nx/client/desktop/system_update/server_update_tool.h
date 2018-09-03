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

namespace nx {
namespace client {
namespace desktop {

class UploadManager;
//using Downloader = vms::common::p2p::downloader::Downloader;
//using FileInformation = vms::common::p2p::downloader::FileInformation;

// A tool to interact with remote server state.
// Wraps up /api/update2 part of REST protocol.
// Note: this class should survive some time until its internal threads are dead.
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

    using RemoteStatus = std::map<QnUuid, nx::update::Status>;

    /** Generate url for download update file, depending on actual targets list. */
    static QUrl generateUpdatePackageUrl(const nx::utils::SoftwareVersion &targetVersion,
        const QString& targetChangeset, const QSet<QnUuid>& targets, QnResourcePool* resourcePool);

    // Check if we should sync UI and data from here.
    bool hasRemoteChanges() const;

    // Try to get status changes from the server
    // Should be non-blocking and fast.
    // Check if we've got update for all the servers
    bool getServersStatusChanges(RemoteStatus& status);

    void requestStartUpdate(const nx::update::Information& info);
    void requestStopAction(QSet<QnUuid> targets);
    void requestInstallAction(QSet<QnUuid> targets);

    // Sends GET https://localhost:7001/ec2/updateInformation and stores response in m_statusRequest
    void requestRemoteUpdateState();

    enum class OfflineUpdateState
    {
        Initial,
        Check,      //< Checking the contents of update file.
        Unpacking,  //< Unpacking all the packages.
        Push,       //< Pushing to the servers
        Done        //< All update contents are pushed to the servers. They can start the update.
    };

    enum class CheckMode
    {
        Internet,
        File,
        Mediaservers,
    };

    struct UpdateCheckResult
    {
        CheckMode mode;
        nx::update::Information info;
        nx::update::InformationError error = nx::update::InformationError::noError;
    };

    static UpdateCheckResult checkUpdateFromInternet(QString updateUrl);

    std::future<UpdateCheckResult> checkUpdateFromFile(QString file);

private:
    // Handlers for resource updates
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resourceChanged(const QnResourcePtr& resource);

    // We pass this callback to all our REST queries at /api/updates2
    void at_updateStatusResponse(bool success, rest::Handle handle, const std::vector<nx::update::Status>& response);
    // Handler for status update from a single server
    void at_updateStatusResponse(bool success, rest::Handle handle, const nx::update::Status& response);

    // Werapper to get REST connection to specified server.
    // For testing purposes. We can switch there to a dummy http server.
    rest::QnConnectionPtr getServerConnection(const QnMediaServerResourcePtr& server);

    OfflineUpdateState getOfflineUpdateState() const;
    void stopOfflineUpdate();

    // Called by QnZipExtractor when the offline update package is unpacked.
    void at_extractFilesFinished(int code);

    UpdateCheckResult readUpdateManifest(QString path);

private:
    OfflineUpdateState iterateOfflineUpdater();

    OfflineUpdateState m_offlineUpdaterState = OfflineUpdateState::Initial;
    QString m_offlineUpdateFile;
    std::promise<UpdateCheckResult> m_offlineUpdateCheckResult;

    std::shared_ptr<QnZipExtractor> m_extractor;
    //std::promise<UpdateCheckResult> m_offlineCheckResult;


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

    QTemporaryDir m_outputDir;
};

} // namespace desktop
} // namespace client
} // namespace nx
