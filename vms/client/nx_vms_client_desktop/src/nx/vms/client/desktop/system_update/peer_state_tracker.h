#pragma once

#include <memory>
#include <chrono>

#include <QtCore/QAbstractTableModel>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/software_version.h>
#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/update/common_update_manager.h>

namespace nx::vms::client::desktop {

using StatusCode = nx::update::Status::Code;

/**
 * This structure keeps all the information necessary to display current state of the server.
 */
struct UpdateItem
{
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    enum class Component
    {
        server,
        client,
    };

    QnUuid id;
    StatusCode state = StatusCode::offline;
    int progress = -1;
    QString statusMessage;
    QString verificationMessage;

    /** Current version of the component. */
    nx::utils::SoftwareVersion version;
    Component component;

    bool incompatible = false;
    /** Flag for servers, that can be updated using legacy 3.2 system. */
    bool onlyLegacyUpdate = false;
    /** Client is uploading files to this server. */
    bool uploading = false;
    bool offline = false;
    bool skipped = false;
    bool installed = false;
    bool changedProtocol = false;
    bool installing = false;
    bool storeUpdates = true;
    /**
     * Actual status can become unknown when we just issue update command. We should wait for
     * another response from /ec2/updateStatus to get relevant state.
     */
    bool statusUnknown = false;
    /** The last time we've seen this peer online. */
    TimePoint lastTimeOnline;
    /** Row in the table. */
    int row = -1;
};

using UpdateItemPtr = std::shared_ptr<UpdateItem>;

class PeerStateTracker:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    PeerStateTracker(QObject* parent = nullptr);

    /**
     * Attaches state tracker to a resource pool. All previous attachments are discarded.
     * @param pool Pointer to the resource pool.
     * @return False if got empty resource pool or systemId.
     */
    bool setResourceFeed(QnResourcePool* pool);

    UpdateItemPtr findItemById(QnUuid id) const;
    UpdateItemPtr findItemByRow(int row) const;

    int peersCount() const;
    /**
     * Get pointer to mediaserver from UpdateItem.
     * It will return nullptr if the item is not a server.
     */
    QnMediaServerResourcePtr getServer(const UpdateItemPtr& item) const;

    /**
     * Get pointer to mediaserver from QnUuid.
     * It will return nullptr if the item is not a server.
     */
    QnMediaServerResourcePtr getServer(QnUuid id) const;

    QnUuid getClientPeerId() const;

    nx::utils::SoftwareVersion lowestInstalledVersion();
    void setUpdateTarget(const nx::utils::SoftwareVersion& version);
    void setUpdateStatus(const std::map<QnUuid, nx::update::Status>& statusAll);
    void markStatusUnknown(const QSet<QnUuid>& targets);
    /**
     * Forcing update for mediaserver versions.
     * We have used direct api call to get this module information.
     */
    void setVersionInformation(const QList<nx::vms::api::ModuleInformation>& moduleInformation);
    /**
     * Set 'installing' flag for specified peers. Mediaserver API provides no information about
     * whether servers are installing anything or not, so client should speculate about it when
     * 'Install update' button is clicked.
     */
    void setPeersInstalling(const QSet<QnUuid>& targets, bool installing);
    void clearState();

    /** Set update check error for specified set of peers. */
    void setVerificationError(const QMap<QnUuid, QString>& errors);

    void setVerificationError(const QSet<QnUuid>& targets, const QString& message);

    /** Clear update check errors for all the peers. */
    void clearVerificationErrors();

    bool hasVerificationErrors() const;
    bool hasStatusErrors() const;

public:
    std::map<QnUuid, nx::update::Status::Code> getAllPeerStates() const;
    std::map<QnUuid, QnMediaServerResourcePtr> getActiveServers() const;

    QList<UpdateItemPtr> getAllItems() const;
    QSet<QnUuid> getAllPeers() const;
    QSet<QnUuid> getPeersInState(StatusCode state) const;
    QSet<QnUuid> getLegacyServers() const;
    QSet<QnUuid> getOfflineServers() const;
    QSet<QnUuid> getPeersInstalling() const;
    QSet<QnUuid> getPeersCompleteInstall() const;
    QSet<QnUuid> getServersWithChangedProtocol() const;
    QSet<QnUuid> getPeersWithUnknownStatus() const;

    /**
     * Processing for task sets. These functions are called every 1sec from
     * MultiServerUpdatesWidget.
     */
    void processDownloadTaskSet();
    void processInstallTaskSet();

    /**
     * Getters for task sets.
     */

    /** Set task set of peers that completed current action. */
    QSet<QnUuid> getPeersComplete() const;
    /** Get task set of peers that are currently active. */
    QSet<QnUuid> getPeersActive() const;
    /** Get task set of peers that are participating in current action. */
    QSet<QnUuid> getPeersIssued() const;
    /** Get task set of peers that have failed current action. */
    QSet<QnUuid> getPeersFailed() const;

    /**
     * We call it every time we start or stop next task, like ready->downloading,
     * readyToInstall->installing or when we cancel current action.
     * It will reset all internal task sets.
     */
    void setTask(const QSet<QnUuid>& targets);

    void setTaskError(const QSet<QnUuid>& targets, const QString& error);

public:
    /**
     * We connect it to ClientUpdateTool::updateStateChanged and turn it to a proper
     * peer state inside our task sets.
     */
    void atClientupdateStateChanged(int state, int percentComplete);

signals:
    /**
     * Called after UpdateItem is already added to the list.
     */
    void itemAdded(UpdateItemPtr item);
    void itemChanged(UpdateItemPtr item);

    /**
     * Called right before UpdateItem is removed from the list.
     * Such order is necessary for any QAbstractItemModel: it should complete its actions before
     * actual number of items changes.
     */
    void itemToBeRemoved(UpdateItemPtr item);

    /**
     * Called after UpdateItem has been removed from the list.
     */
    void itemRemoved(UpdateItemPtr item);

protected:
    // Handlers for resource updates.
    void atResourceAdded(const QnResourcePtr& resource);
    void atResourceRemoved(const QnResourcePtr& resource);
    void atResourceChanged(const QnResourcePtr& resource);

protected:
    UpdateItemPtr addItemForServer(QnMediaServerResourcePtr server);
    UpdateItemPtr addItemForClient();
    /** Reads data from resource to UpdateItem. */
    bool updateServerData(QnMediaServerResourcePtr server, UpdateItemPtr item);
    bool updateClientData();
    void updateContentsIndex();

private:
    /** Explicit connections to resource pool events. */
    QMetaObject::Connection m_onAddedResource, m_onRemovedResource;

    /** The version we are going to install. */
    nx::utils::SoftwareVersion m_targetVersion;

    QList<UpdateItemPtr> m_items;
    UpdateItemPtr m_clientItem;

    /** Servers we do work with. */
    std::map<QnUuid, QnMediaServerResourcePtr> m_activeServers;

    mutable QnMutex m_dataLock;

    /**
     * This sets are changed every time we are initiating some update action.
     * Set of servers that are currently active.
     */
    QSet<QnUuid> m_peersActive;
    /** Set of servers that are used for the current task. */
    QSet<QnUuid> m_peersIssued;
    /** A set of servers that have completed current task. */
    QSet<QnUuid> m_peersComplete;
    /** A set of servers that have failed current task. */
    QSet<QnUuid> m_peersFailed;

    /**
     * Time for server to become online. We will remove this server from the task set after
     * this time expires.
     */
    UpdateItem::Clock::duration m_waitForServerReturn;
};

} // namespace nx::vms::client::desktop
