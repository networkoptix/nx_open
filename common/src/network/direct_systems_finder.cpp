
#include "direct_systems_finder.h"

#include <network/module_finder.h>

QnDirectSystemsFinder::QnDirectSystemsFinder(QObject *parent)
    : base_type(parent)
{
    const auto moduleFinder = QnModuleFinder::instance();
    Q_ASSERT_X(moduleFinder, Q_FUNC_INFO, "Module finder does not exist");
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
    SystemDescriptionList result;
    for (const auto systemDesciption: m_systems)
        result.append(systemDesciption);

    return result;
}

void QnDirectSystemsFinder::addServer(const QnModuleInformation &moduleInformation)
{
    if (moduleInformation.type != QnModuleInformation::nxMediaServerId())
        return;

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
        const auto localSystemId = QnUuid::createUuid();
        const auto systemDescription = QnSystemDescription::createLocalSystem(
            localSystemId, moduleInformation.systemName);

        itSystem = m_systems.insert(
            moduleInformation.systemName, systemDescription);
    }

    const auto systemDescription = itSystem.value();
    if (systemDescription->isContainServer(moduleInformation.id))
        systemDescription->updateServer(moduleInformation);
    else
        systemDescription->addServer(moduleInformation);
    
    m_serverToSystem[moduleInformation.id] = itSystem;

    if (createNewSystem)
        emit systemDiscovered(systemDescription);
}

void QnDirectSystemsFinder::removeServer(const QnModuleInformation &moduleInformation)
{
    if (moduleInformation.type != QnModuleInformation::nxMediaServerId())
        return;

    const auto systemIt = getSystemItByServer(moduleInformation.id);
    const auto serverIsInKnownSystem = (systemIt != m_systems.end());
    Q_ASSERT_X(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto removedCount = m_serverToSystem.remove(moduleInformation.id);
    if (!removedCount)
        return;

    const auto systemDescription = systemIt.value();
    if (!systemDescription->servers().isEmpty())
        return;

    m_systems.erase(systemIt);
    emit systemLost(systemDescription->id());
}

void QnDirectSystemsFinder::updateServer(const SystemsHash::iterator systemIt
    , const QnModuleInformation &moduleInformation)
{
    const bool isServer = moduleInformation.type != QnModuleInformation::nxMediaServerId();
    Q_ASSERT_X(isServer, Q_FUNC_INFO, "Module is not server");
    if (!isServer)
        return;

    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    Q_ASSERT_X(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    auto systemDescription = systemIt.value();
    systemDescription->updateServer(moduleInformation);
}

void QnDirectSystemsFinder::updatePrimaryAddress(const QnModuleInformation &moduleInformation
    , const SocketAddress &address)
{
    if (moduleInformation.type != QnModuleInformation::nxMediaServerId())
        return;
    
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    Q_ASSERT_X(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto serverDescrption = systemIt.value();
    serverDescrption->setPrimaryAddress(moduleInformation.id, address);
}

QnDirectSystemsFinder::SystemsHash::iterator
QnDirectSystemsFinder::getSystemItByServer(const QnUuid &serverId)
{
    const auto it = m_serverToSystem.find(serverId);
    return (it == m_serverToSystem.end() ? m_systems.end() : it.value());
}


