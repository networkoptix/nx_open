// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <memory>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/common/update/update_information.h>

#include "client_update_tool.h"

struct QnUpdateFreeSpaceReply;

namespace nx::vms::client::desktop {

using StatusCode = nx::vms::common::update::Status::Code;

/**
 * This structure keeps all the information necessary to display current state of the server.
 */
struct NX_VMS_CLIENT_DESKTOP_API UpdateItem
{
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using ErrorCode = nx::vms::common::update::Status::ErrorCode;

    enum class Component
    {
        server,
        client,
    };

    QnUuid id;
    StatusCode state = StatusCode::offline;
    int progress = -1;

    /**
     * Message generated from nx::vms::common::update::Status::message.
     * It is displayed only in debug mode.
     */
    QString debugMessage;

    QString statusMessage; /**< Message generated from nx::vms::common::update::Status::errorCode. */
    ErrorCode errorCode = ErrorCode::noError;

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
    /** Protocol version for this peer. */
    int protocol = 0;
    bool installing = false;
    bool storeUpdates = true;
    bool hasInternet = false;
    /**
     * Actual status can become unknown when we just issue update command. We should wait for
     * another response from /ec2/updateStatus to get relevant state.
     */
    bool statusUnknown = false;
    /** The last time we've seen this peer online. */
    TimePoint lastOnlineTime;
    /** The last time we've got status update for this item. */
    TimePoint lastStatusTime;
    /** Row in the table. */
    int row = -1;

    int freeSpace = -1;
    int requiredSpace = -1;

    bool isServer() const;
    bool isClient() const;
};

using UpdateItemPtr = std::shared_ptr<UpdateItem>;

/**
 * PeerStateTracker integrates keeps state for all the peers participating in system update.
 *
 * ServerUpdatesModel uses it as a data source, by subscribing to itemChanged/itemAdded/... events.
 * ServerUpdateTool passes mediaserver responses here.
 */
class NX_VMS_CLIENT_DESKTOP_API PeerStateTracker:
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    PeerStateTracker(QObject* parent = nullptr);

    using ServerFilter = std::function<bool (const QnMediaServerResourcePtr&)>;
    void setServerFilter(ServerFilter filter);

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

    QnUuid getClientPeerId(nx::vms::common::SystemContext* context) const;

    nx::utils::SoftwareVersion lowestInstalledVersion();
    void setUpdateTarget(const nx::utils::SoftwareVersion& version);
    /**
     * Update internal data using response from mediaservers.
     * It will return number if peers with changed data.
     */
    int setUpdateStatus(const std::map<QnUuid, nx::vms::common::update::Status>& statusAll);

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

    /** Checks if any peer has verification error. */
    bool hasVerificationErrors() const;

    bool hasStatusErrors() const;

    struct ErrorReport
    {
        QString message;
        QSet<QnUuid> peers;
    };

    /**
     * Generates an error report for multiple peers.
     * This error will be displayed in error dialog from multi_server_updates_widget
     */
    bool getErrorReport(ErrorReport& report) const;

public:
    std::map<QnUuid, nx::vms::common::update::Status::Code> allPeerStates() const;
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers() const;

    QList<UpdateItemPtr> allItems() const;
    QSet<QnUuid> allPeers() const;
    QSet<QnUuid> peersInState(StatusCode state, bool withClient = true) const;
    QSet<QnUuid> legacyServers() const;
    QSet<QnUuid> offlineServers() const;
    QSet<QnUuid> offlineAndInState(StatusCode state) const;
    QSet<QnUuid> onlineAndInState(StatusCode state) const;
    QSet<QnUuid> peersInstalling() const;
    QSet<QnUuid> peersCompleteInstall() const;
    QSet<QnUuid> serversWithChangedProtocol() const;
    QSet<QnUuid> peersWithUnknownStatus() const;
    QSet<QnUuid> peersWithDownloaderError() const;

    /**
     * Process unknown or offline states. It will change item states.
     */
    void processUnknownStates();

    /**
     * Processing for task sets. These functions are called every 1sec from
     * MultiServerUpdatesWidget.
     * These functions should only affect task sets and do not change state for each item.
     */
    void processDownloadTaskSet();
    void processReadyInstallTaskSet();
    void processInstallTaskSet();

    void skipFailedPeers();

    /**
     * Getters for task sets.
     * Task sets are relevant to current update action, like 'downloading' or 'installing'.
     */

    /** Get a set of peers that completed current action. */
    QSet<QnUuid> peersComplete() const;
    /** Get a set of peers that are currently active. */
    QSet<QnUuid> peersActive() const;
    /** Get a set of peers that are participating in current action. */
    QSet<QnUuid> peersIssued() const;
    /** Get a set of peers that have failed current action. */
    QSet<QnUuid> peersFailed() const;

    /**
     * We call it every time we start or stop next task, like ready->downloading,
     * readyToInstall->installing or when we cancel current action.
     * It will reset all internal task sets.
     */
    void setTask(const QSet<QnUuid>& targets, bool reset = true);
    void setTaskError(const QSet<QnUuid>& targets, const QString& error);
    void addToTask(QnUuid id);
    void removeFromTask(QnUuid id);

    static QString errorString(nx::vms::common::update::Status::ErrorCode code);

public:
    /**
     * We connect it to ClientUpdateTool::updateStateChanged and turn it to a proper
     * peer state inside our task sets.
     */
    void atClientUpdateStateChanged(
        ClientUpdateTool::State state,
        int percentComplete,
        const ClientUpdateTool::Error& error);

signals:
    /**
     * Called after UpdateItem is already added to the list.
     */
    void itemAdded(UpdateItemPtr item);
    void itemChanged(UpdateItemPtr item);
    void itemOnlineStatusChanged(UpdateItemPtr item);

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
    UpdateItemPtr addItemForClient(nx::vms::common::SystemContext* context);
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

    mutable nx::Mutex m_dataLock;

    /**
     * This sets are changed every time we are initiating some update action.
     * Set of peers that are currently active.
     */
    QSet<QnUuid> m_peersActive;
    /** Set of peers that are used for the current task. */
    QSet<QnUuid> m_peersIssued;
    /** A set of peers that have completed current task. */
    QSet<QnUuid> m_peersComplete;
    /** A set of peers that have failed current task. */
    QSet<QnUuid> m_peersFailed;

    /**
     * Time for server to become online. We will remove this server from the task set after
     * this time expires.
     */
    UpdateItem::Clock::duration m_timeForServerToReturn;

    QPointer<QnResourcePool> m_resourcePool;

    ServerFilter m_serverFilter;
};

} // namespace nx::vms::client::desktop
