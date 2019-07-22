#include <gtest/gtest.h>

#include <nx/cloud/storage/service/test_support/cloud_storage_launcher.h>
#include <nx/network/cloud/storage/service/api/client.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/storage/service/controller/storage_manager.h>
#include <nx/cloud/storage/service/http/cloud_db_authentication_manager.h>

namespace nx::cloud::storage::service::test {

namespace {

class StorageManagerStub: public AbstractStorageManager
{
public:
    void addStorage(
        const api::AddStorageRequest& request,
        nx::utils::MoveOnlyFunc<void(api::Result, api::AddStorageResponse)> handler) override
    {
        api::AddStorageResponse response;
        response.totalSpace = request.size;
        response.freeSpace = request.size;
        handler(api::Result{api::ResultCode::ok, {}}, std::move(response));
    }

    void readStorage(
        const std::string& storageId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::ReadStorageResponse)> handler) override
    {
        api::ReadStorageResponse response;
        response.id = storageId;
        handler(api::Result{api::ResultCode::ok, {}}, std::move(response));
    }

    void removeStorage(
        const std::string& /*storageId*/,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler) override
    {
        handler(api::Result{api::ResultCode::ok, {}});
    }
};

class CloudDBAuthenticationForwarderStub:
    public network::http::server::AbstractAuthenticationManager
{
public:
    CloudDBAuthenticationForwarderStub(nx::utils::SyncQueue<bool>* authenticationEvent):
        m_authenticationEvent(authenticationEvent)
    {
    }

    virtual void authenticate(
        const network::http::HttpServerConnection& /*connection*/,
        const network::http::Request& /*request*/,
        network::http::server::AuthenticationCompletionHandler completionHandler) override
    {
        m_authenticationEvent->push(true);
        completionHandler(network::http::server::SuccessfulAuthenticationResult());
    }

private:
    nx::utils::SyncQueue<bool>* m_authenticationEvent = nullptr;
};

} // namespace

class CloudStorageApi:
    public testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_storageManagerFactoryFuncBak =
            StorageManagerFactory::instance().setCustomFunc(
                [](const Settings& /*settings*/)
                {
                    return std::make_unique<StorageManagerStub>();
                });

        m_cloudDBAuthenticationFactoryFuncBak =
            http::CloudDbAuthenticationFactory::instance().setCustomFunc(
                [this](const Settings& /*settings*/)
                {
                    return std::make_unique<CloudDBAuthenticationForwarderStub>(
                        &m_authenticationEvent);
                });

        m_cloudStorage = std::make_unique<CloudStorageLauncher>();
        ASSERT_TRUE(m_cloudStorage->startAndWaitUntilStarted());

        m_cloudStorageClient = std::make_unique<api::Client>(m_cloudStorage->httpUrl());
    }

    virtual void TearDown() override
    {
        if (m_storageManagerFactoryFuncBak)
        {
            StorageManagerFactory::instance().setCustomFunc(
                std::move(m_storageManagerFactoryFuncBak));
        }

        if (m_cloudDBAuthenticationFactoryFuncBak)
        {
            http::CloudDbAuthenticationFactory::instance().setCustomFunc(
                std::move(m_cloudDBAuthenticationFactoryFuncBak));
        }
    }

    void whenAddStorage()
    {
        m_cloudStorageClient->addStorage({100},
            [this](api::Client::ResultCode resultCode, api::AddStorageResponse response)
            {
                ASSERT_EQ(100, response.totalSpace);
                m_response.push(resultCode);
            });
    }

    void whenReadStorage()
    {
        m_cloudStorageClient->readStorage(
            "any_storage_id",
            [this](api::Client::ResultCode resultCode, api::ReadStorageResponse response)
            {
                ASSERT_EQ(std::string("any_storage_id"), response.id);
                m_response.push(resultCode);
            });
    }

    void whenRemoveStorage()
    {
        m_cloudStorageClient->removeStorage(
            "any_storage_id",
            [this](api::Client::ResultCode resultCode)
            {
                m_response.push(resultCode);
            });
    }

    void thenRequestIsForwardedToCloudDb()
    {
        ASSERT_TRUE(m_authenticationEvent.pop());
    }

    void thenRequestSucceeds()
    {
        ASSERT_EQ(api::Client::ResultCode::ok, m_response.pop());
    }

private:
    std::unique_ptr<CloudStorageLauncher> m_cloudStorage;
    std::unique_ptr<api::Client> m_cloudStorageClient;
    nx::utils::SyncQueue<api::Client::ResultCode> m_response;
    StorageManagerFactory::Function m_storageManagerFactoryFuncBak;
    service::http::CloudDbAuthenticationFactory::Function m_cloudDBAuthenticationFactoryFuncBak;
    nx::utils::SyncQueue<bool> m_authenticationEvent;
};

TEST_F(CloudStorageApi, add_storage)
{
    whenAddStorage();
    thenRequestIsForwardedToCloudDb();
    thenRequestSucceeds();
}

TEST_F(CloudStorageApi, read_storage)
{
    whenReadStorage();
    thenRequestIsForwardedToCloudDb();
    thenRequestSucceeds();
}

TEST_F(CloudStorageApi, remove_storage)
{
    whenRemoveStorage();
    thenRequestIsForwardedToCloudDb();
    thenRequestSucceeds();
}

} // namespace nx::cloud::storage::service::test