#include "system_description.h"

#include <nx/utils/log/log.h>
#include <network/system_helpers.h>

namespace
{

// TODO: #ynikitenkov Add fusion functions
#define EXTRACT_CHANGE_FLAG(fieldName, flag) static_cast<QnServerFields>(                \
    before.fieldName != after.fieldName ? flag : QnServerField::NoField)

QnServerFields getChanges(const QnModuleInformation& before
    , const QnModuleInformation& after)
{
    const auto fieldsResult =
        (EXTRACT_CHANGE_FLAG(systemName, QnServerField::SystemName)
        | EXTRACT_CHANGE_FLAG(name, QnServerField::Name)
        | EXTRACT_CHANGE_FLAG(cloudSystemId, QnServerField::CloudId)
        | EXTRACT_CHANGE_FLAG(ecDbReadOnly, QnServerField::SafeMode));

    return (fieldsResult);
}
#undef EXTRACT_CHANGE_FLAG
}

QnSystemDescription::QnSystemDescription(
    const QString& systemId,
    const QnUuid& localId,
    const QString& systemName)
    :
    m_id(systemId),
    m_localId(localId),
    m_systemName(extractSystemName(systemName)),
    m_serverTimestamps(),
    m_servers(),
    m_prioritized(),
    m_hosts(),
    m_reachableServers(),
    m_safeMode(false)
{
    const auto updateData =
        [this]() { updateSafeModeState(); };

    connect(this, &QnBaseSystemDescription::serverAdded, this, updateData);
    connect(this, &QnBaseSystemDescription::serverRemoved, this, updateData);

    connect(this, &QnBaseSystemDescription::reachableStateChanged,
        this, &QnBaseSystemDescription::connectableStateChanged);
    connect(this, &QnBaseSystemDescription::runningStateChanged,
        this, &QnBaseSystemDescription::connectableStateChanged);

    connect(this, &QnBaseSystemDescription::serverChanged, this,
        [this](const QnUuid& /*id*/, QnServerFields fields)
        {
            if (fields.testFlag(QnServerField::SafeMode))
                updateSafeModeState();
        });
}

QnSystemDescription::~QnSystemDescription()
{}

QString QnSystemDescription::extractSystemName(const QString& systemName)
{
    return systemName.isEmpty()
        ? L'<' + tr("Unnamed System") + L'>'
        : systemName;
}

QString QnSystemDescription::id() const
{
    return m_id;
}

QnUuid QnSystemDescription::localId() const
{
    return m_localId;
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

bool QnSystemDescription::isReachableServer(const QnUuid& serverId) const
{
    return m_reachableServers.contains(serverId);
}

QnSystemDescription::ServersList QnSystemDescription::servers() const
{
    ServersList result;
    for (const auto& id: m_prioritized)
    {
        const auto it = m_servers.find(id);
        if (it != m_servers.end())
            result.append(it.value());
    }
    return result;
}

void QnSystemDescription::addServer(const QnModuleInformation& serverInfo,
    int priority, bool online)
{
    const bool containsServer = m_servers.contains(serverInfo.id);
    NX_ASSERT(!containsServer, Q_FUNC_INFO, "System contains specified server");

    if (containsServer)
    {
        updateServer(serverInfo);
        return;
    }

    if (online)
        handleReachableServerAdded(serverInfo.id);

    m_prioritized.insertMulti(priority, serverInfo.id);
    m_servers.insert(serverInfo.id, serverInfo);
    m_serverTimestamps[serverInfo.id].restart();
    setName(serverInfo.systemName);
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

    setName(serverInfo.systemName);
    emit serverChanged(serverInfo.id, changes);
    return changes;
}

bool QnSystemDescription::isReachable() const
{
    return !m_reachableServers.isEmpty();
}

bool QnSystemDescription::isConnectable() const
{
    return (isReachable() && isRunning());
}

void QnSystemDescription::handleReachableServerAdded(const QnUuid& serverId)
{
    const bool wasReachable = isReachable();

    const bool containsAlready = m_reachableServers.contains(serverId);
    NX_ASSERT(!containsAlready, "Server is supposed as reachable already");
    if (containsAlready)
        return;

    m_reachableServers.insert(serverId);
    if (wasReachable != isReachable())
        emit reachableStateChanged();
}

void QnSystemDescription::handleServerRemoved(const QnUuid& serverId)
{
    const bool wasReachable = isReachable();
    if (m_reachableServers.remove(serverId) && (wasReachable != isReachable()))
        emit reachableStateChanged();
}

void QnSystemDescription::removeServer(const QnUuid& serverId)
{
    const bool containsServer = m_servers.contains(serverId);
    NX_ASSERT(containsServer, Q_FUNC_INFO,
        "System does not contain specified server");
    if (!containsServer)
        return;

    handleServerRemoved(serverId);
    const auto priorityPred = [serverId](const QnUuid& id) { return (serverId == id); };
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
    const auto newValue = extractSystemName(value);
    if (m_systemName == newValue)
        return;

    m_systemName = newValue;
    emit systemNameChanged();
}

void QnSystemDescription::setServerHost(const QnUuid& serverId, const nx::utils::Url& host)
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
    emit serverChanged(serverId, QnServerField::Host);
}

nx::utils::Url QnSystemDescription::getServerHost(const QnUuid& serverId) const
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

bool QnSystemDescription::safeMode() const
{
    return m_safeMode;
}

void QnSystemDescription::updateSafeModeState()
{
    const bool newSafeModeState = std::any_of(m_servers.begin(), m_servers.end(),
        [](const QnModuleInformation& info) { return helpers::isSafeMode(info); });

    if (newSafeModeState == m_safeMode)
        return;

    m_safeMode = newSafeModeState;
    emit safeModeStateChanged();
}
