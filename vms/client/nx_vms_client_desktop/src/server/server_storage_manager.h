// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <optional>

#include <api/model/api_model_fwd.h>
#include <api/model/recording_stats_reply.h>
#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/id_data.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <server/server_storage_manager_fwd.h>

struct QnStorageStatusReply;

/**
 * Client-side class to monitor server-related storages state: storages structure, space, roles and
 *     rebuild process.
 */
class QnServerStorageManager:
    public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QnServerStorageManager(
        nx::vms::client::desktop::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~QnServerStorageManager() override;

    /**
     * Temporary method to access storage manager from the current system context.
     */
    static QnServerStorageManager* instance();

    QSet<QString> protocols(const QnMediaServerResourcePtr& server) const;
    nx::vms::api::StorageScanInfo rebuildStatus(
        const QnMediaServerResourcePtr& server, QnServerStoragesPool pool) const;

    bool rebuildServerStorages(const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);
    bool cancelServerStoragesRebuild(
        const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);

    // TODO: #vbreus Method name doesn't reflect actual action, should be fixed.
    /**
     * This method updates active metadata storage and sends storages archive rebuild progress
     * requests for given server.
     */
    void checkStoragesStatus(const QnMediaServerResourcePtr& server);

    void saveStorages(const QnStorageResourceList& storages);
    void deleteStorages(const nx::vms::api::IdDataList& ids);

    QnStorageResourcePtr activeMetadataStorage(const QnMediaServerResourcePtr& server) const;

    /**
     * Requests storage space from mediaserver.
     * Response data can be retrieved from signal `storageSpaceRecieved`
     * @returns request handle. It is zero if something goes wrong.
     */
    int requestStorageSpace(const QnMediaServerResourcePtr& server);

    int requestStorageStatus(
        const QnMediaServerResourcePtr& server,
        const QString& storageUrl,
        std::function<void(const QnStorageStatusReply&)> callback);

    int requestRecordingStatistics(const QnMediaServerResourcePtr& server,
        qint64 bitrateAnalyzePeriodMs,
        std::function<void (bool, int, const QnRecordingStatsReply&)> callback);

signals:
    void serverProtocolsChanged(
        const QnMediaServerResourcePtr& server, const QSet<QString>& protocols);

    void serverRebuildStatusChanged(const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool, const nx::vms::api::StorageScanInfo& status);

    void serverRebuildArchiveFinished(
        const QnMediaServerResourcePtr& server, QnServerStoragesPool pool);

    void storageAdded(const QnStorageResourcePtr& storage);
    void storageChanged(const QnStorageResourcePtr& storage);
    void storageRemoved(const QnStorageResourcePtr& storage);

    // For convenience this signal is also sent when the server goes online or offline or is removed.
    void activeMetadataStorageChanged(const QnMediaServerResourcePtr& server);

    void storageSpaceRecieved(QnMediaServerResourcePtr server,
        bool success, int handle, const QnStorageSpaceReply& reply);

private:
    void invalidateRequests();
    bool isServerValid(const QnMediaServerResourcePtr& server) const;

    bool sendArchiveRebuildRequest(const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool, Qn::RebuildAction action = Qn::RebuildAction_ShowProgress);

    void checkStoragesStatusInternal(const QnResourcePtr& resource);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    QnStorageResourcePtr calculateActiveMetadataStorage(
        const QnMediaServerResourcePtr& server) const;

    void setActiveMetadataStorage(const QnMediaServerResourcePtr& server,
        const QnStorageResourcePtr& storage, bool suppressNotificationSignal);

    void updateActiveMetadataStorage(const QnMediaServerResourcePtr& server,
        bool suppressNotificationSignal = false);

private:
    void at_archiveRebuildReply(bool success, int handle, const nx::vms::api::StorageScanInfo& reply);
    void at_storageSpaceReply(bool success, int handle, const QnStorageSpaceReply& reply);

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
