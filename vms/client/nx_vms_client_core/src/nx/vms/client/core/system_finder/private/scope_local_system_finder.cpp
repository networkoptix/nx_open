// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scope_local_system_finder.h"

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>

#include "local_system_description.h"

namespace nx::vms::client::core {

using TileVisibilityScope = welcome_screen::TileVisibilityScope;

ScopeLocalSystemFinder::ScopeLocalSystemFinder(QObject* parent):
    base_type(parent)
{
    connect(&appContext()->coreSettings()->tileScopeInfo,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &ScopeLocalSystemFinder::updateSystems);
    updateSystems();
}

void ScopeLocalSystemFinder::updateSystems()
{
    // Connection version is not actual for the systems, which are not accessible in any way, as
    // tile is offline and does not contain an address.
    static constexpr nx::utils::SoftwareVersion kSystemVersionIsUnknown{5, 1, 0, 0};

    SystemsHash newSystems;
    const auto systemScopes = appContext()->coreSettings()->tileScopeInfo();
    for (const auto& [id, info]: systemScopes.asKeyValueRange())
    {
        if (info.visibilityScope == TileVisibilityScope::DefaultTileVisibilityScope)
            continue;

        const auto system = LocalSystemDescription::create(
            id.toSimpleString(),
            id,
            /*cloudSystemId*/ QString(),
            info.name);

        nx::vms::api::ModuleInformation fakeServerInfo;
        fakeServerInfo.id = nx::Uuid::createUuid(); // It must be new unique id.
        fakeServerInfo.systemName = system->name();
        fakeServerInfo.realm = QString::fromStdString(nx::network::AppInfo::realm());
        fakeServerInfo.version = kSystemVersionIsUnknown;
        fakeServerInfo.cloudHost =
            QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());
        system->addServer(fakeServerInfo, /*online*/ false);
        newSystems.insert(system->id(), system);
    }

    setSystems(newSystems);
}

} // namespace nx::vms::client::core
