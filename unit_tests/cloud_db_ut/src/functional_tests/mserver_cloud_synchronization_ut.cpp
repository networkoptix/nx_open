/**********************************************************
* 11 aug 2016
* a.kolesnikov
***********************************************************/

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include <transaction/transaction_transport.h>

#include "ec2/appserver2_process.h"
#include "test_setup.h"
#include "transaction_transport.h"


namespace nx {
namespace cdb {

class Ec2MserverCloudSynchronization
:
    public CdbFunctionalTest
{
public:
    Ec2MserverCloudSynchronization()
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

TEST_F(Ec2MserverCloudSynchronization, establishConnection)
{
    constexpr static const auto kWaitTimeout = std::chrono::seconds(5);

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
            system1.id, system1.authKey, kWaitTimeout));
}

class Ec2MserverCloudSynchronization2
    :
    public ::testing::Test
{
public:
    Ec2MserverCloudSynchronization2()
    {
        const auto tmpDir = 
            CdbLauncher::temporaryDirectoryPath().isEmpty()
            ? QDir::homePath()
            : CdbLauncher::temporaryDirectoryPath() + "/ec2_cloud_sync_ut.data";

        const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
        m_appserver2.addArg(dbFileArg.toStdString().c_str());
    }

    ~Ec2MserverCloudSynchronization2()
    {
        m_appserver2.stop();
        m_cdb.stop();
    }

    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>* appserver2()
    {
        return &m_appserver2;
    }

    CdbLauncher* cdb()
    {
        return &m_cdb;
    }

    const CdbLauncher* cdb() const
    {
        return &m_cdb;
    }

    api::ResultCode bindRandomSystem()
    {
        api::ResultCode result = m_cdb.addActivatedAccount(&m_account, &m_accountPassword);
        if (result != api::ResultCode::ok)
            return result;

        //adding system1 to account1
        result = m_cdb.bindRandomSystem(m_account.email, m_accountPassword, &m_system);
        if (result != api::ResultCode::ok)
            return result;

        //saving cloud credentials to vms db
        ec2::ApiUserDataList users;
        if (m_appserver2.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)
                ->getUsersSync(&users) != ec2::ErrorCode::ok)
        {
            return api::ResultCode::unknownError;
        }

        QnUuid adminUserId;
        for (const auto& user: users)
        {
            if (user.name == "admin")
                adminUserId = user.id;
        }
        if (adminUserId.isNull())
            return api::ResultCode::notFound;

        ec2::ApiResourceParamWithRefDataList params;
        params.emplace_back(ec2::ApiResourceParamWithRefData(
            adminUserId,
            "cloudSystemID",
            QString::fromStdString(m_system.id)));
        params.emplace_back(ec2::ApiResourceParamWithRefData(
            adminUserId,
            "cloudAuthKey",
            QString::fromStdString(m_system.authKey)));
        ec2::ApiResourceParamWithRefDataList outParams;
        if (m_appserver2.moduleInstance()->ecConnection()->getResourceManager(Qn::kSystemAccess)
                ->saveSync(params, &outParams) != ec2::ErrorCode::ok)
        {
            return api::ResultCode::unknownError;
        }

        return api::ResultCode::ok;
    }

    const api::SystemData& registeredSystemData() const
    {
        return m_system;
    }

    QUrl cdbEc2TransactionUrl() const
    {
        QUrl url(lit("http://%1/").arg(cdb()->endpoint().toString()));
        url.setUserName(QString::fromStdString(m_system.id));
        url.setPassword(QString::fromStdString(m_system.authKey));
        return url;
    }

private:
    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic> m_appserver2;
    CdbLauncher m_cdb;
    api::AccountData m_account;
    std::string m_accountPassword;
    api::SystemData m_system;
};

TEST_F(Ec2MserverCloudSynchronization2, general)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

    std::this_thread::sleep_for(std::chrono::minutes(100));
}

}   //namespace cdb
}   //namespace nx
