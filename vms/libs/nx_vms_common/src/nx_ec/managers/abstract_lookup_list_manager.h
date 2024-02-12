// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/lookup_list_data.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractLookupListNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::LookupListData& data, ec2::NotificationSource source);
    void removed(const nx::Uuid& id);
};

/*!
\note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractLookupListManager
{
public:
    virtual ~AbstractLookupListManager() = default;

    virtual int getLookupLists(
        Handler<nx::vms::api::LookupListDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result getLookupListsSync(nx::vms::api::LookupListDataList* outDataList);

    virtual int save(
        const nx::vms::api::LookupListData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result saveSync(const nx::vms::api::LookupListData& data);

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result removeSync(const nx::Uuid& id);
};

} // namespace ec2
