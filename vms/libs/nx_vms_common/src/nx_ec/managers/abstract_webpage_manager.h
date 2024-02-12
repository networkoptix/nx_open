// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/webpage_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractWebPageNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::WebPageData& webpage, ec2::NotificationSource source);
    void removed(const nx::Uuid& id, ec2::NotificationSource source);
};

/*!
\note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractWebPageManager
{
public:
    virtual ~AbstractWebPageManager() = default;

    virtual int getWebPages(
        Handler<nx::vms::api::WebPageDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getWebPagesSync(nx::vms::api::WebPageDataList* outDataList);

    virtual int save(
        const nx::vms::api::WebPageData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::WebPageData& data);

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const nx::Uuid& id);
};

} // namespace ec2
