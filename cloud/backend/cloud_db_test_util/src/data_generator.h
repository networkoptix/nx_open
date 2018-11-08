#pragma once

#include <list>

#include <nx/cloud/cdb/api/cdb_client.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace cdb {
namespace test {

class DataGenerator
{
public:
    DataGenerator(
        const std::string& cdbUrl,
        const std::string& accountEmail,
        const std::string& accountPassword);

    void createRandomSystems(int systemCount);

private:
    struct SystemContext
    {
        api::SystemRegistrationData registrationData;
        api::SystemData system;
        std::unique_ptr<api::CdbClient> cdbClient;
    };

    const std::string m_cdbUrl;
    api::CdbClient m_cdbClient;
    api::SystemDataExList m_systems;
    std::list<SystemContext> m_systemsToRegister;
    std::list<SystemContext> m_systemsBeingRegistered;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::atomic<int> m_systemsAdded;

    void prepareSystemsToAdd(int systemCount);
    void startAddSystemRequests();
    void startAddSystemRequests(const QnMutexLockerBase& lock);
    void waitUntilAllSystemsHaveBeenAdded();

    void onSystemBound(
        std::list<SystemContext>::iterator systemContextIter,
        api::ResultCode resultCode,
        api::SystemData system);
    void onSystemActivated(
        std::list<SystemContext>::iterator systemContextIter,
        api::ResultCode resultCode,
        api::NonceData nonce);
};

} // namespace test
} // namespace cdb
} // namespace nx
