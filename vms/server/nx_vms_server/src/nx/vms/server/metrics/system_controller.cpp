#include "system_controller.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <media_server/media_server_module.h>
#include <licensing/license.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>

namespace nx::vms::server::metrics {

SystemResourceController::SystemResourceController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<void*>("systems", makeProviders())
{
}

void SystemResourceController::start()
{
    // TODO: Monitor cloudSystemId and use it instead of localSystemId when avaliable.
    QObject::connect(
        globalSettings(), &QnGlobalSettings::localSystemIdChanged,
        [&]()
        {
            const auto newId = globalSettings()->localSystemId();
            auto& lastId = *m_lastId.lock();
            if (lastId)
            {
                if (*lastId == newId)
                    return; //< Nothing has changed!

                remove(*lastId);
            }

            lastId = newId;
            add(nullptr, newId, utils::metrics::Scope::system);
        });
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
                "servers",
                [pool](const auto&) { return Value(pool->getAllServers(Qn::AnyStatus).size()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cameras",
                [pool](const auto&)
                {
                    return Value(pool->getAllCameras(
                        QnResourcePtr(), /*ignoreDesktopCameras*/ true).size());
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "storages",
                [pool](const auto&) { return Value(pool->getResources<QnStorageResource>().size()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "users",
                [pool](const auto&)
                {
                    // Subtracting video wall user (see: addFakeVideowallUser),
                    // avoid filtering for performance.
                    return Value(pool->getResources<QnUserResource>().size() - 1);
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "version",
                [pool](const auto&)
                {
                    return Value(toString(pool->getOwnMediaServerOrThrow()->getVersion()));
                }
            )
        )
    );
}

} // namespace nx::vms::server::metrics
