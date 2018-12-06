#pragma once

#include "rest/server/json_rest_handler.h"
#include <api/model/time_reply.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

class QnCommonModule;

namespace rest {
namespace handlers {

class SetPrimaryTimeServerRestHandler: public QnJsonRestHandler
{
public:
    virtual int executePost(
        const QString& /*path*/,
        const QnRequestParams& /*params*/,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    static QnJsonRestResult execute(
        nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager,
        QnCommonModule* commonModule,
        const QnUuid& id);
};

} // namespace handlers
} // namespace rest
