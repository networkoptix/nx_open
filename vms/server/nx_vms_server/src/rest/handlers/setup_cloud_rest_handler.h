#pragma once

#include <api/model/setup_cloud_system_data.h>
#include <core/resource_access/user_access_data.h>
#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx { namespace vms { namespace cloud_integration { class CloudManagerGroup; } } }

namespace nx::vms::server {

class SetupCloudSystemRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    SetupCloudSystemRestHandler(
        QnMediaServerModule* serverModule,
        cloud_integration::CloudManagerGroup* cloudManagerGroup);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value execute(
        SetupCloudSystemData data,
        const QnRestConnectionProcessor* owner,
        nx::network::rest::JsonResult& result);

private:
    cloud_integration::CloudManagerGroup* m_cloudManagerGroup;
};

} // namespace nx::vms::server
