// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/videowall_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractVideowallNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(
        const nx::vms::api::VideowallData& videowall, ec2::NotificationSource source);

    void removed(const nx::Uuid& id, ec2::NotificationSource source);
    void controlMessage(const nx::vms::api::VideowallControlMessageData& message);
};

/*!
\note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractVideowallManager
{
public:
    virtual ~AbstractVideowallManager() = default;

    virtual int getVideowalls(
        Handler<nx::vms::api::VideowallDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getVideowallsSync(nx::vms::api::VideowallDataList* outDataList);

    virtual int save(
        const nx::vms::api::VideowallData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::VideowallData& data);

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const nx::Uuid& id);

    virtual int sendControlMessage(
        const nx::vms::api::VideowallControlMessageData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode sendControlMessageSync(const nx::vms::api::VideowallControlMessageData& data);
};

} // namespace ec2
