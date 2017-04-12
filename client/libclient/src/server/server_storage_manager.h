#pragma once

#include <array>

#include <api/model/api_model_fwd.h>

#include <client_core/connection_context_aware.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <server/server_storage_manager_fwd.h>

#include <nx/utils/singleton.h>
#include <utils/common/connective.h>

/** Client-side class to monitor server-related storages state: rebuild and backup process. */
class QnServerStorageManager:
    public Connective<QObject>,
    public Singleton<QnServerStorageManager>,
    public QnConnectionContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnServerStorageManager(QObject *parent = nullptr);
    virtual ~QnServerStorageManager();

    QSet<QString> protocols(const QnMediaServerResourcePtr &server) const;
    QnStorageScanData rebuildStatus(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool) const;
    QnBackupStatusData backupStatus(const QnMediaServerResourcePtr &server) const;

    bool rebuildServerStorages(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);
    bool cancelServerStoragesRebuild(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);

    bool backupServerStorages(const QnMediaServerResourcePtr &server);
    bool cancelBackupServerStorages(const QnMediaServerResourcePtr &server);

    /** Force check all storage status data (space, rebuild, backup) */
    void checkStoragesStatus(const QnMediaServerResourcePtr &server);

    void saveStorages(const QnStorageResourceList &storages);
    void deleteStorages(const ec2::ApiIdDataList &ids);
signals:
    void serverProtocolsChanged(const QnMediaServerResourcePtr &server, const QSet<QString> &protocols);
    void serverRebuildStatusChanged(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageScanData &status);
    void serverBackupStatusChanged(const QnMediaServerResourcePtr &server, const QnBackupStatusData &status);

    void serverRebuildArchiveFinished(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);
    void serverBackupFinished(const QnMediaServerResourcePtr &server);

    void storageAdded(const QnStorageResourcePtr &storage);
    void storageChanged(const QnStorageResourcePtr &storage);
    void storageRemoved(const QnStorageResourcePtr &storage);

private:
    void invalidateRequests();

    bool isServerValid(const QnMediaServerResourcePtr &server) const;

    bool sendArchiveRebuildRequest(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, Qn::RebuildAction action = Qn::RebuildAction_ShowProgress);
    bool sendBackupRequest(const QnMediaServerResourcePtr &server, Qn::BackupAction action = Qn::BackupAction_ShowProgress);
    bool sendStorageSpaceRequest(const QnMediaServerResourcePtr &server);

private slots:
    void at_archiveRebuildReply (int status, const QnStorageScanData    &reply, int handle);
    void at_backupStatusReply   (int status, const QnBackupStatusData   &reply, int handle);
    void at_storageSpaceReply   (int status, const QnStorageSpaceReply  &reply, int handle);



private:
    struct ServerInfo;

    struct RequestKey {
        QnMediaServerResourcePtr server;
        QnServerStoragesPool pool;

        RequestKey();
        RequestKey(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);
    };

    QHash<QnMediaServerResourcePtr, ServerInfo> m_serverInfo;
    QHash<int, RequestKey> m_requests;
};

#define qnServerStorageManager QnServerStorageManager::instance()
