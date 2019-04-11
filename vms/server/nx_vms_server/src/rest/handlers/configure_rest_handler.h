#pragma once

#include <core/resource_access/user_access_data.h>
#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace ec2 { class AbstractTransactionMessageBus; }
struct ConfigureSystemData;

namespace nx::vms::server {

class ConfigureRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    ConfigureRestHandler(QnMediaServerModule* serverModule);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value execute(
        const ConfigureSystemData& data,
        nx::network::rest::JsonResult& result,
        const QnRestConnectionProcessor* owner);

    int changePort(const QnRestConnectionProcessor* owner, int port);
};

} // namespace nx::vms::server
