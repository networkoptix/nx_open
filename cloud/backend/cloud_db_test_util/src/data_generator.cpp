#include "data_generator.h"

#include <iostream>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

#include <utils/common/app_info.h>

namespace nx {
namespace cdb {
namespace test {

constexpr int kMaxConcurrentRequests = 50;

DataGenerator::DataGenerator(
    const std::string& cdbUrl,
    const std::string& accountEmail,
    const std::string& accountPassword)
    :
    m_cdbUrl(cdbUrl),
    m_systemsAdded(0)
{
    m_cdbClient.setCloudUrl(m_cdbUrl);
    m_cdbClient.setCredentials(accountEmail, accountPassword);
}

void DataGenerator::createRandomSystems(int systemCount)
{
    using namespace std::placeholders;

    prepareSystemsToAdd(systemCount);
    startAddSystemRequests();
    waitUntilAllSystemsHaveBeenAdded();
}

void DataGenerator::prepareSystemsToAdd(int systemCount)
{
    for (int i = 0; i < systemCount; ++i)
    {
        SystemContext systemContext;
        systemContext.registrationData.customization = QnAppInfo::customizationName().toStdString();
        systemContext.registrationData.name = 
            "load_test_system_" + utils::generateRandomName(8).toStdString();
        m_systemsToRegister.push_back(std::move(systemContext));
    }
}

void DataGenerator::startAddSystemRequests()
{
    QnMutexLocker lock(&m_mutex);
    startAddSystemRequests(lock);
}

void DataGenerator::startAddSystemRequests(const QnMutexLockerBase& /*lock*/)
{
    using namespace std::placeholders;

    for (auto it = m_systemsToRegister.begin(); it != m_systemsToRegister.end(); )
    {
        m_systemsBeingRegistered.push_back(std::move(*it));
        it = m_systemsToRegister.erase(it);
        
        auto systemBeingRegisteredIter = --m_systemsBeingRegistered.end();
        m_cdbClient.systemManager()->bindSystem(
            systemBeingRegisteredIter->registrationData,
            std::bind(&DataGenerator::onSystemBound, this, systemBeingRegisteredIter, _1, _2));
        if (m_systemsBeingRegistered.size() >= kMaxConcurrentRequests)
            break;
    }
}

void DataGenerator::waitUntilAllSystemsHaveBeenAdded()
{
    QnMutexLocker lock(&m_mutex);
    while (!m_systemsBeingRegistered.empty())
        m_cond.wait(lock.mutex());
}

void DataGenerator::onSystemBound(
    std::list<DataGenerator::SystemContext>::iterator systemContextIter,
    api::ResultCode resultCode,
    api::SystemData system)
{
    using namespace std::placeholders;

    if (resultCode != api::ResultCode::ok)
    {
        // TODO: Retrying request.
        QnMutexLocker lock(&m_mutex);
        m_systemsBeingRegistered.erase(systemContextIter);
        startAddSystemRequests(lock);
        m_cond.wakeAll();
        std::cout << "Failed to add system: " << api::toString(resultCode) << std::endl;
        return;
    }

    systemContextIter->cdbClient = std::make_unique<api::CdbClient>();
    systemContextIter->cdbClient->setCloudUrl(m_cdbUrl);
    systemContextIter->cdbClient->setCredentials(system.id, system.authKey);
    systemContextIter->system = std::move(system);

    // Activating system.
    systemContextIter->cdbClient->authProvider()->getCdbNonce(
        std::bind(&DataGenerator::onSystemActivated, this, systemContextIter, _1, _2));
}

void DataGenerator::onSystemActivated(
    std::list<DataGenerator::SystemContext>::iterator systemContextIter,
    api::ResultCode resultCode,
    api::NonceData /*nonce*/)
{
    QnMutexLocker lock(&m_mutex);

    if (resultCode == api::ResultCode::ok)
    {
        ++m_systemsAdded;
        std::cout << "Added system: " << systemContextIter->system.id<<", "
            << systemContextIter->system.name << std::endl;
    }
    else
    {
        std::cout << "Failed to add system: " << api::toString(resultCode) << std::endl;
    }

    m_systemsBeingRegistered.erase(systemContextIter);
    startAddSystemRequests(lock);
    m_cond.wakeAll();
}

} // namespace test
} // namespace cdb
} // namespace nx
