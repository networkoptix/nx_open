// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <chrono>
#include <future>
#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <api/http_client_pool.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_factory.h>
#include <nx/network/ssl/certificate.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::common::test {

using namespace std::chrono_literals;

namespace {

static constexpr auto kWaitTimeout = 10s;
static const std::string kCameraHistoryPath = "/ec2/cameraHistory";

class AcceptAnyCertificateVerifier: public AbstractCertificateVerifier
{
public:
    virtual nx::network::ssl::AdapterFunc makeAdapterFunc(
        const nx::Uuid& /*serverId*/, const nx::Url& /*url*/) override
    {
        return nx::network::ssl::kAcceptAnyCertificate;
    }
};

/** Sends the history requests via the given connection instead of the connected server one. */
class TestCameraHistoryPool: public QnCameraHistoryPool
{
public:
    TestCameraHistoryPool(QnResourcePool* resourcePool, rest::ServerConnectionPtr connection):
        QnCameraHistoryPool(resourcePool),
        m_connection(std::move(connection))
    {
    }

protected:
    virtual rest::ServerConnectionPtr connectedServerApi() const override { return m_connection; }

private:
    const rest::ServerConnectionPtr m_connection;
};

} // namespace

/**
 * The history request callbacks are delivered in the thread of the pool. The test thread does not
 * run its event loop, like the server thread which has stopped, so the callbacks never come.
 */
class CameraHistoryPoolTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        // The server connection always uses HTTPS.
        nx::network::ssl::useRandomCertificate("CameraHistoryPoolTest");

        ASSERT_TRUE(m_httpServer.registerRequestProcessorFunc(kCameraHistoryPath,
            [this](nx::network::http::RequestContext /*request*/,
                nx::network::http::RequestProcessedHandler handler)
            {
                if (++m_requestCount == 1)
                    m_requestReceived.set_value();
                handler(nx::network::http::StatusCode::notFound);
            }));
        ASSERT_TRUE(m_httpServer.bindAndListen());

        auto connection = std::make_shared<rest::ServerConnection>(&m_httpClientPool,
            /*serverId*/ nx::Uuid::createUuid(),
            /*auditId*/ nx::Uuid::createUuid(),
            &m_certificateVerifier,
            m_httpServer.serverAddress(),
            nx::network::http::PasswordCredentials("user", "password"));
        m_historyPool = std::make_unique<TestCameraHistoryPool>(
            m_context.systemContext()->resourcePool(), std::move(connection));

        // The archive on several servers makes the pool request the history.
        m_camera = m_resources.addCamera();
        for (int i = 0; i < 2; ++i)
        {
            const auto server = m_resources.addServer();
            server->setStatus(nx::vms::api::ResourceStatus::online);
            m_historyPool->setServerFootageData(server->getId(), {m_camera->getId()});
        }
    }

    virtual void TearDown() override
    {
        // Delivers the request callbacks, so a waiting thread is not left behind on a failure.
        while (m_syncUpdate.valid() && m_syncUpdate.wait_for(10ms) != std::future_status::ready)
            QCoreApplication::processEvents();

        m_historyPool.reset();
        m_resources.clear();
    }

    void whenSyncUpdateIsWaitingForRequest()
    {
        m_syncUpdate = std::async(std::launch::async,
            [this]() { return m_historyPool->updateCameraHistorySync(m_camera); });

        // The sync update holds the mutex from before sending the request until it waits, so
        // pleaseStop() called after that runs once the waiting has started.
        ASSERT_EQ(
            std::future_status::ready, m_requestReceived.get_future().wait_for(kWaitTimeout));
    }

protected:
    ApplicationContext m_appContext{nx::vms::api::PeerType::server};
    Context m_context;
    QnResourcePoolTestHelper m_resources{m_context.systemContext()};
    nx::network::http::TestHttpServer m_httpServer;
    nx::network::http::ClientPool m_httpClientPool;
    AcceptAnyCertificateVerifier m_certificateVerifier;
    std::unique_ptr<TestCameraHistoryPool> m_historyPool;
    QnVirtualCameraResourcePtr m_camera;
    std::atomic<int> m_requestCount = 0;
    std::promise<void> m_requestReceived;
    std::future<bool> m_syncUpdate;
};

TEST_F(CameraHistoryPoolTest, pleaseStopInterruptsWaitingSyncUpdate)
{
    ASSERT_NO_FATAL_FAILURE(whenSyncUpdateIsWaitingForRequest());

    m_historyPool->pleaseStop();

    ASSERT_EQ(std::future_status::ready, m_syncUpdate.wait_for(kWaitTimeout));
    EXPECT_FALSE(m_syncUpdate.get());

    // The interrupted request is still running, but the next calls do not wait for it.
    EXPECT_FALSE(m_historyPool->updateCameraHistorySync(m_camera));
    EXPECT_EQ(1, m_requestCount);
}

TEST_F(CameraHistoryPoolTest, syncUpdateDoesNotSendRequestAfterPleaseStop)
{
    m_historyPool->pleaseStop();

    EXPECT_FALSE(m_historyPool->updateCameraHistorySync(m_camera));
    EXPECT_EQ(0, m_requestCount);
}

} // namespace nx::vms::common::test
