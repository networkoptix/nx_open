#pragma once

#include <memory> // for shared_ptr

#include <QtCore/QAbstractTableModel>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/software_version.h>
#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include <update/updates_common.h>
#include <nx/update/common_update_manager.h>

namespace nx::vms::client::desktop {

using StatusCode = nx::update::Status::Code;

/**
 * This structure keeps all the information necessary to display current state of the server.
 */
struct UpdateItem
{
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
    /** Flag for servers, that can be updated using legacy 3.2 system. */
    bool onlyLegacyUpdate = false;
    bool legacyUpdateUsed = false;
    /** Client is uploading files to this server. */
    bool uploading = false;
    bool offline = false;
    bool skipped = false;
    bool installed = false;
    bool changedProtocol = false;
    bool installing = false;
    bool storeUpdates = true;
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
    QSet<QnUuid> getServersInState(StatusCode state) const;
    QSet<QnUuid> getLegacyServers() const;
    QSet<QnUuid> getOfflineServers() const;
    QSet<QnUuid> getPeersInstalling() const;
    QSet<QnUuid> getPeersCompleteInstall() const;
    QSet<QnUuid> getServersWithChangedProtocol() const;

public:
    /**
     * We connect it to ClientUpdateTool::updateStateChanged and turn it to a proper
     * peer state inside our task sets.
     */
    void atClientupdateStateChanged(int state, int percentComplete);

signals:
    void itemAdded(UpdateItemPtr item);
    void itemChanged(UpdateItemPtr item);
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
};

} // namespace nx::vms::client::desktop
