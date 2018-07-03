#pragma once

#include <functional>

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>
#include <update/update_process.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/system_information.h>
#include <utils/common/connective.h>

#include <api/server_rest_connection.h>

//class QnCheckForUpdatesPeerTask;
class QnUploadUpdatesPeerTask;
class QnInstallUpdatesPeerTask;
class QnRestUpdatePeerTask;
struct QnLowFreeSpaceWarning;

namespace nx {
namespace client {
namespace desktop {

// A tool to interact with remote server state.
// Wraps up /api/update2 part of REST protocol.
// Note: do not add here anything, not related to remote server state.
class ServerUpdateTool: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:
    ServerUpdateTool(QObject* parent = nullptr);
    ~ServerUpdateTool();

    void setResourceFeed(QnResourcePool* pool);

    //QList<QnMediaServerResourcePtr> getOfflineServers() const;
    //QList<QnMediaServerResourcePtr> getIncompatibleServers() const;

    using UpdateStatus = std::map<QnUuid, nx::api::Updates2StatusData>;

    QnFullUpdateStage stage() const;

    bool isUpdating() const;
    bool idle() const;

    struct LegacyUpdateStatus
    {
        // Overall progress
        int progress = -1;
        std::map<QnUuid, QnPeerUpdateStage> peerStage;
        std::map<QnUuid, int> peerProgress;
        QnFullUpdateStage stage;
    };

    /** Generate url for download update file, depending on actual targets list. */
    static QUrl generateUpdatePackageUrl(const nx::utils::SoftwareVersion &targetVersion,
        const QString& targetChangeset, const QSet<QnUuid>& targets, QnResourcePool* resourcePool);

    // Old update API
   // void startUpdate(const QnSoftwareVersion &version = QnSoftwareVersion());
    //void startUpdate(const QString &fileName);
    void startOnlineClientUpdate(const QSet<QnUuid>& targets, const nx::utils::SoftwareVersion &version, bool enableClientUpdates);

    bool canCancelUpdate() const;
    bool cancelUpdate();

    // Check if we should sync UI and data from here.
    bool hasRemoteChanges() const;
    // Try to get status changes from the server
    // Should be non-blocking and fast.
    // Check if we've got update for all the servers
    bool getServersStatusChanges(UpdateStatus& status);

    // Check if update status for legacy updater has changed
    bool getLegacyUpdateStatusChanges(LegacyUpdateStatus& status);

    // Wrappers for REST API
    // Sends POST to /api/updates2/status/all
    // Result can be obtained by polling getServerStatusChanges
    void requestUpdateActionAll(nx::api::Updates2ActionData::ActionCode action);

    // Sends POST to /api/updates2/status for each target
    // Result can be obtained by polling getServerStatusChanges
    void requestUpdateAction(nx::api::Updates2ActionData::ActionCode action, QSet<QnUuid> targets);

    // Sends GET to /api/updates2/status/all and stores response in m_statusRequest
    void requestRemoteUpdateState();

signals:
    void stageChanged(QnFullUpdateStage stage);
    void stageProgressChanged(QnFullUpdateStage stage, int progress);
    //void peerStageChanged(const QnUuid &peerId, QnPeerUpdateStage stage);
    //void peerStageProgressChanged(const QnUuid &peerId, QnPeerUpdateStage stage, int progress);

    //void targetsChanged(const QSet<QnUuid> &targets);

    void updateFinished(QnUpdateResult result);
    void lowFreeSpaceWarning(QnLowFreeSpaceWarning& lowFreeSpaceWarning);

private:
    void startUpdate(const QnUpdateTarget &target);

    // Handlers for resource updates
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);

    // We pass this callback to all our REST queries at /api/updates2
    void at_updateStatusResponse(bool success, rest::Handle handle, const std::vector<nx::api::Updates2StatusData>& response);
    // Handler for status update from a single server
    void at_updateStatusResponse(bool success, rest::Handle handle, const nx::api::Updates2StatusData& response);

    void finishUpdate(const QnUpdateResult &result);

    // Werapper to get REST connection to specified server.
    // For testing purposes. We can switch there to a dummy http server.
    rest::QnConnectionPtr getServerConnection(const QnMediaServerResourcePtr& server);

private:
    QnFullUpdateStage m_stage;

    QPointer<QnUpdateProcess> m_updateProcess;
    // Container for remote state
    // We keep temporary state updates here. Client will pull this data periodically
    UpdateStatus m_remoteUpdateStatus;
    bool m_checkingRemoteUpdateStatus = false;
    // Update status for legacy update system.
    // It is handled in the same polling way.
    LegacyUpdateStatus m_legacyStatus;
    bool m_legacyStatusChanged = false;
    // Servers we do work with.
    std::map<QnUuid, QnMediaServerResourcePtr> m_activeServers;

    // Explicit connections to resource pool events
    QMetaObject::Connection m_onAddedResource, m_onRemovedResource, m_onUpdatedResource;
};

} // namespace desktop
} // namespace client
} // namespace nx
