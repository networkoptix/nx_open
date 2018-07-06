#pragma once

#include "rest/server/json_rest_handler.h"
#include <api/model/time_reply.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

namespace rest {
namespace handlers {

class SyncTimeRestHandler: public QnJsonRestHandler
{
public:
    SyncTimeRestHandler() {}
    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;

    static SyncTimeData execute(nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager);
};

} // namespace handlers
} // namespace rest
