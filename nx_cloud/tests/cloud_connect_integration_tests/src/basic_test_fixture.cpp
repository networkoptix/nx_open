#include "basic_test_fixture.h"

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_global.h>

#include <libconnection_mediator/src/mediator_service.h>
#include <libtraffic_relay/src/model/listening_peer_pool.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

void PredefinedCredentialsProvider::setCredentials(
    hpm::api::SystemCredentials cloudSystemCredentials)
{
    m_cloudSystemCredentials = std::move(cloudSystemCredentials);
}

boost::optional<hpm::api::SystemCredentials> 
    PredefinedCredentialsProvider::getSystemCredentials() const
{
    return m_cloudSystemCredentials;
}

//-------------------------------------------------------------------------------------------------

BasicTestFixture::BasicTestFixture():
    m_staticMsgBody("Hello, hren!"),
    m_unfinishedRequestsLeft(0)
{
}

BasicTestFixture::~BasicTestFixture()
{
    std::list<std::unique_ptr<nx_http::AsyncClient>> httpClients;
    {
        QnMutexLocker lock(&m_mutex);
        httpClients.swap(m_httpClients);
    }

    for (const auto& httpClient: httpClients)
        httpClient->pleaseStopSync();

    if (m_stunClient)
        m_stunClient->pleaseStopSync();

    m_httpServer.reset();
}

void BasicTestFixture::SetUp()
{
    ASSERT_TRUE(m_trafficRelay.startAndWaitUntilStarted());

    m_mediator.addArg("-trafficRelay/url");
    m_relayUrl = QUrl(lm("http://127.0.0.1:%1/")
        .arg(m_trafficRelay.moduleInstance()->httpEndpoints()[0].port).toQString());
    m_mediator.addArg(m_relayUrl.toString().toStdString().c_str());

    ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());

    SocketGlobals::mediatorConnector().mockupAddress(m_mediator.stunEndpoint());

    m_stunClient = std::make_shared<nx::stun::AsyncClient>();
    m_stunClient->connect(m_mediator.moduleInstance()->impl()->stunEndpoints()[0]);
}

void BasicTestFixture::startServer()
{
    m_cloudSystemCredentials = m_mediator.addRandomSystem();

    hpm::api::SystemCredentials credentials;
    credentials.systemId = m_cloudSystemCredentials.id;
    credentials.key = m_cloudSystemCredentials.authKey;
    m_credentialsProvider.setCredentials(std::move(credentials));

    startHttpServer();
}

QUrl BasicTestFixture::relayUrl() const
{
    return m_relayUrl;
}

void BasicTestFixture::assertConnectionCanBeEstablished()
{
    auto clientSocket = SocketFactory::createStreamSocket();
    ASSERT_TRUE(clientSocket->setNonBlockingMode(true));

    nx::utils::promise<SystemError::ErrorCode> done;
    clientSocket->connectAsync(
        serverSocketCloudAddress(),
        [&done](SystemError::ErrorCode sysErrorCode)
        {
            done.set_value(sysErrorCode);
        });
    const auto resultCode = done.get_future().get();
    ASSERT_EQ(SystemError::noError, resultCode);

    clientSocket->pleaseStopSync();
}

void BasicTestFixture::startExchangingFixedData()
{
    QnMutexLocker lock(&m_mutex);

    m_httpClients.push_back(std::make_unique<nx_http::AsyncClient>());
    m_httpClients.back()->setSendTimeout(std::chrono::seconds::zero());
    m_httpClients.back()->setResponseReadTimeout(std::chrono::seconds::zero());

    ++m_unfinishedRequestsLeft;

    m_expectedMsgBody = m_staticMsgBody;
    m_httpClients.back()->doGet(
        m_staticUrl,
        std::bind(&BasicTestFixture::onHttpRequestDone, this, --m_httpClients.end()));
}

void BasicTestFixture::assertDataHasBeenExchangedCorrectly()
{
    while (m_unfinishedRequestsLeft > 0)
    {
        const auto result = m_httpRequestResults.pop();
        ASSERT_EQ(nx_http::StatusCode::ok, result.statusCode);
        ASSERT_EQ(m_expectedMsgBody, result.msgBody);
        --m_unfinishedRequestsLeft;
    }
}

nx::hpm::MediatorFunctionalTest& BasicTestFixture::mediator()
{
    return m_mediator;
}

nx::cloud::relay::test::Launcher& BasicTestFixture::trafficRelay()
{
    return m_trafficRelay;
}

nx::String BasicTestFixture::serverSocketCloudAddress() const
{
    return m_cloudSystemCredentials.id;
}

void BasicTestFixture::startHttpServer()
{
    auto serverConnection =
        std::make_unique<hpm::api::MediatorServerTcpConnection>(
            m_stunClient,
            &m_credentialsProvider);

    auto cloudServerSocket =
        std::make_unique<CloudServerSocket>(std::move(serverConnection));

    m_httpServer = std::make_unique<TestHttpServer>(std::move(cloudServerSocket));
    m_httpServer->registerStaticProcessor("/static", m_staticMsgBody, "text/plain");
    m_staticUrl = QUrl(lm("http://%1/static").arg(serverSocketCloudAddress()));

    ASSERT_TRUE(m_httpServer->bindAndListen());

    // Waiting for server to be registered on relay.
    while (!m_trafficRelay.moduleInstance()->listeningPeerPool().isPeerOnline(
                m_cloudSystemCredentials.id.toStdString()))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void BasicTestFixture::onHttpRequestDone(
    std::list<std::unique_ptr<nx_http::AsyncClient>>::iterator clientIter)
{
    QnMutexLocker lock(&m_mutex);

    if (m_httpClients.empty())
        return; //< Most likely, current object is being destructed.

    auto httpClient = std::move(*clientIter);
    m_httpClients.erase(clientIter);

    HttpRequestResult result;
    if (httpClient->response())
    {
        result.statusCode = 
            (nx_http::StatusCode::Value)httpClient->response()->statusLine.statusCode;
        result.msgBody = httpClient->fetchMessageBodyBuffer();
    }

    m_httpRequestResults.push(std::move(result));
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
