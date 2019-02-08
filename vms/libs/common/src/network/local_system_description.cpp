#include "local_system_description.h"

#include <network/system_helpers.h>

#include <nx/vms/api/data_fwd.h>

QnSystemDescription::PointerType QnLocalSystemDescription::createFactory(const QString& systemId)
{
    return PointerType(static_cast<base_type*>(new QnLocalSystemDescription(systemId)));
}

QnSystemDescription::PointerType QnLocalSystemDescription::create(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName)
{
    return PointerType(static_cast<base_type*>(
        new QnLocalSystemDescription(systemId, localSystemId, systemName)));
}

QnLocalSystemDescription::QnLocalSystemDescription(const QString& systemId):
    base_type(systemId, QnUuid::fromStringSafe(systemId), tr("New Server")),
    m_isNewSystem(true)
{
    init();
}

QnLocalSystemDescription::QnLocalSystemDescription(
    const QString& systemId,
    const QnUuid& localSystemId,
    const QString& systemName)
    :
    base_type(systemId, localSystemId, systemName),
    m_isNewSystem(false)
{
    init();
}

void QnLocalSystemDescription::init()
{
    connect(this, &QnBaseSystemDescription::safeModeStateChanged, this,
        [this]() { updateNewSystemState(); });
}

bool QnLocalSystemDescription::isCloudSystem() const
{
    return false;
}

bool QnLocalSystemDescription::isRunning() const
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
        this, &QnBaseSystemDescription::runningStateChanged);

    const auto currentServers = servers();
    const bool newSystemState = std::any_of(currentServers.begin(), currentServers.end(),
        [](const nx::vms::api::ModuleInformation& info) { return helpers::isNewSystem(info); });

    if (newSystemState == m_isNewSystem)
        return;

    m_isNewSystem = newSystemState;
    emit newSystemStateChanged();
}
