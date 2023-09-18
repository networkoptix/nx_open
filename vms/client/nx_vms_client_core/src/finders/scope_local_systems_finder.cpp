// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scope_local_systems_finder.h"

#include <network/local_system_description.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>

using namespace nx::vms::client::core;

using TileVisibilityScope = welcome_screen::TileVisibilityScope;

ScopeLocalSystemsFinder::ScopeLocalSystemsFinder(QObject* parent):
    base_type(parent)
{
    connect(&appContext()->coreSettings()->tileScopeInfo,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &ScopeLocalSystemsFinder::updateSystems);
    updateSystems();
}

void ScopeLocalSystemsFinder::updateSystems()
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

        const auto system = QnLocalSystemDescription::create(id.toSimpleString(), id, info.name);

        static const int kVeryFarPriority = 100000;

        nx::vms::api::ModuleInformation fakeServerInfo;
        fakeServerInfo.id = QnUuid::createUuid(); // It must be new unique id.
        fakeServerInfo.systemName = system->name();
        fakeServerInfo.realm = QString::fromStdString(nx::network::AppInfo::realm());
        fakeServerInfo.version = kSystemVersionIsUnknown;
        fakeServerInfo.cloudHost =
            QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());
        system->addServer(fakeServerInfo, kVeryFarPriority, false);
        newSystems.insert(system->id(), system);
    }

    const auto newFinalSystems = filterOutSystems(newSystems);
    setFinalSystems(newFinalSystems);
}
