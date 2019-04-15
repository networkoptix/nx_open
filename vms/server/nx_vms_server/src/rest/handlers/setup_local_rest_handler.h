#pragma once

#include <nx/network/http/http_types.h>

#include <api/model/setup_local_system_data.h>
#include <nx/network/rest/handler.h>
#include <core/resource_access/user_access_data.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server {

class SetupLocalSystemRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    SetupLocalSystemRestHandler(QnMediaServerModule* serverModule);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value execute(
        SetupLocalSystemData data,
        const QnRestConnectionProcessor* owner,
        nx::network::rest::JsonResult& result);
};

} // namespace nx::vms::server
