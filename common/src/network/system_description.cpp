
#include "system_description.h"

#include <nx/utils/log/log.h>

namespace
{

// TODO: #ynikitenkov Add fusion functions
#define EXTRACT_CHANGE_FLAG(fieldName, flag) static_cast<QnServerFields>(     \
    before.fieldName != after.fieldName ? flag : QnServerField::NoField)

    QnServerFields getChanges(const QnModuleInformation &before
        , const QnModuleInformation &after)
    {
        auto result =
            (EXTRACT_CHANGE_FLAG(systemName, QnServerField::SystemNameField)
            | EXTRACT_CHANGE_FLAG(name, QnServerField::NameField)
            | EXTRACT_CHANGE_FLAG(serverFlags, QnServerField::FlagsField)
            | EXTRACT_CHANGE_FLAG(cloudSystemId, QnServerField::CloudIdField));

        return result;
    }
#undef EXTRACT_CHANGE_FLAG
}

QnSystemDescription::PointerType QnSystemDescription::createFactorySystem(const QString& systemId)
{
    return PointerType(new QnSystemDescription(systemId));
}

QnSystemDescription::PointerType QnSystemDescription::createLocalSystem(
    const QString& systemId,
    const QString& systemName)
{
    return PointerType(new QnSystemDescription(systemId, systemName));
}

QnSystemDescription::PointerType QnSystemDescription::createCloudSystem(
    const QString& systemId,
    const QString& systemName,
    const QString& ownerAccountEmail,
    const QString& ownerFullName)
{
    return PointerType(
        new QnSystemDescription(systemId, systemName
            , ownerAccountEmail, ownerFullName));
}

QnSystemDescription::QnSystemDescription(const QString& systemId) :
    m_id(systemId),
    m_ownerAccountEmail(),
    m_ownerFullName(),
    m_isCloudSystem(false),
    m_isNewSystem(true),
    m_systemName(tr("New system")),
    m_serverTimestamps(),
    m_servers(),
    m_prioritized(),
    m_hosts()
{}


QnSystemDescription::QnSystemDescription(const QString& systemId, const QString& systemName) :
    m_id(systemId),
    m_ownerAccountEmail(),
    m_ownerFullName(),
    m_isCloudSystem(false),
    m_isNewSystem(false),
    m_systemName(extractSystemName(systemName)),
    m_serverTimestamps(),
    m_servers(),
    m_prioritized(),
    m_hosts()
{}

QnSystemDescription::QnSystemDescription(
    const QString& systemId,
    const QString& systemName,
    const QString& cloudOwnerAccountEmail,
    const QString& ownerFullName)
    :
    m_id(systemId),
    m_ownerAccountEmail(cloudOwnerAccountEmail),
    m_ownerFullName(ownerFullName),
    m_isCloudSystem(true),
    m_isNewSystem(false),
    m_systemName(extractSystemName(systemName)),
    m_serverTimestamps(),
    m_servers(),
    m_prioritized(),
    m_hosts()
{}

QnSystemDescription::~QnSystemDescription()
{}

QString QnSystemDescription::extractSystemName(const QString& systemName)
{
    return (systemName.isEmpty() ? tr("<Unnamed system>") : systemName);
}

QString QnSystemDescription::id() const
{
    return m_id;
}

QString QnSystemDescription::name() const
{
    return m_systemName;
}

QString QnSystemDescription::ownerAccountEmail() const
{
    return m_ownerAccountEmail;
}

QString QnSystemDescription::ownerFullName() const
{
    return m_ownerFullName;
}

bool QnSystemDescription::isCloudSystem() const
{
    return m_isCloudSystem;
}

bool QnSystemDescription::isNewSystem() const
{
    return m_isNewSystem;
}

QnSystemDescription::ServersList QnSystemDescription::servers() const
{
    ServersList result;
    for (const auto id: m_prioritized)
    {
        const auto it = m_servers.find(id);
        if (it != m_servers.end())
            result.append(it.value());
    }
    return result;
}

void QnSystemDescription::addServer(const QnModuleInformation& serverInfo, int priority)
{
    const bool containsServer = m_servers.contains(serverInfo.id);
    NX_ASSERT(!containsServer, Q_FUNC_INFO, "System contains specified server");

    if (containsServer)
    {
        updateServer(serverInfo);
        return;
    }

    m_prioritized.insertMulti(priority, serverInfo.id);
    m_servers.insert(serverInfo.id, serverInfo);
    m_serverTimestamps[serverInfo.id].restart();
    emit serverAdded(serverInfo.id);
}

bool QnSystemDescription::containsServer(const QnUuid& serverId) const
{
    return m_servers.contains(serverId);
}

QnModuleInformation QnSystemDescription::getServer(const QnUuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId), Q_FUNC_INFO,
        "System does not contain specified server");
    return m_servers.value(serverId);
}

QnServerFields QnSystemDescription::updateServer(const QnModuleInformation& serverInfo)
{
    const auto it = m_servers.find(serverInfo.id);
    const bool containsServer = (it != m_servers.end());
    NX_ASSERT(containsServer, Q_FUNC_INFO,
        "System does not contain specified server");

    if (!containsServer)
    {
        addServer(serverInfo, kDefaultPriority);
        return QnServerField::NoField;
    }

    auto& current = it.value();
    const auto changes = getChanges(current, serverInfo);
    m_serverTimestamps[serverInfo.id].restart();
    current = serverInfo;
    if (!changes)
        return QnServerField::NoField;

    emit serverChanged(serverInfo.id, changes);
    return changes;
}

void QnSystemDescription::removeServer(const QnUuid& serverId)
{
    const bool containsServer = m_servers.contains(serverId);
    NX_ASSERT(containsServer, Q_FUNC_INFO,
        "System does not contain specified server");
    if (!containsServer)
        return;

    const auto priorityPred = [serverId](const QnUuid &id) { return (serverId == id); };
    const auto it = std::find_if(m_prioritized.begin(), m_prioritized.end(), priorityPred);
    if (it != m_prioritized.end())
        m_prioritized.erase(it);

    m_hosts.remove(serverId);
    m_serverTimestamps.remove(serverId);
    const bool someoneRemoved = m_servers.remove(serverId);
    if (someoneRemoved)
        emit serverRemoved(serverId);
}

void QnSystemDescription::setName(const QString& value)
{
    if (m_systemName == value)
        return;

    m_systemName = value;
    emit systemNameChanged();
}

void QnSystemDescription::setServerHost(const QnUuid& serverId, const QUrl& host)
{
    const bool containsServer = m_servers.contains(serverId);

    NX_ASSERT(containsServer, Q_FUNC_INFO,
        "System does not contain specified server");

    if (!containsServer)
        return;

    const auto it = m_hosts.find(serverId);
    const bool changed = ((it == m_hosts.end())
        || (it.value() != host));

    m_serverTimestamps[serverId].restart();
    if (!changed)
        return;
    m_hosts[serverId] = host;
    emit serverChanged(serverId, QnServerField::HostField);
}

QUrl QnSystemDescription::getServerHost(const QnUuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId), Q_FUNC_INFO,
        "System does not contain specified server");

    return m_hosts.value(serverId);
}

qint64 QnSystemDescription::getServerLastUpdatedMs(const QnUuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId), Q_FUNC_INFO,
        "System does not contain specified server");

    return m_serverTimestamps.value(serverId).elapsed();
}

