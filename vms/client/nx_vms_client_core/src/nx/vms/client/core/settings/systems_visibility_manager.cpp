// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_visibility_manager.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace {

const nx::Uuid cloudTileId = nx::Uuid::fromString("cloudWelcomeScreenTile");

} // namespace

namespace nx::vms::client::core {

static SystemsVisibilityManager* s_instance = nullptr;

SystemsVisibilityManager::SystemsVisibilityManager(QObject* parent):
    base_type(parent)
{
    m_visibilityScopes = appContext()->coreSettings()->tileScopeInfo();

    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;
}

SystemsVisibilityManager::~SystemsVisibilityManager()
{
    if (s_instance == this)
        s_instance = nullptr;
}

SystemsVisibilityManager* SystemsVisibilityManager::instance()
{
    return s_instance;
}

void SystemsVisibilityManager::removeSystemData(const nx::Uuid& localId)
{
    m_visibilityScopes.remove(localId);
    appContext()->coreSettings()->tileScopeInfo = m_visibilityScopes;
}

welcome_screen::TileVisibilityScope SystemsVisibilityManager::cloudTileScope() const
{
    return appContext()->coreSettings()->cloudTileScope();
}

void SystemsVisibilityManager::setCloudTileScope(welcome_screen::TileVisibilityScope visibilityScope)
{
    appContext()->coreSettings()->cloudTileScope = visibilityScope;
}

QString SystemsVisibilityManager::systemName(const nx::Uuid& localId) const
{
    auto it = m_visibilityScopes.find(localId);
    return it != m_visibilityScopes.end() ? it->name : QString();
}

welcome_screen::TileVisibilityScope SystemsVisibilityManager::scope(const nx::Uuid& localId) const
{
    auto it = m_visibilityScopes.find(localId);
    return it != m_visibilityScopes.end()
        ? it->visibilityScope
        : welcome_screen::TileVisibilityScope::DefaultTileVisibilityScope;
}

bool SystemsVisibilityManager::setScopeInfo(
    const nx::Uuid& localId,
    const QString& name,
    welcome_screen::TileVisibilityScope visibilityScope)
{
    if (visibilityScope == welcome_screen::TileVisibilityScope::DefaultTileVisibilityScope)
    {
        if (!m_visibilityScopes.remove(localId))
            return false;
    }
    else
    {
        auto it = m_visibilityScopes.find(localId);
        if (it != m_visibilityScopes.end())
        {
            if (it->name == name && it->visibilityScope == visibilityScope)
                return false;

            it->name = name;
            it->visibilityScope = visibilityScope;
        }
        else
        {
            m_visibilityScopes[localId] = {name, visibilityScope};
        }
    }

    appContext()->coreSettings()->tileScopeInfo = m_visibilityScopes;

    return true;
}

} // namespace nx::vms::client::core
