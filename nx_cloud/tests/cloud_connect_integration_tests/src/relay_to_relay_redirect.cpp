#include <vector>
#include <memory>
#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>
#include <http/http_api_path.h>
#include <model/listening_peer_pool.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <libtraffic_relay/src/test_support/traffic_relay_launcher.h>


namespace nx {
namespace network {
namespace cloud {
namespace test {


using Relay = nx::cloud::relay::test::Launcher;
using RelayPtr = std::unique_ptr<Relay>;
using RelayPtrList = std::vector<RelayPtr>;


static const char* const kCloudModulesXmlPath = "/cloud_modules.xml";


class RelayToRelayRedirect: public ::testing::Test
{
public:
    RelayToRelayRedirect():
        m_mediator(
            nx::hpm::MediatorFunctionalTest::allFlags &
            ~nx::hpm::MediatorFunctionalTest::initializeConnectivity)
    {}

    virtual void SetUp() override
    {
        ConnectorFactory::setEnabledCloudConnectMask((int)CloudConnectType::proxy);
        startRelays();
        startMediator();
        startXmlProvider();
        startServer();
    }

private:
    RelayPtrList m_relays;
    nx::hpm::MediatorFunctionalTest m_mediator;
    TestHttpServer m_cloudModulesXmlProvider;
    hpm::api::SystemCredentials m_cloudSystemCredentials;
    std::unique_ptr<TestHttpServer> m_httpServer;
    const nx_http::BufferType m_staticMsgBody = "Some body";
    QUrl m_staticUrl;

    void startRelays()
    {
        for (int i = 0; i < 3; ++i)
            startRelay(20000 + i);
    }

    void startRelay(int port)
    {
        auto newRelay = std::make_unique<Relay>();

        std::string hostString = std::string("0.0.0.0:") + std::to_string(port);
        newRelay->addArg("-http/listenOn", hostString.c_str());

        std::string dataDirString = std::string("relay_") + std::to_string(port);
        newRelay->addArg("-dataDir", dataDirString.c_str());

        ASSERT_TRUE(newRelay->startAndWaitUntilStarted());
        m_relays.push_back(std::move(newRelay));
    }

    void initializeCloudModulesXmlWithStunOverHttp()
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
                .arg(m_mediator.httpEndpoint())
                .arg(nx::hpm::http::kStunOverHttpTunnelPath)
                .arg(m_mediator.stunEndpoint()).toUtf8(),
            "application/xml");
    }

    void startMediator()
    {
        m_mediator.addArg("-trafficRelay/url", "http://127.0.0.1:20000/");
        m_mediator.addArg("-dataDir", "mediator");

        ASSERT_TRUE(m_mediator.startAndWaitUntilStarted());
    }

    void startXmlProvider()
    {
        ASSERT_TRUE(m_cloudModulesXmlProvider.bindAndListen());

        initializeCloudModulesXmlWithStunOverHttp();

        SocketGlobals::mediatorConnector().mockupCloudModulesXmlUrl(
            nx::network::url::Builder().setScheme("http")
            .setEndpoint(m_cloudModulesXmlProvider.serverAddress())
            .setPath(kCloudModulesXmlPath));
        SocketGlobals::mediatorConnector().enable(true);
    }

    void startServer()
    {
        auto cloudSystemCredentials = m_mediator.addRandomSystem();

        m_cloudSystemCredentials.systemId = cloudSystemCredentials.id;
        m_cloudSystemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();
        m_cloudSystemCredentials.key = cloudSystemCredentials.authKey;

        SocketGlobals::mediatorConnector().setSystemCredentials(m_cloudSystemCredentials);

        auto cloudServerSocket = std::make_unique<CloudServerSocket>(
            &SocketGlobals::mediatorConnector());

        m_httpServer = std::make_unique<TestHttpServer>(std::move(cloudServerSocket));
        m_httpServer->registerStaticProcessor("/static", m_staticMsgBody, "text/plain");
        m_staticUrl = QUrl(lm("http://%1/static").arg(m_cloudSystemCredentials.systemId));

        ASSERT_TRUE(m_httpServer->bindAndListen());

        while (!m_relays[0]->moduleInstance()->listeningPeerPool().isPeerOnline(
            m_cloudSystemCredentials.systemId.toStdString()))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        qWarning() << "Server" << m_cloudSystemCredentials.systemId << "has registered on relay";
    }
};

TEST_F(RelayToRelayRedirect, RedirectToAnotherRelay)
{
    std::this_thread::sleep_for(std::chrono::minutes(10));
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx