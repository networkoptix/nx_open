// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_systems_controller.h"

#include <algorithm>

#include <nx/vms/client/core/application_context.h>

namespace nx::vms::client::core {
namespace test {

using TileVisibilityScope = welcome_screen::TileVisibilityScope;

TestSystemsController::TestSystemsController(QObject* parent): AbstractSystemsController(parent)
{
}

TestSystemsController::~TestSystemsController()
{
}

CloudStatusWatcher::Status TestSystemsController::cloudStatus() const
{
    return qnCloudStatusWatcher->status();
}

QString TestSystemsController::cloudLogin() const
{
    return qnCloudStatusWatcher->cloudLogin();
}

SystemDescriptionList TestSystemsController::systemsList() const
{
    return m_systems;
}

TileVisibilityScope TestSystemsController::visibilityScope(const nx::Uuid& localId) const
{
    if (m_scopeInfoHash.contains(localId))
        return m_scopeInfoHash[localId].visibilityScope;
    return TileVisibilityScope::DefaultTileVisibilityScope;
}

bool TestSystemsController::setScopeInfo(
    const nx::Uuid& localId,
    const QString& name,
    TileVisibilityScope visibilityScope)
{
    if (visibilityScope == TileVisibilityScope::DefaultTileVisibilityScope)
        return false;

    auto it = m_scopeInfoHash.find(localId);
    if (it != m_scopeInfoHash.end() && it->name == name && it->visibilityScope == visibilityScope)
        return false;

    m_scopeInfoHash[localId] = {localId, name, visibilityScope};

    return true;
}

void TestSystemsController::emitCloudStatusChanged()
{
    emit cloudStatusChanged();
}

void TestSystemsController::emitCloudLoginChanged()
{
    emit cloudLoginChanged();
}

int TestSystemsController::findSystem(const QString& systemId) const
{
    const auto iter = std::find_if(m_systems.cbegin(), m_systems.cend(),
        [systemId](const SystemDescriptionPtr& system) { return system->id() == systemId; });

    return iter != m_systems.cend() ? (int) std::distance(m_systems.cbegin(), iter) : -1;
}

bool TestSystemsController::discoverSystem(SystemDescriptionPtr systemDescription)
{
    if (findSystem(systemDescription->id()) != -1)
        return false;

    m_systems.push_back(systemDescription);
    emit systemDiscovered(systemDescription);
    return true;
}

bool TestSystemsController::loseSystem(const QString& systemId)
{
    const int index = findSystem(systemId);
    if (index == -1)
        return false;

    const auto system = m_systems.takeAt(index);
    emit systemLost(systemId, system->localId());
    return true;
}

} // namespace test
} // namespace nx::vms::client::core
