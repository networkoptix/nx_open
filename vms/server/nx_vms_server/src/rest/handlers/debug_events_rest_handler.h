#pragma once

#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server {

class DebugEventsRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    DebugEventsRestHandler(QnMediaServerModule* serverModule);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value testNetworkIssue(
        const nx::network::rest::Params& params,
        nx::network::rest::Result& result,
        const QnRestConnectionProcessor* owner);

    nx::network::http::StatusCode::Value testCameraDisconnected(
        const nx::network::rest::Params& params,
        nx::network::rest::Result& result,
        const QnRestConnectionProcessor* owner);

};

} // namespace nx::vms::server
