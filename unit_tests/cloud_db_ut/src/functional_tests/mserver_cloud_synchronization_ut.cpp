/**********************************************************
* 11 aug 2016
* a.kolesnikov
***********************************************************/

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>

#include <transaction/transaction_transport.h>

#include "test_setup.h"
#include "transaction_transport.h"


namespace nx {
namespace cdb {

class MserverCloudSynchronization
:
    public CdbFunctionalTest
{
public:
    MserverCloudSynchronization()
    :
        m_moduleGuid(QnUuid::createUuid()),
        m_runningInstanceGuid(QnUuid::createUuid()),
        m_transactionConnectionIdSequence(0)
    {
    }

    /** @return new connection id */
    int openTransactionConnection(
        const std::string& systemId,
        const std::string& systemAuthKey)
    {
        auto transactionConnection =
            std::make_unique<test::TransactionTransport>(
                localPeer(),
                systemId,
                systemAuthKey);
        QObject::connect(
            transactionConnection.get(), &test::TransactionTransport::stateChanged,
            transactionConnection.get(),
            [transactionConnection = transactionConnection.get(), this](
                test::TransactionTransport::State state)
            {
                onTransactionConnectionStateChanged(transactionConnection, state);
            },
            Qt::DirectConnection);

        std::lock_guard<std::mutex> lk(m_mutex);
        auto transactionConnectionPtr = transactionConnection.get();
        const int connectionId = ++m_transactionConnectionIdSequence;
        m_connections.emplace(
            connectionId,
            std::move(transactionConnection));
        const QUrl url(lit("http://%1/ec2/events").arg(endpoint().toString()));
        transactionConnectionPtr->doOutgoingConnect(url);

        return connectionId;
    }

    bool openTransactionConnectionAndWaitForResult(
        const std::string& systemId,
        const std::string& systemAuthKey,
        std::chrono::milliseconds durationToWait)
    {
        const int connectionId = openTransactionConnection(systemId, systemAuthKey);

        //waiting for connection state change
        const auto waitUntil = std::chrono::steady_clock::now() + durationToWait;
        std::unique_lock<std::mutex> lk(m_mutex);
        for (;;)
        {
            auto connectionIter = m_connections.find(connectionId);
            if (connectionIter == m_connections.end())
                return false;   //connection removed
            switch (connectionIter->second->getState())
            {
                case test::TransactionTransport::Connected:
                case test::TransactionTransport::NeedStartStreaming:
                case test::TransactionTransport::ReadyForStreaming:
                    return true;

                default:
                    if (m_condition.wait_until(lk, waitUntil) == std::cv_status::timeout)
                        return false;
            }
        }
    }

private:
    QnUuid m_moduleGuid;
    QnUuid m_runningInstanceGuid;
    std::map<int, std::unique_ptr<test::TransactionTransport>> m_connections;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<int> m_transactionConnectionIdSequence;

    ec2::ApiPeerData localPeer() const
    {
        return ec2::ApiPeerData(
            m_moduleGuid,
            m_runningInstanceGuid,
            Qn::PT_Server);
    }

    void onTransactionConnectionStateChanged(
        ec2::QnTransactionTransportBase* connection,
        ec2::QnTransactionTransportBase::State newState)
    {
        m_condition.notify_all();
    }
};

TEST_F(MserverCloudSynchronization, general)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::ok, result);

    //creating transaction connection
    ASSERT_TRUE(
        openTransactionConnectionAndWaitForResult(
            system1.id, system1.authKey, std::chrono::seconds(5)));
}

}   //namespace cdb
}   //namespace nx
