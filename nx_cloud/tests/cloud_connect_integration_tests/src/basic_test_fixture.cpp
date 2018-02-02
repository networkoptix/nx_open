#include "basic_test_fixture.h"

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>

#include <libconnection_mediator/src/listening_peer_pool.h>
#include <libconnection_mediator/src/mediator_service.h>
#include <libtraffic_relay/src/model/listening_peer_pool.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

static const char* const kCloudModulesXmlPath = "/cloud_modules.xml";

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
    m_mediator(
        nx::hpm::MediatorFunctionalTest::allFlags &
            ~nx::hpm::MediatorFunctionalTest::initializeConnectivity),
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

    m_httpServer.reset();
}

void BasicTestFixture::SetUp()
{
    ASSERT_TRUE(m_trafficRelay.startAndWaitUntilStarted());

    m_mediator.addArg("-trafficRelay/url");
    m_relayUrl = QUrl(lm("http://127.0.0.1:%1/")
        .arg(m_trafficRelay.moduleInstance()->httpEndpoints()[0].port).toQString());
    m_mediator.addArg(m_relayUrl.toString().toStdString().c_str());
    m_mediator.addArg("-stun/addrToListenList", "127.0.0.1:0");
    m_mediator.addArg("-http/addrToListenList", "127.0.0.1:0");

    ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());
    ASSERT_TRUE(m_cloudModulesXmlProvider.bindAndListen());

    switch (m_mediatorApiProtocol)
    {
        case MediatorApiProtocol::stun:
            initializeCloudModulesXmlWithDirectStunPort();
            break;

        case MediatorApiProtocol::http:
            initializeCloudModulesXmlWithStunOverHttp();
            break;
    }

    SocketGlobals::mediatorConnector().mockupCloudModulesXmlUrl(
        nx::network::url::Builder().setScheme("http")
            .setEndpoint(m_cloudModulesXmlProvider.serverAddress())
            .setPath(kCloudModulesXmlPath));
    SocketGlobals::mediatorConnector().enable(true);
}

void BasicTestFixture::startServer()
{
    auto cloudSystemCredentials = m_mediator.addRandomSystem();

    m_cloudSystemCredentials.systemId = cloudSystemCredentials.id;
    m_cloudSystemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();
    m_cloudSystemCredentials.key = cloudSystemCredentials.authKey;

    SocketGlobals::mediatorConnector().setSystemCredentials(m_cloudSystemCredentials);

    startHttpServer();
}

QUrl BasicTestFixture::relayUrl() const
{
    return m_relayUrl;
}

void BasicTestFixture::restartMediator()
{
    m_mediator.restart();
}

void BasicTestFixture::assertConnectionCanBeEstablished()
{
    m_clientSocket = SocketFactory::createStreamSocket();
    auto clientSocketGuard = makeScopeGuard([this]() { m_clientSocket->pleaseStopSync(); });
    ASSERT_TRUE(m_clientSocket->setNonBlockingMode(true));

    nx::String targetAddress = 
        m_remotePeerName
        ? *m_remotePeerName
        : serverSocketCloudAddress();

    nx::utils::promise<SystemError::ErrorCode> done;
    m_clientSocket->connectAsync(
        targetAddress,
        [&done](SystemError::ErrorCode sysErrorCode)
        {
            done.set_value(sysErrorCode);
        });
    const auto resultCode = done.get_future().get();
    ASSERT_EQ(SystemError::noError, resultCode);
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
    return m_cloudSystemCredentials.systemId;
}

const hpm::api::SystemCredentials& BasicTestFixture::cloudSystemCredentials() const
{
    return m_cloudSystemCredentials;
}

void BasicTestFixture::waitUntilServerIsRegisteredOnMediator()
{
    for (;;)
    {
        if (!m_mediator.moduleInstance()->impl()->listeningPeerPool()->findPeersBySystemId(
                m_cloudSystemCredentials.systemId).empty())
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void BasicTestFixture::waitUntilServerIsRegisteredOnTrafficRelay()
{
    while (!m_trafficRelay.moduleInstance()->listeningPeerPool().isPeerOnline(
        m_cloudSystemCredentials.systemId.toStdString()))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void BasicTestFixture::setRemotePeerName(const nx::String& remotePeerName)
{
    m_remotePeerName = remotePeerName;
}

void BasicTestFixture::setMediatorApiProtocol(MediatorApiProtocol mediatorApiProtocol)
{
    m_mediatorApiProtocol = mediatorApiProtocol;
}

const std::unique_ptr<AbstractStreamSocket>& BasicTestFixture::clientSocket()
{
    return m_clientSocket;
}

void BasicTestFixture::initializeCloudModulesXmlWithDirectStunPort()
{
    static const char* const kCloudModulesXmlTemplate = R"xml(
        <sequence>
            <set resName="hpm" resValue="stun://%1"/>
        </sequence>
    )xml";

    m_cloudModulesXmlProvider.registerStaticProcessor(
        kCloudModulesXmlPath,
        lm(kCloudModulesXmlTemplate).arg(m_mediator.stunEndpoint()).toUtf8(),
        "application/xml");
}

void BasicTestFixture::initializeCloudModulesXmlWithStunOverHttp()
{
    static const char* kCloudModulesXmlTemplate = R"xml(
        <sequence>
            <sequence>
                <set resName="hpm.tcpUrl" resValue="http://%1%2"/>
                <set resName="hpm.udpUrl" resValue="stun://%3"/>
            </sequence>
        </sequence>
    )xml";

    m_cloudModulesXmlProvider.registerStaticProcessor(
        kCloudModulesXmlPath,
        lm(kCloudModulesXmlTemplate)
            .arg(m_mediator.httpEndpoint()).arg(nx::hpm::api::kMediatorApiPrefix)
            .arg(m_mediator.stunEndpoint()).toUtf8(),
        "application/xml");
}

void BasicTestFixture::startHttpServer()
{
    auto cloudServerSocket = std::make_unique<CloudServerSocket>(
        &SocketGlobals::mediatorConnector());

    m_httpServer = std::make_unique<TestHttpServer>(std::move(cloudServerSocket));
    m_httpServer->registerStaticProcessor("/static", m_staticMsgBody, "text/plain");
    m_staticUrl = QUrl(lm("http://%1/static").arg(serverSocketCloudAddress()));

    ASSERT_TRUE(m_httpServer->bindAndListen());

    waitUntilServerIsRegisteredOnTrafficRelay();
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
