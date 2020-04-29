#pragma once

#include <array>

#include <api/model/api_model_fwd.h>
#include <client_core/connection_context_aware.h>
#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_fwd.h>
#include <server/server_storage_manager_fwd.h>
#include <utils/common/connective.h>

#include <nx/utils/singleton.h>

/**
 * Client-side class to monitor server-related storages state: rebuild and backup process.
 */
class QnServerStorageManager:
    public Connective<QObject>,
    public Singleton<QnServerStorageManager>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit QnServerStorageManager(QObject* parent = nullptr);
    virtual ~QnServerStorageManager() override;

    QSet<QString> protocols(const QnMediaServerResourcePtr& server) const;
    QnStorageScanData rebuildStatus(
        const QnMediaServerResourcePtr& server, QnServerStoragesPool pool) const;
    QnBackupStatusData backupStatus(const QnMediaServerResourcePtr& server) const;

    bool rebuildServerStorages(const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);
    bool cancelServerStoragesRebuild(
        const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);

    bool backupServerStorages(const QnMediaServerResourcePtr& server);
    bool cancelBackupServerStorages(const QnMediaServerResourcePtr& server);

    /**
     * Force check all storage status data (space, rebuild, backup).
     */
    void checkStoragesStatus(const QnMediaServerResourcePtr& server);

    void saveStorages(const QnStorageResourceList& storages);
    void deleteStorages(const nx::vms::api::IdDataList& ids);

    QnStorageResourcePtr activeMetadataStorage(const QnMediaServerResourcePtr& server) const;

signals:
    void serverProtocolsChanged(
        const QnMediaServerResourcePtr& server, const QSet<QString>& protocols);

    void serverRebuildStatusChanged(const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool, const QnStorageScanData& status);

    void serverBackupStatusChanged(
        const QnMediaServerResourcePtr& server, const QnBackupStatusData& status);

    void serverRebuildArchiveFinished(
        const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);

    void serverBackupFinished(const QnMediaServerResourcePtr& server);

    void storageAdded(const QnStorageResourcePtr& storage);
    void storageChanged(const QnStorageResourcePtr& storage);
    void storageRemoved(const QnStorageResourcePtr& storage);

    // For convenience this signal is also sent when the server goes online or offline or is removed.
    void activeMetadataStorageChanged(const QnMediaServerResourcePtr& server);

private:
    void invalidateRequests();

    bool isServerValid(const QnMediaServerResourcePtr& server) const;

    bool sendArchiveRebuildRequest(const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool, Qn::RebuildAction action = Qn::RebuildAction_ShowProgress);

    bool sendBackupRequest(const QnMediaServerResourcePtr& server,
        Qn::BackupAction action = Qn::BackupAction_ShowProgress);

    bool sendStorageSpaceRequest(const QnMediaServerResourcePtr& server);

    void checkStoragesStatusInternal(const QnResourcePtr& resource);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    QnStorageResourcePtr calculateActiveMetadataStorage(
        const QnMediaServerResourcePtr& server) const;

    void setActiveMetadataStorage(const QnMediaServerResourcePtr& server,
        const QnStorageResourcePtr& storage, bool suppressNotificationSignal);

    void updateActiveMetadataStorage(const QnMediaServerResourcePtr& server,
        bool suppressNotificationSignal = false);

private slots:
    void at_archiveRebuildReply(int status, const QnStorageScanData& reply, int handle);
    void at_backupStatusReply(int status, const QnBackupStatusData& reply, int handle);
    void at_storageSpaceReply(int status, const QnStorageSpaceReply& reply, int handle);

private:
    struct ServerInfo;

    struct RequestKey
    {
        QnMediaServerResourcePtr server;
        QnServerStoragesPool pool = QnServerStoragesPool::Main;

        RequestKey() = default;
        RequestKey(const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);
    };

    QHash<QnMediaServerResourcePtr, ServerInfo> m_serverInfo;
    QHash<int, RequestKey> m_requests;

    QHash<QnMediaServerResourcePtr, QnStorageResourcePtr> m_activeMetadataStorages;
};

#define qnServerStorageManager QnServerStorageManager::instance()
