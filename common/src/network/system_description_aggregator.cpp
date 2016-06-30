
#include "system_description_aggregator.h"

#include <nx/utils/log/assert.h>

namespace {

    QnBaseSystemDescription::ServersList subtractLists(
        const QnBaseSystemDescription::ServersList& first, 
        const QnBaseSystemDescription::ServersList& second)
    {
        QnBaseSystemDescription::ServersList result;
        std::copy_if(first.begin(), first.end(), std::back_inserter(result),
                     [&second](const QnModuleInformation& first)
        {
            const auto secondIt = std::find_if(second.begin(), second.end(),
                                               [first](const QnModuleInformation& second)
            {
                return (first.id == second.id);
            });

            return (secondIt == second.end()); // add to result if not found in second
        });
        return result;
    };

}

QnSystemDescriptionAggregator::QnSystemDescriptionAggregator(const QnSystemDescriptionPtr &systemDescription)
    : m_cloudSystem(systemDescription && systemDescription->isCloudSystem() ? 
        systemDescription : QnSystemDescriptionPtr())
    , m_localSystem(systemDescription && !systemDescription->isCloudSystem() ?
        systemDescription : QnSystemDescriptionPtr())
{
    NX_ASSERT(m_cloudSystem || m_localSystem, "Empty system description is no allowed");
}

QnSystemDescriptionAggregator::~QnSystemDescriptionAggregator()
{
}

bool QnSystemDescriptionAggregator::isAggregator() const
{
    return (m_cloudSystem && m_localSystem);
}

bool QnSystemDescriptionAggregator::containsSystem(const QString& systemId) const
{
    return ((m_localSystem && (m_localSystem->id() == systemId)) ||
        (m_cloudSystem && (m_cloudSystem->id() == systemId)));
}

void QnSystemDescriptionAggregator::mergeSystem(const QnSystemDescriptionPtr& system)
{
    NX_ASSERT(system, "System is empty");
    if (!system)
        return;

    const bool isInCloud = system->isCloudSystem();
    const bool cloudSystemSet = (m_cloudSystem && isInCloud);
    NX_ASSERT(!cloudSystemSet, "Cloud system is set already");
    if (cloudSystemSet)
        return;

    const bool localSystemSet = (m_localSystem && !isInCloud);
    NX_ASSERT(!localSystemSet, "Local system is set already");
    if (localSystemSet)
        return;

    const bool wasCloudSystem = isCloudSystem();
    const auto oldServers = servers();

    if (isInCloud)
        m_cloudSystem = system;
    else
        m_localSystem = system;

    emitChangesSignals(wasCloudSystem, oldServers);
}

void QnSystemDescriptionAggregator::emitChangesSignals(bool wasCloudSystem
                                                       , const ServersList& oldServers)
{
    if (wasCloudSystem != isCloudSystem())
    {
        emit idChanged();
        emit isCloudSystemChanged();
        emit ownerChanged();
    }

    const auto newServers = servers();

    const auto toRemove = subtractLists(oldServers, newServers);
    for (const auto& server : toRemove)
        emit serverRemoved(server.id);

    const auto toAdd = subtractLists(newServers, oldServers);
    for (const auto& server : toAdd)
        emit serverAdded(server.id);
}

void QnSystemDescriptionAggregator::removeSystem(const QString& id)
{
    const auto wasCloudSystem = isCloudSystem();
    const auto oldServers = servers();

    if (m_cloudSystem && m_cloudSystem->id() == id)
        m_cloudSystem.reset();
    else if (m_localSystem && m_localSystem->id() == id)
        m_localSystem.reset();
    else
        NX_ASSERT(false, "Never should get here!");


    NX_ASSERT(m_cloudSystem || m_localSystem, "Empty system aggregator");
    emitChangesSignals(wasCloudSystem, oldServers);
}

QString QnSystemDescriptionAggregator::id() const
{
    return (m_cloudSystem ? m_cloudSystem->id() : m_localSystem->id());
}

QString QnSystemDescriptionAggregator::name() const
{
    return (m_cloudSystem ? m_cloudSystem->name() : m_localSystem->name());
}

QString QnSystemDescriptionAggregator::ownerAccountEmail() const
{
    return (m_cloudSystem ? m_cloudSystem->ownerAccountEmail() : m_localSystem->ownerAccountEmail());
}

QString QnSystemDescriptionAggregator::ownerFullName() const
{
    return (m_cloudSystem ? m_cloudSystem->ownerFullName() : m_localSystem->ownerFullName());
}

bool QnSystemDescriptionAggregator::isCloudSystem() const
{
    return m_cloudSystem;
}

QnBaseSystemDescription::ServersList QnSystemDescriptionAggregator::servers() const
{
    // Cloud servers has priority under local servers
    auto result = (m_cloudSystem ? m_cloudSystem->servers() : ServersList());
    
    const auto localSystemServers = (m_localSystem ? m_localSystem->servers() : ServersList());
    const auto toAddList = (localSystemServers, result);
    std::copy(toAddList.begin(), toAddList.end(), std::back_inserter(result));

    return result;
}

bool QnSystemDescriptionAggregator::containsServer(const QnUuid &serverId) const
{
    return ((m_cloudSystem && m_cloudSystem->containsServer(serverId)) || 
        (m_localSystem && m_localSystem->containsServer(serverId)));
}

QnModuleInformation QnSystemDescriptionAggregator::getServer(const QnUuid &serverId) const
{
    if (m_cloudSystem && m_cloudSystem->containsServer(serverId))
        return m_cloudSystem->getServer(serverId);

    NX_ASSERT(m_localSystem, "Invalid system description aggregator");
    if (!m_localSystem)
        return QnModuleInformation();

    return m_localSystem->getServer(serverId);
}

QString QnSystemDescriptionAggregator::getServerHost(const QnUuid &serverId) const
{
    if (m_cloudSystem && m_cloudSystem->containsServer(serverId))
        return m_cloudSystem->getServerHost(serverId);

    NX_ASSERT(m_localSystem, "Invalid system description aggregator");
    if (!m_localSystem)
        return QString();

    return m_localSystem->getServerHost(serverId);
}

qint64 QnSystemDescriptionAggregator::getServerLastUpdatedMs(const QnUuid &serverId) const
{
    const qint64 localMs = (m_localSystem ? m_localSystem->getServerLastUpdatedMs(serverId) : 0);
    const qint64 cloudMs = (m_cloudSystem ? m_cloudSystem->getServerLastUpdatedMs(serverId) : 0);

    return std::max<qint64>(localMs, cloudMs);
}
