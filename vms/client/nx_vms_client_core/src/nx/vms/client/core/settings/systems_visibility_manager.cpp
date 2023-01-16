// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_visibility_manager.h"

#include <client_core/client_core_settings.h>
#include <nx/utils/log/log.h>

namespace {

const QnUuid cloudTileId = QnUuid::fromString("cloudWelcomeScreenTile");

} // namespace

namespace nx::vms::client::core {

static SystemsVisibilityManager* s_instance = nullptr;

SystemsVisibilityManager::SystemsVisibilityManager(QObject* parent):
    base_type(parent)
{
    NX_CRITICAL(qnClientCoreSettings, "Client core settings is not initialized yet");

    m_visibilityScopes = qnClientCoreSettings->tileScopeInfo();

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

void SystemsVisibilityManager::removeSystemData(const QnUuid& localId)
{
    m_visibilityScopes.remove(localId);
    qnClientCoreSettings->setTileScopeInfo(m_visibilityScopes);
    qnClientCoreSettings->save();
}

welcome_screen::TileVisibilityScope SystemsVisibilityManager::cloudTileScope() const
{
    return static_cast<welcome_screen::TileVisibilityScope>(
        qnClientCoreSettings->cloudTileScope());
}

void SystemsVisibilityManager::setCloudTileScope(welcome_screen::TileVisibilityScope visibilityScope)
{
    qnClientCoreSettings->setCloudTileScope(visibilityScope);
    qnClientCoreSettings->save();
}

QString SystemsVisibilityManager::systemName(const QnUuid& localId) const
{
    auto it = m_visibilityScopes.find(localId);
    return it != m_visibilityScopes.end() ? it->name : "";
}

welcome_screen::TileVisibilityScope SystemsVisibilityManager::scope(const QnUuid& localId) const
{
    auto it = m_visibilityScopes.find(localId);
    return it != m_visibilityScopes.end()
        ? static_cast<welcome_screen::TileVisibilityScope>(it->visibilityScope)
        : welcome_screen::TileVisibilityScope::DefaultTileVisibilityScope;
}

bool SystemsVisibilityManager::setScopeInfo(
    const QnUuid& localId,
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

    qnClientCoreSettings->setTileScopeInfo(m_visibilityScopes);
    qnClientCoreSettings->save();

    return true;
}

} // namespace nx::vms::client::core
