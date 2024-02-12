// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/id_data.h>
#include <nx/vms/api/data/media_server_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractMediaServerNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::MediaServerData& server, ec2::NotificationSource source);
    void storageChanged(const nx::vms::api::StorageData& storage, ec2::NotificationSource source);
    void removed(const nx::Uuid& id, ec2::NotificationSource source);
    void storageRemoved(const nx::Uuid& id, ec2::NotificationSource source);
    void userAttributesChanged(const nx::vms::api::MediaServerUserAttributesData& attributes);
    void userAttributesRemoved(const nx::Uuid& id);
};

class NX_VMS_COMMON_API AbstractMediaServerManager
{
public:
    virtual ~AbstractMediaServerManager() = default;

    virtual int getServers(
        Handler<nx::vms::api::MediaServerDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getServersSync(nx::vms::api::MediaServerDataList* outDataList);

    virtual int getServersEx(
        Handler<nx::vms::api::MediaServerDataExList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getServersExSync(nx::vms::api::MediaServerDataExList* outDataList);

    virtual int save(
        const nx::vms::api::MediaServerData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result saveSync(const nx::vms::api::MediaServerData& data);

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const nx::Uuid& id);

    virtual int saveUserAttributes(
        const nx::vms::api::MediaServerUserAttributesDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveUserAttributesSync(
        const nx::vms::api::MediaServerUserAttributesDataList& dataList);

    virtual int saveStorages(
        const nx::vms::api::StorageDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveStoragesSync(const nx::vms::api::StorageDataList& dataList);

    virtual int removeStorages(
        const nx::vms::api::IdDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeStoragesSync(const nx::vms::api::IdDataList& dataList);

    /*!
    \param mediaServerId if not NULL, returned list contains at most one element: the one
    corresponding to \a mediaServerId. If NULL, returned list contains data of all known servers.
    */
    virtual int getUserAttributes(
        const nx::Uuid& mediaServerId,
        Handler<nx::vms::api::MediaServerUserAttributesDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    /*!
    \param mediaServerId if not NULL, returned list contains at most one element: the one
    corresponding to \a mediaServerId. If NULL, returned list contains data of all known servers.
    */
    Result getUserAttributesSync(
        const nx::Uuid& mediaServerId, nx::vms::api::MediaServerUserAttributesDataList* outDataList);

    /*!
    \param mediaServerId if not NULL, returned list contains at most one element: the one
    corresponding to \a mediaServerId. If NULL, returned list contains data of all known servers.
    */
    virtual int getStorages(
        const nx::Uuid& mediaServerId,
        Handler<nx::vms::api::StorageDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    /*!
    \param mediaServerId if not NULL, returned list contains at most one element: the one
    corresponding to \a mediaServerId. If NULL, returned list contains data of all known servers.
    */
    ErrorCode getStoragesSync(
        const nx::Uuid& mediaServerId, nx::vms::api::StorageDataList* outDataList);
};

} // namespace ec2
