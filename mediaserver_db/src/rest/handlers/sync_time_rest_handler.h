#pragma once

#include "rest/server/json_rest_handler.h"
#include <api/model/time_reply.h>

namespace nx { namespace time_sync { class TimeSyncManager; } }

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

    static SyncTimeData execute(nx::time_sync::TimeSyncManager* timeSyncManager);
};

} // namespace handlers
} // namespace rest
