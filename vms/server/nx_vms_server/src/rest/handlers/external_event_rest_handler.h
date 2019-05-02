#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server {

class ExternalEventRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    ExternalEventRestHandler(QnMediaServerModule* serverModule);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;
};

} // namespace nx::vms::server
