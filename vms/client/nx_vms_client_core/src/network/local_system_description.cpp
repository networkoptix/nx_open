// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_system_description.h"

#include <network/system_helpers.h>

QnLocalSystemDescriptionPtr QnLocalSystemDescription::createFactory(const QString& systemId)
{
    return QnLocalSystemDescriptionPtr(new QnLocalSystemDescription(systemId));
}

QnLocalSystemDescriptionPtr QnLocalSystemDescription::create(
    const QString& systemId,
    const nx::Uuid& localSystemId,
    const QString& systemName)
{
    return QnLocalSystemDescriptionPtr(
        new QnLocalSystemDescription(systemId, localSystemId, systemName));
}

QnLocalSystemDescription::QnLocalSystemDescription(const QString& systemId):
    base_type(systemId, nx::Uuid::fromStringSafe(systemId), tr("New Server")),
    m_isNewSystem(true)
{
    init();
}

QnLocalSystemDescription::QnLocalSystemDescription(
    const QString& systemId,
    const nx::Uuid& localSystemId,
    const QString& systemName)
    :
    base_type(systemId, localSystemId, systemName),
    m_isNewSystem(false)
{
    init();
}

void QnLocalSystemDescription::init()
{
    connect(this, &QnBaseSystemDescription::newSystemStateChanged, this,
        &QnLocalSystemDescription::updateNewSystemState);
}

bool QnLocalSystemDescription::isCloudSystem() const
{
    return false;
}

bool QnLocalSystemDescription::is2FaEnabled() const
{
    return false;
}

bool QnLocalSystemDescription::isOnline() const
{
    return isReachable();
}

bool QnLocalSystemDescription::isNewSystem() const
{
    return m_isNewSystem;
}

QString QnLocalSystemDescription::ownerAccountEmail() const
{
    return QString();
}

QString QnLocalSystemDescription::ownerFullName() const
{
    return QString();
}

void QnLocalSystemDescription::updateNewSystemState()
{
    connect(this, &QnBaseSystemDescription::reachableStateChanged,
        this, &QnBaseSystemDescription::onlineStateChanged);

    const auto currentServers = servers();
    const bool newSystemState = std::any_of(currentServers.begin(), currentServers.end(),
        [](const nx::vms::api::ModuleInformationWithAddresses& info) { return helpers::isNewSystem(info); });

    if (newSystemState == m_isNewSystem)
        return;

    m_isNewSystem = newSystemState;
    emit newSystemStateChanged();
}
