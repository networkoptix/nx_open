// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/resource_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractResourceNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void statusChanged(
        const nx::Uuid& resourceId, nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source);

    void resourceParamChanged(
        const nx::vms::api::ResourceParamWithRefData& param,
        ec2::NotificationSource source);

    void resourceParamRemoved(const nx::vms::api::ResourceParamWithRefData& param);
    void resourceRemoved(const nx::Uuid& resourceId, ec2::NotificationSource source);
    void resourceStatusRemoved(const nx::Uuid& resourceId, ec2::NotificationSource source);
};

/*!
    \note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractResourceManager
{
public:
    virtual ~AbstractResourceManager() = default;

    virtual int getResourceTypes(
        Handler<QnResourceTypeList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getResourceTypesSync(QnResourceTypeList* outTypeList);

    virtual int setResourceStatus(
        const nx::Uuid& resourceId,
        nx::vms::api::ResourceStatus status,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode setResourceStatusSync(const nx::Uuid& resourceId, nx::vms::api::ResourceStatus status);

    virtual int getKvPairs(
        const nx::Uuid& resourceId,
        Handler<nx::vms::api::ResourceParamWithRefDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getKvPairsSync(
        const nx::Uuid& resourceId, nx::vms::api::ResourceParamWithRefDataList* outDataList);

    virtual int getStatusList(
        const nx::Uuid& resourceId,
        Handler<nx::vms::api::ResourceStatusDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getStatusListSync(
        const nx::Uuid& resourceId, nx::vms::api::ResourceStatusDataList* outDataList);

    virtual int save(
        const nx::vms::api::ResourceParamWithRefDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::ResourceParamWithRefDataList& dataList);

    int save(
        const nx::Uuid& resourceId,
        const nx::vms::api::ResourceParamDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {});

    ErrorCode saveSync(
        const nx::Uuid& resourceId, const nx::vms::api::ResourceParamDataList& dataList);

    virtual int remove(
        const nx::Uuid& resourceId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const nx::Uuid& resourceId);

    virtual int remove(
        const QVector<nx::Uuid>& resourceIds,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int remove(
        const nx::vms::api::ResourceParamWithRefData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int removeHardwareIdMapping(
        const nx::Uuid& resourceId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const QVector<nx::Uuid>& resourceIds);
    ErrorCode removeSync(const nx::vms::api::ResourceParamWithRefData& data);
    ErrorCode removeHardwareIdMappingSync(const nx::Uuid& resourceId);
};

} // namespace ec2
