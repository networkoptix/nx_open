#pragma once

#include <api/model/time_reply.h>
#include <nx/network/rest/handler.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

class QnCommonModule;

namespace rest {
namespace handlers {

class SetPrimaryTimeServerRestHandler: public nx::network::rest::Handler
{
protected:
    nx::network::rest::Response executePost(const nx::network::rest::Request& request) override;
};

} // namespace handlers
} // namespace rest
