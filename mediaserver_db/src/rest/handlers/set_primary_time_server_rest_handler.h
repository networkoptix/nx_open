#pragma once

#include "rest/server/json_rest_handler.h"
#include <api/model/time_reply.h>

namespace nx { namespace time_sync { class TimeSyncManager; } }

namespace rest {
namespace handlers {

class SetPrimaryTimeServerRestHandler: public QnJsonRestHandler
{
public:
    SetPrimaryTimeServerRestHandler() {}
    virtual int executeGet(
        const QString &path, 
        const QnRequestParams &params, 
        QnJsonRestResult &result, 
        const QnRestConnectionProcessor*) override;
    
    static QnJsonRestResult execute(
        nx::time_sync::TimeSyncManager* timeSyncManager, const QnUuid& id);
};

} // namespace handlers
} // namespace rest
