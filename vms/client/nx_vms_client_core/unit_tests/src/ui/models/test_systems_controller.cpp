// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_systems_controller.h"

#include <algorithm>

using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

TestSystemsController::TestSystemsController(QObject* parent): AbstractSystemsController(parent)
{
}

TestSystemsController::~TestSystemsController()
{
}

QnCloudStatusWatcher::Status TestSystemsController::cloudStatus() const
{
    return qnCloudStatusWatcher->status();
}

QString TestSystemsController::cloudLogin() const
{
    return qnCloudStatusWatcher->cloudLogin();
}

QnAbstractSystemsFinder::SystemDescriptionList TestSystemsController::systemsList() const
{
    return m_systems;
}

TileVisibilityScope TestSystemsController::visibilityScope(const QnUuid& localId) const
{
    if (m_scopeInfoHash.contains(localId))
        return m_scopeInfoHash[localId].visibilityScope;
    return TileVisibilityScope::DefaultTileVisibilityScope;
}

bool TestSystemsController::setScopeInfo(
    const QnUuid& localId,
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

void TestSystemsController::emitSystemDiscovered(QnSystemDescriptionPtr systemDescription)
{
    m_systems.push_back(systemDescription);

    emit systemDiscovered(systemDescription);
}

void TestSystemsController::emitSystemLost(const QString& systemId)
{
    auto iter = std::find_if(m_systems.begin(), m_systems.end(),
        [systemId](QnSystemDescriptionPtr system){
            return system->localId() == QnUuid(systemId);
        });

    if (iter != m_systems.end())
        m_systems.erase(iter);

    emit systemLost(systemId);
}

