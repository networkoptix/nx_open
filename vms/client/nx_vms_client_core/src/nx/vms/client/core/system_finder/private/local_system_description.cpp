// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_system_description.h"

#include <network/system_helpers.h>

namespace nx::vms::client::core {

LocalSystemDescriptionPtr LocalSystemDescription::createFactory(const QString& systemId)
{
    return LocalSystemDescriptionPtr(new LocalSystemDescription(systemId));
}

LocalSystemDescriptionPtr LocalSystemDescription::create(
    const QString& systemId,
    const nx::Uuid& localSystemId,
    const QString& systemName)
{
    return LocalSystemDescriptionPtr(
        new LocalSystemDescription(systemId, localSystemId, systemName));
}

LocalSystemDescription::LocalSystemDescription(const QString& systemId):
    base_type(systemId, nx::Uuid::fromStringSafe(systemId), tr("New Server")),
    m_isNewSystem(true)
{
    init();
}

LocalSystemDescription::LocalSystemDescription(
    const QString& systemId,
    const nx::Uuid& localSystemId,
    const QString& systemName)
    :
    base_type(systemId, localSystemId, systemName),
    m_isNewSystem(false)
{
    init();
}

void LocalSystemDescription::init()
{
    connect(this, &SystemDescription::newSystemStateChanged, this,
        &LocalSystemDescription::updateNewSystemState);
}

bool LocalSystemDescription::isCloudSystem() const
{
    return false;
}

bool LocalSystemDescription::is2FaEnabled() const
{
    return false;
}

bool LocalSystemDescription::isOnline() const
{
    return isReachable();
}

bool LocalSystemDescription::isNewSystem() const
{
    return m_isNewSystem;
}

QString LocalSystemDescription::ownerAccountEmail() const
{
    return QString();
}

QString LocalSystemDescription::ownerFullName() const
{
    return QString();
}

void LocalSystemDescription::updateNewSystemState()
{
    connect(this, &SystemDescription::reachableStateChanged,
        this, &SystemDescription::onlineStateChanged);

    const auto currentServers = servers();
    const bool newSystemState = std::any_of(currentServers.begin(), currentServers.end(),
        [](const nx::vms::api::ModuleInformationWithAddresses& info) { return helpers::isNewSystem(info); });

    if (newSystemState == m_isNewSystem)
        return;

    m_isNewSystem = newSystemState;
    emit newSystemStateChanged();
}

} // namespace nx::vms::client::core
