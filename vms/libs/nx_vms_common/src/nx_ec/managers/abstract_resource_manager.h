// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/async_handler_executor.h>
#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>
#include <nx/vms/api/data/resource_data.h>

#include <QtCore/QObject>
#include <QtCore/QVector>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractResourceNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void statusChanged(
        const QnUuid& resourceId, nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source);

    void resourceParamChanged(
        const nx::vms::api::ResourceParamWithRefData& param,
        ec2::NotificationSource source);

    void resourceParamRemoved(const nx::vms::api::ResourceParamWithRefData& param);
    void resourceRemoved(const QnUuid& resourceId);
    void resourceStatusRemoved(const QnUuid& resourceId);
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
        const QnUuid& resourceId,
        nx::vms::api::ResourceStatus status,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode setResourceStatusSync(const QnUuid& resourceId, nx::vms::api::ResourceStatus status);

    virtual int getKvPairs(
        const QnUuid& resourceId,
        Handler<nx::vms::api::ResourceParamWithRefDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getKvPairsSync(
        const QnUuid& resourceId, nx::vms::api::ResourceParamWithRefDataList* outDataList);

    virtual int getStatusList(
        const QnUuid& resourceId,
        Handler<nx::vms::api::ResourceStatusDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getStatusListSync(
        const QnUuid& resourceId, nx::vms::api::ResourceStatusDataList* outDataList);

    virtual int save(
        const nx::vms::api::ResourceParamWithRefDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::ResourceParamWithRefDataList& dataList);

    int save(
        const QnUuid& resourceId,
        const nx::vms::api::ResourceParamDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {});

    ErrorCode saveSync(
        const QnUuid& resourceId, const nx::vms::api::ResourceParamDataList& dataList);

    virtual int remove(
        const QnUuid& resourceId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const QnUuid& resourceId);

    virtual int remove(
        const QVector<QnUuid>& resourceIds,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int remove(
        const nx::vms::api::ResourceParamWithRefData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int removeHardwareIdMapping(
        const QnUuid& resourceId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const QVector<QnUuid>& resourceIds);
    ErrorCode removeSync(const nx::vms::api::ResourceParamWithRefData& data);
    ErrorCode removeHardwareIdMappingSync(const QnUuid& resourceId);
};

} // namespace ec2
