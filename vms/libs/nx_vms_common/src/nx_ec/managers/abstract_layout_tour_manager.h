// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/api/data/layout_tour_data.h>

#include <QtCore/QObject>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractLayoutTourNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::LayoutTourData& tour, ec2::NotificationSource source);
    void removed(const QnUuid& id);
};

/*!
\note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractLayoutTourManager
{
public:
    virtual ~AbstractLayoutTourManager() = default;

    virtual int getLayoutTours(
        Handler<nx::vms::api::LayoutTourDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getLayoutToursSync(nx::vms::api::LayoutTourDataList* outDataList);

    virtual int save(
        const nx::vms::api::LayoutTourData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::LayoutTourData& data);

    virtual int remove(
        const QnUuid& tourId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const QnUuid& tourId);
};

} // namespace ec2
