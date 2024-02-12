// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/layout_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractLayoutNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::LayoutData& layout, ec2::NotificationSource source);
    void removed(const nx::Uuid& id, ec2::NotificationSource source);
};

/*!
\note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractLayoutManager
{
public:
    virtual ~AbstractLayoutManager() = default;

    virtual int getLayouts(
        Handler<nx::vms::api::LayoutDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getLayoutsSync(nx::vms::api::LayoutDataList* outDataList);

    virtual int save(
        const nx::vms::api::LayoutData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::LayoutData& data);

    virtual int remove(
        const nx::Uuid& layoutId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const nx::Uuid& layoutId);
};

} // namespace ec2
