
#include "direct_systems_finder.h"

#include <network/module_finder.h>

QnDirectSystemsFinder::QnDirectSystemsFinder(QObject *parent)
    : base_type(parent)
{
    const auto moduleFinder = QnModuleFinder::instance();
    NX_ASSERT(moduleFinder, Q_FUNC_INFO, "Module finder does not exist");
    if (!moduleFinder)
        return;

    connect(moduleFinder, &QnModuleFinder::moduleChanged
        , this, &QnDirectSystemsFinder::addServer);
    connect(moduleFinder, &QnModuleFinder::moduleLost
        , this, &QnDirectSystemsFinder::removeServer);
    connect(moduleFinder, &QnModuleFinder::modulePrimaryAddressChanged
        , this, &QnDirectSystemsFinder::updatePrimaryAddress);

    for (const auto moduleInformation: moduleFinder->discoveredServers())
        addServer(moduleInformation);
}

QnDirectSystemsFinder::~QnDirectSystemsFinder()
{}

QnAbstractSystemsFinder::SystemDescriptionList QnDirectSystemsFinder::systems() const
{
    return m_systems.values();
}

void QnDirectSystemsFinder::addServer(const QnModuleInformation &moduleInformation)
{
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    if (systemIt != m_systems.end())
    {
        updateServer(systemIt, moduleInformation);
        return;
    }

    auto itSystem = m_systems.find(moduleInformation.systemName);
    const auto createNewSystem = (itSystem == m_systems.end());
    if (createNewSystem)
    {
        const auto localSystemId = QnUuid::createUuid().toString();
        const auto systemDescription = QnSystemDescription::createLocalSystem(
            localSystemId, moduleInformation.systemName);

        itSystem = m_systems.insert(
            moduleInformation.systemName, systemDescription);
    }

    const auto systemDescription = itSystem.value();
    if (systemDescription->containsServer(moduleInformation.id))
        systemDescription->updateServer(moduleInformation);
    else
        systemDescription->addServer(moduleInformation);
    
    m_serverToSystem[moduleInformation.id] = systemDescription->name();

    if (createNewSystem)
        emit systemDiscovered(systemDescription);
}

void QnDirectSystemsFinder::removeServer(const QnModuleInformation &moduleInformation)
{
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    const auto serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto removedCount = m_serverToSystem.remove(moduleInformation.id);
    if (!removedCount)
        return;

    const auto systemDescription = systemIt.value();
    systemDescription->removeServer(moduleInformation.id);
    if (!systemDescription->servers().isEmpty())
        return;

    m_systems.erase(systemIt);
    emit systemLost(systemDescription->id());
}

void QnDirectSystemsFinder::updateServer(const SystemsHash::iterator systemIt
    , const QnModuleInformation &moduleInformation)
{
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    auto systemDescription = systemIt.value();
    systemDescription->updateServer(moduleInformation);
}

void QnDirectSystemsFinder::updatePrimaryAddress(const QnModuleInformation &moduleInformation
    , const SocketAddress &address)
{
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto systemDescription = systemIt.value();
    systemDescription->setServerHost(moduleInformation.id, address.toString());
}

QnDirectSystemsFinder::SystemsHash::iterator
QnDirectSystemsFinder::getSystemItByServer(const QnUuid &serverId)
{
    const auto it = m_serverToSystem.find(serverId);
    if (it == m_serverToSystem.end())
        return m_systems.end();

    const auto systemName = it.value();
    return m_systems.find(systemName);
}


