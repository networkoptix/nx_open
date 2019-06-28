#pragma once

#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server {

class BackupControlRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    BackupControlRestHandler(QnMediaServerModule* serverModule);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;
    nx::network::rest::Response executePost(const nx::network::rest::Request& request) override;

private:
    int execute(const QString& command);
};

} // namespace nx::vms::server
