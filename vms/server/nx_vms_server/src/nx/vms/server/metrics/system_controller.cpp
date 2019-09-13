#include "system_controller.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <media_server/media_server_module.h>
#include <licensing/license.h>

namespace nx::vms::server::metrics {

namespace {

class SystemDescription: public utils::metrics::ResourceDescription<void*>
{
public:
    SystemDescription(QnGlobalSettings* s): ResourceDescription(nullptr), m_settings(s) {}

    QString id() const override
    {
        const auto id = m_settings->cloudSystemId();
        return (!id.isNull() ? id : m_settings->localSystemId().toSimpleString());
    }

    utils::metrics::Scope scope() const override
    {
        return utils::metrics::Scope::system;
    }

private:
    QnGlobalSettings* m_settings;
};

} // namespace

SystemResourceController::SystemResourceController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<void*>("systems", makeProviders())
{
}

void SystemResourceController::start()
{
    add(std::make_unique<SystemDescription>(globalSettings()));
}

utils::metrics::ValueGroupProviders<SystemResourceController::Resource>
    SystemResourceController::makeProviders()
{
    const auto pool = serverModule()->commonModule()->resourcePool();
    const auto settings = globalSettings();

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [settings](const auto&) { return Value(settings->systemName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "servers", [pool](const auto&) { return Value(pool->getAllServers(Qn::AnyStatus).size()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "users", [pool](const auto&) { return Value(pool->getResources<QnUserResource>().size()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cameras", [pool](const auto&) { return Value(pool->getAllCameras().size()); }
            )
        )
    );
}

} // namespace nx::vms::server::metrics
