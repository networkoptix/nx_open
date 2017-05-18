#include "basic_test_fixture.h"

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_global.h>

#include <libconnection_mediator/src/mediator_service.h>

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
    m_staticMsgBody("Hello, hren!")
{
}

BasicTestFixture::~BasicTestFixture()
{
    if (m_httpClient)
        m_httpClient->pleaseStopSync();

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
    m_httpClient = std::make_unique<nx_http::AsyncClient>();
    m_httpClient->setSendTimeout(std::chrono::seconds(10));

    m_expectedMsgBody = m_staticMsgBody;
    m_httpClient->doGet(
        m_staticUrl,
        std::bind(&BasicTestFixture::onHttpRequestDone, this));
}

void BasicTestFixture::assertDataHasBeenExchangedCorrectly()
{
    const auto result = m_httpRequestResults.pop();
    ASSERT_EQ(nx_http::StatusCode::ok, result.statusCode);
    ASSERT_EQ(m_expectedMsgBody, result.msgBody);
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
}

void BasicTestFixture::onHttpRequestDone()
{
    HttpRequestResult result;
    if (m_httpClient->response())
    {
        result.statusCode = 
            (nx_http::StatusCode::Value)m_httpClient->response()->statusLine.statusCode;
        result.msgBody = m_httpClient->fetchMessageBodyBuffer();
    }

    m_httpRequestResults.push(std::move(result));
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
