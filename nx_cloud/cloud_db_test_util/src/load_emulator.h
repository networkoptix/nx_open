#pragma once

#include <list>

#include <nx/cloud/cdb/api/cdb_client.h>

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

    void setMaxDelayBeforeConnect(std::chrono::milliseconds delay);
    void setTransactionConnectionCount(int connectionCount);

    void start();

    std::size_t activeConnectionCount() const;
    std::size_t totalFailedConnections() const;
    std::size_t connectedConnections() const;

private:
    const std::string m_cdbUrl;
    api::CdbClient m_cdbClient;
    api::SystemDataExList m_systems;
    int m_transactionConnectionCount;
    TransactionConnectionHelper m_connectionHelper;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    void onSystemListReceived(api::ResultCode resultCode, api::SystemDataExList systems);
    void openConnections();
};

} // namespace test
} // namespace cdb
} // namespace nx
