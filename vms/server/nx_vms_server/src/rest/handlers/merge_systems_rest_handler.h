#pragma once

#include <api/model/configure_system_data.h>
#include <nx/network/rest/handler.h>
#include <core/resource/resource_fwd.h>
#include <utils/merge_systems_common.h>

#include <nx/vms/api/data_fwd.h>
#include <nx/vms/server/server_module_aware.h>

struct MergeSystemData;
namespace ec2 { class AbstractTransactionMessageBus; }
class QnCommonModule;

namespace nx::vms::server {

class MergeSystemsRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    MergeSystemsRestHandler(QnMediaServerModule* serverModule);

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value execute(
        MergeSystemData data,
        const QnRestConnectionProcessor* owner,
        nx::network::rest::JsonResult& result);

private:
    ec2::AbstractTransactionMessageBus* m_messageBus;

    void updateLocalServerAuthKeyInConfig(
        QnCommonModule* commonModule);

    void initiateConnectionToRemoteServer(
        QnCommonModule* commonModule,
        const QUrl& remoteModuleUrl,
        const nx::vms::api::ModuleInformationWithAddresses& remoteModuleInformation);
};

} // namespace nx::vms::server
