#pragma once

#include <cdb/cdb_client.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <test_support/transaction_connection_helper.h>

namespace nx {
namespace cdb {
namespace test {

class LoadEmulator
{
public:
    LoadEmulator(
        const std::string& cdbUrl,
        const std::string& accountEmail,
        const std::string& accountPassword);

    void setTransactionConnectionCount(int connectionCount);

    void createRandomSystems(int systemCount);

    void start();

private:
    const std::string m_cdbUrl;
    api::CdbClient m_cdbClient;
    api::SystemDataExList m_systems;
    int m_transactionConnectionCount;
    TransactionConnectionHelper m_connectionHelper;
    std::atomic<int> m_awaitedResponseCount;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    void onSystemBound(
        api::SystemRegistrationData registrationData,
        api::ResultCode resultCode,
        api::SystemData system);
    void onSystemListReceived(api::ResultCode resultCode, api::SystemDataExList systems);
    void openConnections();
};

} // namespace test
} // namespace cdb
} // namespace nx
