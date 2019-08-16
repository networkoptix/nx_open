#pragma once

#include <list>

#include <nx/cloud/db/api/cdb_client.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/cloud/db/test_support/transaction_connection_helper.h>

namespace nx::cloud::db::test {

class LoadEmulator
{
public:
    LoadEmulator(
        const std::string& cdbUrl,
        const std::string& accountEmail,
        const std::string& accountPassword);

    void setMaxDelayBeforeConnect(std::chrono::milliseconds delay);
    void setTransactionConnectionCount(int connectionCount);
    void setReplaceFailedConnection(bool value);

    void start();

    std::size_t activeConnectionCount() const;
    std::size_t totalFailedConnections() const;
    std::size_t connectedConnections() const;

private:
    const std::string m_cdbUrl;
    nx::utils::Url m_syncUrl;
    api::CdbClient m_cdbClient;
    api::SystemDataExList m_systems;
    int m_transactionConnectionCount = 100;
    TransactionConnectionHelper m_connectionHelper;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    bool m_replaceFailedConnection = true;
    std::map<int /*connectionId*/, api::SystemDataEx> m_connections;
    nx::utils::SubscriptionId m_connectionFailureSubscriptionId;

    void onSystemListReceived(api::ResultCode resultCode, api::SystemDataExList systems);

    void openConnections();

    void openConnection(
        const QnMutexLockerBase&,
        const api::SystemDataEx& system);

    void handleConnectionFailure(
        int connectionId,
        ::ec2::QnTransactionTransportBase::State state);
};

} // namespace nx::cloud::db::test
