#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include <libconnection_mediator/src/mediator_process_public.h>
#include <libconnection_mediator/src/mediator_service.h>
#include <libconnection_mediator/src/test_support/local_cloud_data_provider.h>

namespace nx {
namespace hpm {
namespace api {
namespace test {

static const char* kCloudModulesXmlPath = "/MediatorConnector/cloud_modules.xml";

class MediatorConnector:
    public ::testing::Test
{
public:
    MediatorConnector()
    {
        m_factoryFuncToRestore =
            AbstractCloudDataProviderFactory::setFactoryFunc(
                [this](
                    const boost::optional<QUrl>& /*cdbUrl*/,
                    const std::string& /*user*/,
                    const std::string& /*password*/,
                    std::chrono::milliseconds /*updateInterval*/,
                    std::chrono::milliseconds /*startTimeout*/)
                {
                    return std::make_unique<CloudDataProviderStub>(
                        &m_localCloudDataProvider);
                });

        m_cloudSystemCredentials.systemId = QnUuid::createUuid().toSimpleByteArray();
        m_cloudSystemCredentials.key = QnUuid::createUuid().toSimpleByteArray();
        m_cloudSystemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();
        m_localCloudDataProvider.addSystem(
            m_cloudSystemCredentials.systemId,
            AbstractCloudDataProvider::System(
                m_cloudSystemCredentials.systemId,
                m_cloudSystemCredentials.key,
                true));
    }

    ~MediatorConnector()
    {
        if (m_cloudServerSocket)
            m_cloudServerSocket->pleaseStopSync();

        if (m_factoryFuncToRestore)
        {
            AbstractCloudDataProviderFactory::setFactoryFunc(
                std::move(*m_factoryFuncToRestore));
            m_factoryFuncToRestore.reset();
        }
    }

protected:
    void givenPeerConnectedToMediator()
    {
        startMediator();
        m_mediatorConnector->enable(true /*wait for completion*/);

        waitForPeerToRegisterOnMediator();
        andNewMediatorEndpointIsAvailable();
    }

    void givenPeerFailedToConnectToMediator()
    {
        m_mediatorConnector->enable(true /*wait for completion*/);
    }

    void whenMediatorUrlEndpointIsChanged()
    {
        // Launching another mediator to make sure it has different endpoint.
        decltype(m_mediator) oldMediator;
        oldMediator.swap(m_mediator);
        startMediator();
    }

    void whenStartMediator()
    {
        startMediator();
    }

    void thenConnectionToMediatorIsReestablished()
    {
        waitForPeerToRegisterOnMediator();

        while (!m_mediatorConnector->isConnected())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void waitForPeerToRegisterOnMediator()
    {
        for (;;)
        {
            Client mediatorClient(nx::network::url::Builder()
                .setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(m_mediator->moduleInstance()->impl()->httpEndpoints().front()));
            const auto response = mediatorClient.getListeningPeers();
            ASSERT_EQ(nx_http::StatusCode::ok, std::get<0>(response));
            const auto listeningPeers = std::get<1>(response);
            if (listeningPeers.systems.find(QString::fromUtf8(m_cloudSystemCredentials.systemId)) !=
                listeningPeers.systems.end())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void andNewMediatorEndpointIsAvailable()
    {
        ASSERT_EQ(
            m_mediator->moduleInstance()->impl()->stunEndpoints().front(),
            *m_mediatorConnector->udpEndpoint());
    }

private:
    using Mediator = nx::utils::test::ModuleLauncher<MediatorProcessPublic>;

    boost::optional<AbstractCloudDataProviderFactory::FactoryFunc> m_factoryFuncToRestore;
    std::unique_ptr<Mediator> m_mediator;
    std::unique_ptr<api::MediatorConnector> m_mediatorConnector;
    SystemCredentials m_cloudSystemCredentials;
    LocalCloudDataProvider m_localCloudDataProvider;
    TestHttpServer m_cloudModulesXmlProvider;
    std::unique_ptr<nx::network::cloud::CloudServerSocket> m_cloudServerSocket;

    virtual void SetUp() override
    {
        initializeCloudModulesXmlProvider();
        intializeMediatorConnector();
    }

    void startMediator()
    {
        m_mediator = std::make_unique<Mediator>();
        m_mediator->addArg("--cloud_db/runWithCloud=false");
        m_mediator->addArg("--http/addrToListenList=127.0.0.1:0");
        m_mediator->addArg("--stun/addrToListenList=127.0.0.1:0");
        ASSERT_TRUE(m_mediator->startAndWaitUntilStarted());
    }

    void initializeCloudModulesXmlProvider()
    {
        m_cloudModulesXmlProvider.registerContentProvider(
            kCloudModulesXmlPath,
            std::bind(&MediatorConnector::generateCloudModuleXml, this));
        ASSERT_TRUE(m_cloudModulesXmlProvider.bindAndListen());
    }

    std::unique_ptr<nx_http::AbstractMsgBodySource> generateCloudModuleXml()
    {
        SocketAddress mediatorStunEndpoint;
        if (m_mediator)
        {
            mediatorStunEndpoint =
                m_mediator->moduleInstance()->impl()->stunEndpoints().front();
        }
        else
        {
            mediatorStunEndpoint = SocketAddress(
                HostAddress::localhost,
                nx::utils::random::number<int>(30000, 40000));
        }

        auto modulesXml = lm(
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            "<sequence>\r\n"
                "<set resName=\"hpm\" resValue=\"stun://%1\"/>\r\n"
            "</sequence>\r\n").args(mediatorStunEndpoint).toUtf8();
        return std::make_unique<nx_http::BufferSource>(
            "text/xml",
            std::move(modulesXml));
    }

    void intializeMediatorConnector()
    {
        using namespace std::placeholders;

        auto mediatorUrlFetcher =
            std::make_unique<nx::network::cloud::ConnectionMediatorUrlFetcher>();
        mediatorUrlFetcher->setModulesXmlUrl(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(m_cloudModulesXmlProvider.serverAddress())
                .setPath(kCloudModulesXmlPath));
        m_mediatorConnector =
            std::make_unique<api::MediatorConnector>(std::move(mediatorUrlFetcher));
        m_mediatorConnector->setSystemCredentials(m_cloudSystemCredentials);

        // Using cloud server socket to register peer on mediator.
        m_cloudServerSocket = std::make_unique<nx::network::cloud::CloudServerSocket>(
            m_mediatorConnector.get());
        ASSERT_TRUE(m_cloudServerSocket->listen(0));
        m_cloudServerSocket->acceptAsync(
            std::bind(&MediatorConnector::onSocketAccepted, this, _1, _2));
    }

    void onSocketAccepted(
        SystemError::ErrorCode /*systemErrorCode*/,
        std::unique_ptr<AbstractStreamSocket> /*streamSocket*/)
    {
        // TODO
    }
};

TEST_F(MediatorConnector, DISABLED_reloads_cloud_modules_list_after_loosing_connection_to_mediator)
{
    givenPeerConnectedToMediator();

    whenMediatorUrlEndpointIsChanged();

    thenConnectionToMediatorIsReestablished();
    andNewMediatorEndpointIsAvailable();
}

TEST_F(MediatorConnector, DISABLED_reloads_cloud_modules_list_after_each_failure_to_connect_to_mediator)
{
    givenPeerFailedToConnectToMediator();
    whenStartMediator();

    thenConnectionToMediatorIsReestablished();
    andNewMediatorEndpointIsAvailable();
}

} // namespace test
} // namespace api
} // namespace hpm
} // namespace nx
