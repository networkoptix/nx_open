#pragma once

#include <nx/network/rest/handler.h>
#include <core/resource_access/user_access_data.h>
#include <nx/vms/server/server_module_aware.h>

struct PasswordData;
struct CurrentPasswordData;
namespace nx::vms::cloud_integration { class CloudConnectionManager; }
namespace ec2 { class AbstractTransactionMessageBus; }

namespace nx::vms::server {

class DetachFromSystemRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    DetachFromSystemRestHandler(
        QnMediaServerModule* serverModule,
        nx::vms::cloud_integration::CloudConnectionManager* const cloudConnectionManager,
        ec2::AbstractTransactionMessageBus* messageBus);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value execute(
        CurrentPasswordData passwordData,
        const QnRestConnectionProcessor* owner,
        nx::network::rest::JsonResult& result);

private:
    nx::vms::cloud_integration::CloudConnectionManager* const m_cloudConnectionManager;
    ec2::AbstractTransactionMessageBus* m_messageBus;
};

} // namespace nx::vms::server
