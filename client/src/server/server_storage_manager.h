#pragma once

#include <array>

#include <api/model/api_model_fwd.h>

#include <core/resource/resource_fwd.h>

#include <server/server_storage_manager_fwd.h>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>

/** Client-side class to monitor server-related storages state: rebuild and backup process. */
class QnServerStorageManager: public Connective<QObject>, public Singleton<QnServerStorageManager> {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnServerStorageManager(QObject *parent = nullptr);
    virtual ~QnServerStorageManager();

    QSet<QString> protocols(const QnMediaServerResourcePtr &server) const;
    QnStorageSpaceDataList storages(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool) const;
    QnStorageScanData rebuildStatus(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool) const;
    QnBackupStatusData backupStatus(const QnMediaServerResourcePtr &server) const;

    bool rebuildServerStorages(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);
    bool cancelServerStoragesRebuild(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);

    bool backupServerStorages(const QnMediaServerResourcePtr &server);
    bool cancelBackupServerStorages(const QnMediaServerResourcePtr &server);

signals:
    void serverProtocolsChanged(const QnMediaServerResourcePtr &server, const QSet<QString> &protocols);
    void serverStorageSpaceChanged(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageSpaceDataList &storages);
    void serverRebuildStatusChanged(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageScanData &status);
    void serverBackupStatusChanged(const QnMediaServerResourcePtr &server, const QnBackupStatusData &status);

    void serverRebuildArchiveFinished(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);
    void serverBackupFinished(const QnMediaServerResourcePtr &server);

private:
    bool isServerValid(const QnMediaServerResourcePtr &server) const;

    bool sendArchiveRebuildRequest(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, Qn::RebuildAction action = Qn::RebuildAction_ShowProgress);
    bool sendBackupRequest(const QnMediaServerResourcePtr &server, Qn::BackupAction action = Qn::BackupAction_ShowProgress);
    bool sendStorageSpaceRequest(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool);

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
