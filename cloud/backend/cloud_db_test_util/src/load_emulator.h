#pragma once

#include <list>

#include <nx/cloud/db/api/cdb_client.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/cloud/db/test_support/transaction_connection_helper.h>

namespace nx::cloud::db::test {

class LoadTest;

class LoadEmulator
{
public:
    LoadEmulator(
        const std::string& cdbUrl,
        const std::string& accountEmail,
        const std::string& accountPassword);

    ~LoadEmulator();

    void setMaxDelayBeforeConnect(std::chrono::milliseconds delay);
    void setTransactionConnectionCount(int connectionCount);
    void setReplaceFailedConnection(bool value);

    void start();

    std::size_t activeConnectionCount() const;
    std::size_t totalFailedConnections() const;
    std::size_t connectedConnections() const;

private:
    mutable QnMutex m_mutex;
    const std::string m_cdbUrl;
    nx::utils::Url m_syncUrl;
    api::CdbClient m_cdbClient;
    api::SystemDataExList m_systems;
    std::chrono::milliseconds m_maxDelayBeforeConnect;
    int m_transactionConnectionCount = 100;
    bool m_replaceFailedConnection = true;
    std::vector<std::unique_ptr<LoadTest>> m_tests;

    void onSystemListReceived(api::ResultCode resultCode, api::SystemDataExList systems);
};

//-------------------------------------------------------------------------------------------------

class LoadTest:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    LoadTest(
        nx::utils::Url syncUrl,
        std::vector<api::SystemDataEx> systems,
        std::chrono::milliseconds maxDelayBeforeConnect,
        int connectionCount,
        bool replaceFailedConnection);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* thread) override;

    void start();

    std::size_t activeConnectionCount() const;
    std::size_t totalFailedConnections() const;
    std::size_t connectedConnections() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::utils::Url m_syncUrl;
    std::vector<api::SystemDataEx> m_systems;
    const int m_transactionConnectionCount = 0;
    const bool m_replaceFailedConnection;

    TransactionConnectionHelper m_connectionHelper;
    std::map<int /*connectionId*/, api::SystemDataEx> m_connections;
    nx::utils::SubscriptionId m_connectionFailureSubscriptionId;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    void openConnections();

    void openConnection(
        const QnMutexLockerBase&,
        const api::SystemDataEx& system);

    void handleConnectionFailure(
        int connectionId,
        ::ec2::QnTransactionTransportBase::State state);
};

} // namespace nx::cloud::db::test
