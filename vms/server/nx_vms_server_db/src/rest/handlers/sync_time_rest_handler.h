#pragma once

#include <api/model/time_reply.h>
#include <nx/network/rest/handler.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

namespace rest {
namespace handlers {

class SyncTimeRestHandler: public nx::network::rest::Handler
{
protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

public:
    static SyncTimeData execute(nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager);
};

} // namespace handlers
} // namespace rest
