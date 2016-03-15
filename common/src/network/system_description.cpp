
#include "system_description.h"

namespace
{

// TODO: #ynikitenkov Add fusion functions
#define EXTRACT_CHANGE_FLAG(fieldName, flag) static_cast<QnServerFields>(     \
    before.fieldName != after.fieldName ? flag : QnServerField::NoField)
    
    QnServerFields getChanges(const QnModuleInformation &before
        , const QnModuleInformation &after)
    {        
        const auto result = (EXTRACT_CHANGE_FLAG(systemName, QnServerField::SystemNameField)
            | EXTRACT_CHANGE_FLAG(name, QnServerField::NameField));

        return result;
    }
#undef EXTRACT_CHANGE_FLAG
}

QnSystemDescriptionPtr QnSystemDescription::createLocalSystem(const QnUuid &systemId
    , const QString &systemName)
{
    return QnSystemDescriptionPtr(
        new QnSystemDescription(systemId, systemName, false));
}

QnSystemDescriptionPtr QnSystemDescription::createCloudSystem(const QnUuid &systemId
    , const QString &systemName)
{
    return QnSystemDescriptionPtr(
        new QnSystemDescription(systemId, systemName, true));
}

QnSystemDescription::QnSystemDescription(const QnUuid &systemId
    , const QString &systemName
    , const bool isCloudSystem)
    : m_id(systemId)
    , m_systemName(systemName)
    , m_isCloudSystem(isCloudSystem)
    , m_servers()
    , m_prioritized()
    , m_primaryAddresses()
{}

QnSystemDescription::~QnSystemDescription()
{}

QnUuid QnSystemDescription::id() const
{
    return m_id;
}

QString QnSystemDescription::name() const
{
    return m_systemName;
}

bool QnSystemDescription::isCloudSystem() const
{
    return m_isCloudSystem;
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

void QnSystemDescription::addServer(const QnModuleInformation &serverInfo
    , int priority)
{
    const bool containsServer = m_servers.contains(serverInfo.id);
    Q_ASSERT_X(!containsServer, Q_FUNC_INFO
        , "System contains specified server");

    if (containsServer)
    {
        updateServer(serverInfo);
        return;
    }

    m_prioritized.insertMulti(priority, serverInfo.id);
    m_servers.insert(serverInfo.id, serverInfo);
    emit serverAdded(serverInfo.id);
}

bool QnSystemDescription::containsServer(const QnUuid &serverId) const
{
    return m_servers.contains(serverId);
}

QnModuleInformation QnSystemDescription::getServer(const QnUuid &serverId) const
{
    Q_ASSERT_X(m_servers.contains(serverId), Q_FUNC_INFO
        , "System does not contain specified server");
    return m_servers.value(serverId);
}

void QnSystemDescription::updateServer(const QnModuleInformation &serverInfo)
{
    const auto it = m_servers.find(serverInfo.id);
    const bool containsServer = (it != m_servers.end());
    Q_ASSERT_X(containsServer, Q_FUNC_INFO
        , "System does not contain specified server");

    if (!containsServer)
    {
        addServer(serverInfo);
        return;
    }

    auto &current = it.value();
    const auto changes = getChanges(current, serverInfo);
    if (changes)
        emit serverChanged(serverInfo.id, changes);
}

void QnSystemDescription::removeServer(const QnUuid &serverId)
{
    const bool containsServer = m_servers.contains(serverId);
    Q_ASSERT_X(containsServer, Q_FUNC_INFO
        , "System does not contain specified server");
    if (!containsServer)
        return;

    const auto priorityPred = [serverId](const QnUuid &id)
        { return (serverId == id); };
    const auto it = std::find_if(m_prioritized.begin()
        , m_prioritized.end(), priorityPred);
    if (it != m_prioritized.end())
        m_prioritized.erase(it);

    m_primaryAddresses.remove(serverId);
    const bool removedCount = m_servers.remove(serverId);
    if (removedCount)
        emit serverRemoved(serverId);
}

void QnSystemDescription::setPrimaryAddress(const QnUuid &serverId
    , const SocketAddress &address)
{
    const bool containsServer = m_servers.contains(serverId);
    
    Q_ASSERT_X(containsServer, Q_FUNC_INFO
        , "System does not contain specified server");
    
    if (!containsServer)
        return;

    m_primaryAddresses[serverId] = address;
    emit serverChanged(serverId, QnServerField::PrimaryAddressField);
}

SocketAddress QnSystemDescription::getServerPrimaryAddress(const QnUuid &serverId) const
{
    Q_ASSERT_X(m_servers.contains(serverId), Q_FUNC_INFO
        , "System does not contain specified server");

    return m_primaryAddresses.value(serverId);
}
