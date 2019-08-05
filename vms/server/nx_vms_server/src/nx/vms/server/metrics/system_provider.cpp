#include "system_provider.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <media_server/media_server_module.h>
#include <licensing/license.h>

namespace nx::vms::server::metrics {

SystemProvider::SystemProvider(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceProvider<void*>(makeProviders())
{
}

void SystemProvider::startMonitoring()
{
    found(nullptr);
}

std::optional<utils::metrics::ResourceDescription> SystemProvider::describe(const Resource&) const
{
    QnUuid systemId(globalSettings()->cloudSystemId());
    if (systemId.isNull())
        systemId = globalSettings()->localSystemId();

    return utils::metrics::ResourceDescription(
        systemId.toSimpleString(),
        QnUuid().toSimpleString(),
        globalSettings()->systemName(),
        /*isLocal*/ false); //< System resource is not local for any server.
}

SystemProvider::ParameterProviders SystemProvider::makeProviders()
{
    const auto pool = serverModule()->commonModule()->resourcePool();
    return parameterProviders(
        singleParameterProvider(
            {"servers"},
            [pool](const auto&) { return Value(pool->getAllServers(Qn::AnyStatus).size()); }
        ),
        singleParameterProvider(
            {"offlineServers", "offline servers"},
            [pool](const auto&) { return Value(pool->getAllServers(Qn::Offline).size()); }
        ),
        singleParameterProvider(
            {"offlineServerEvents", "offline server events"},
            [pool](const auto&) { return Value(0); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"users"},
            [pool](const auto&) { return Value(pool->getResources<QnUserResource>().size()); }
        ),
        singleParameterProvider(
            {"cameras"},
            [pool](const auto&) { return Value(pool->getAllCameras().size()); }
        ),
        singleParameterProvider(
            {"encoders"},
            [pool](const auto&) { return Value(0); } //< TODO: Implement.
        ),
        makeLicensingProvider()
    );
}

SystemProvider::ParameterProviderPtr SystemProvider::makeLicensingProvider()
{
    ParameterProviders typeProviders;
    for (int i = 0; i < static_cast<int>(Qn::LicenseType::LC_Count); ++i)
    {
        const auto type = static_cast<Qn::LicenseType>(i);
        const auto displayName = QnLicense::displayName(type);

        auto diplayParts = displayName.split(" ");
        if (!diplayParts.isEmpty())
        diplayParts[0] = diplayParts[0].toLower().replace("/", "");

        const auto id = diplayParts.join("");
        const auto name = displayName.toLower();

        typeProviders.push_back(singleParameterProvider(
            {id, name},
            [](const auto&) { return Value(0); } //< TODO: Implement.
        ));

        typeProviders.push_back(singleParameterProvider(
            {id + "Required", name + " required"},
            [](const auto&) { return Value(0); } //< TODO: Implement.
        ));
    }

    return std::make_unique<utils::metrics::ParameterGroupProvider<Resource>>(
        api::metrics::ParameterManifest{"licensing"}, std::move(typeProviders));
}

} // namespace nx::vms::server::metrics
