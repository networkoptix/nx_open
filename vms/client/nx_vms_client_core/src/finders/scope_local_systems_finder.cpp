// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scope_local_systems_finder.h"

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <network/local_system_description.h>

#include <client_core/client_core_settings.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>

using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

ScopeLocalSystemsFinder::ScopeLocalSystemsFinder(QObject* parent): base_type(parent)
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
    {
        if (valueId == QnClientCoreSettings::TileScopeInfo)
            updateSystems();
    });
    updateSystems();
}

void ScopeLocalSystemsFinder::updateSystems()
{
    SystemsHash newSystems;
    const auto systemScopes = qnClientCoreSettings->tileScopeInfo();
    for (auto iter = systemScopes.begin(); iter != systemScopes.end(); ++iter)
    {
        if (iter->visibilityScope == TileVisibilityScope::DefaultTileVisibilityScope)
            continue;

        const auto system = QnLocalSystemDescription::create(
            iter.key().toString(), iter.key(), iter->name);

        static const int kVeryFarPriority = 100000;

        nx::vms::api::ModuleInformation fakeServerInfo;
        fakeServerInfo.id = QnUuid::createUuid(); // It must be new unique id.
        fakeServerInfo.systemName = system->name();
        fakeServerInfo.realm = QString::fromStdString(nx::network::AppInfo::realm());
        fakeServerInfo.cloudHost = QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());
        system->addServer(fakeServerInfo, kVeryFarPriority, false);
        newSystems.insert(system->id(), system);
    }

    const auto newFinalSystems = filterOutSystems(newSystems);
    setFinalSystems(newFinalSystems);
}
