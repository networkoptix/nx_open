#include <future>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/connection_mediator_url_fetcher.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <nx/cloud/mediator/mediator_process_public.h>
#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/test_support/local_cloud_data_provider.h>

namespace nx {
namespace hpm {
namespace api {
namespace test {

static const char* kCloudModulesXmlPath = "/MediatorConnector/cloud_modules.xml";

template<typename CloudModuleListGenerator>
class MediatorConnector:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    MediatorConnector():
        nx::utils::test::TestWithTemporaryDirectory("mediator", QString())
    {
        m_factoryFuncToRestore =
            AbstractCloudDataProviderFactory::setFactoryFunc(
                [this](
                    const std::optional<nx::utils::Url>& /*cdbUrl*/,
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

        if (m_malfunctioningServer)
            m_malfunctioningServer->pleaseStopSync();

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

        waitForPeerToRegisterOnMediator();
        andNewMediatorEndpointIsAvailable();
    }

    void givenWrongCloudModulesXml()
    {
        m_malfunctioningServer = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
        ASSERT_TRUE(m_malfunctioningServer->setNonBlockingMode(true));
        ASSERT_TRUE(m_malfunctioningServer->bind(nx::network::SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_malfunctioningServer->listen());

        acceptAndReset();
    }

    void givenPeerFailedToConnectToMediator()
    {
        auto client = m_mediatorConnector->clientConnection();
        auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });
        ConnectRequest request;
        std::promise<ResultCode> done;
        client->connect(
            request,
            [this, &done](
                network::stun::TransportHeader /*stunTransportHeader*/,
                ResultCode resultCode,
                ConnectResponse)
            {
                done.set_value(resultCode);
            });
        const auto resultCode = done.get_future().get();
        ASSERT_EQ(ResultCode::networkError, resultCode);
    }

    void givenPeerFailedToResolveMediatorAddress()
    {
        std::promise<nx::network::http::StatusCode::Value> fetchMediatorEndpointCompleted;
        m_mediatorConnector->fetchAddress(
            [&fetchMediatorEndpointCompleted](
                nx::network::http::StatusCode::Value statusCode,
                hpm::api::MediatorAddress)
            {
                fetchMediatorEndpointCompleted.set_value(statusCode);
            });

        ASSERT_FALSE(nx::network::http::StatusCode::isSuccessCode(
            fetchMediatorEndpointCompleted.get_future().get()));
    }

    void whenMediatorUrlEndpointIsChanged()
    {
        // Launching another mediator to make sure it has different endpoint.
        decltype(m_mediator) oldMediator(m_mediator.release());
        startMediator();
    }

    void whenStartMediator()
    {
        startMediator();
    }

    void thenConnectionToMediatorIsReestablished()
    {
        waitForPeerToRegisterOnMediator();
    }

    void waitForPeerToRegisterOnMediator()
    {
        for (;;)
        {
            Client mediatorClient(nx::network::url::Builder()
                .setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(m_mediator->moduleInstance()->impl()->httpEndpoints().front())
                .setPath(api::kMediatorApiPrefix));
            const auto response = mediatorClient.getListeningPeers();
            ASSERT_EQ(api::ResultCode::ok, std::get<0>(response));
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
        while (getMediatorAddress().stunUdpEndpoint.toString() !=
            m_mediator->moduleInstance()->impl()->stunUdpEndpoints().front().toString())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    nx::utils::Url cloudModulesXmlUrl()
    {
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_cloudModulesXmlProvider.serverAddress())
            .setPath(kCloudModulesXmlPath);
    }

    api::MediatorConnector& mediatorConnector()
    {
        return *m_mediatorConnector;
    }

    hpm::api::MediatorAddress getMediatorAddress()
    {
        std::promise<hpm::api::MediatorAddress> mediatorAddressPromise;
        mediatorConnector().fetchAddress(
            [&mediatorAddressPromise](
                auto /*resultCode*/,
                hpm::api::MediatorAddress mediatorAddress)
            {
                mediatorAddressPromise.set_value(mediatorAddress);
            });

        return mediatorAddressPromise.get_future().get();
    }

private:
    using Mediator = nx::utils::test::ModuleLauncher<MediatorProcessPublic>;

    boost::optional<AbstractCloudDataProviderFactory::FactoryFunc> m_factoryFuncToRestore;
    nx::utils::AtomicUniquePtr<Mediator> m_mediator;
    std::unique_ptr<api::MediatorConnector> m_mediatorConnector;
    SystemCredentials m_cloudSystemCredentials;
    LocalCloudDataProvider m_localCloudDataProvider;
    nx::network::http::TestHttpServer m_cloudModulesXmlProvider;
    std::unique_ptr<nx::network::TCPServerSocket> m_malfunctioningServer;
    std::unique_ptr<nx::network::cloud::CloudServerSocket> m_cloudServerSocket;
    CloudModuleListGenerator m_cloudModuleListGenerator;

    virtual void SetUp() override
    {
        initializeCloudModulesXmlProvider();
        intializeMediatorConnector();
    }

    void startMediator()
    {
        auto mediator = nx::utils::make_atomic_unique<Mediator>();
        mediator->addArg("--cloud_db/runWithCloud=false");
        mediator->addArg("--http/addrToListenList=127.0.0.1:0");
        mediator->addArg("--stun/addrToListenList=127.0.0.1:0");
        mediator->addArg("-general/dataDir", testDataDir().toStdString().c_str());
        ASSERT_TRUE(mediator->startAndWaitUntilStarted());

        m_mediator = std::move(mediator);
    }

    void initializeCloudModulesXmlProvider()
    {
        m_cloudModulesXmlProvider.registerContentProvider(
            kCloudModulesXmlPath,
            std::bind(&MediatorConnector::generateCloudModuleXml, this));
        ASSERT_TRUE(m_cloudModulesXmlProvider.bindAndListen());
    }

    std::unique_ptr<nx::network::http::AbstractMsgBodySource> generateCloudModuleXml()
    {
        if (m_mediator)
        {
            return std::make_unique<nx::network::http::BufferSource>(
                "text/xml",
                m_cloudModuleListGenerator.get(m_mediator.get()));
        }

        if (m_malfunctioningServer)
        {
            return std::make_unique<nx::network::http::BufferSource>(
                "text/xml",
                m_cloudModuleListGenerator.get(
                    m_malfunctioningServer->getLocalAddress(),
                    m_malfunctioningServer->getLocalAddress()));
        }

        return nullptr;
    }

    void intializeMediatorConnector()
    {
        using namespace std::placeholders;

        m_mediatorConnector = std::make_unique<api::MediatorConnector>("127.0.0.1");
        m_mediatorConnector->mockupCloudModulesXmlUrl(cloudModulesXmlUrl());
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
        std::unique_ptr<nx::network::AbstractStreamSocket> /*streamSocket*/)
    {
        // TODO
    }

    void acceptAndReset()
    {
        m_malfunctioningServer->acceptAsync(
            [this](SystemError::ErrorCode resultCode,
                std::unique_ptr<nx::network::AbstractStreamSocket> connection)
            {
                connection.reset();
                acceptAndReset();
            });
    }
};

TYPED_TEST_CASE_P(MediatorConnector);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(
    MediatorConnector,
    reloads_cloud_modules_list_after_loosing_connection_to_mediator)
{
    this->givenPeerConnectedToMediator();

    this->whenMediatorUrlEndpointIsChanged();

    this->thenConnectionToMediatorIsReestablished();
    this->andNewMediatorEndpointIsAvailable();
}

TYPED_TEST_P(
    MediatorConnector,
    reloads_cloud_modules_list_after_each_failure_to_connect_to_mediator)
{
    this->givenWrongCloudModulesXml();
    this->givenPeerFailedToConnectToMediator();

    this->whenStartMediator();

    this->thenConnectionToMediatorIsReestablished();
    this->andNewMediatorEndpointIsAvailable();
}

TYPED_TEST_P(
    MediatorConnector,
    reloads_cloud_modules_list_after_failure_to_load_cloud_modules_list)
{
    this->givenPeerFailedToResolveMediatorAddress();
    this->whenStartMediator();

    this->thenConnectionToMediatorIsReestablished();
    this->andNewMediatorEndpointIsAvailable();
}

REGISTER_TYPED_TEST_CASE_P(MediatorConnector,
    reloads_cloud_modules_list_after_loosing_connection_to_mediator,
    reloads_cloud_modules_list_after_each_failure_to_connect_to_mediator,
    reloads_cloud_modules_list_after_failure_to_load_cloud_modules_list);

//-------------------------------------------------------------------------------------------------

class MediatorStunEndpointListGenerator
{
public:
    nx::Buffer get(
        nx::utils::test::ModuleLauncher<MediatorProcessPublic>* mediator)
    {
        nx::network::SocketAddress mediatorStunTcpEndpoint;
        nx::network::SocketAddress mediatorStunUdpEndpoint;
        if (mediator)
        {
            mediatorStunTcpEndpoint =
                mediator->moduleInstance()->impl()->stunTcpEndpoints().front();
            mediatorStunUdpEndpoint =
                mediator->moduleInstance()->impl()->stunUdpEndpoints().front();
        }
        else
        {
            mediatorStunTcpEndpoint = nx::network::SocketAddress(
                nx::network::HostAddress::localhost,
                nx::utils::random::number<int>(30000, 40000));
            mediatorStunUdpEndpoint = mediatorStunTcpEndpoint;
        }

        return get(mediatorStunTcpEndpoint, mediatorStunUdpEndpoint);
    }

    nx::Buffer get(
        const nx::network::SocketAddress& mediatorStunTcpEndpoint,
        const nx::network::SocketAddress& mediatorStunUdpEndpoint)
    {
        auto modulesXml = lm(
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            "<sequence>\r\n"
                "<sequence>\r\n"
                    "<set resName=\"hpm.tcpUrl\" resValue=\"stun://%1\"/>\r\n"
                    "<set resName=\"hpm.udpUrl\" resValue=\"stun://%2\"/>\r\n"
                "</sequence>\r\n"
            "</sequence>\r\n"
        ).args(mediatorStunTcpEndpoint, mediatorStunUdpEndpoint).toUtf8();
        return modulesXml;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    UsingMediatorStunEndpoint,
    MediatorConnector,
    MediatorStunEndpointListGenerator);

//-------------------------------------------------------------------------------------------------

class MediatorHttpEndpointListGenerator
{
public:
    nx::Buffer get(
        nx::utils::test::ModuleLauncher<MediatorProcessPublic>* mediator)
    {
        nx::network::SocketAddress mediatorStunUdpEndpoint;
        nx::network::SocketAddress mediatorHttpEndpoint;
        if (mediator)
        {
            mediatorHttpEndpoint = mediator->moduleInstance()->impl()->httpEndpoints().front();
            mediatorStunUdpEndpoint = mediator->moduleInstance()->impl()->stunUdpEndpoints().front();
        }
        else
        {
            mediatorStunUdpEndpoint = nx::network::SocketAddress(
                nx::network::HostAddress::localhost,
                nx::utils::random::number<int>(30000, 40000));
            mediatorHttpEndpoint = mediatorStunUdpEndpoint;
        }

        return get(
            mediatorHttpEndpoint,
            mediatorStunUdpEndpoint);
    }

    nx::Buffer get(
        const nx::network::SocketAddress& mediatorHttpEndpoint,
        const nx::network::SocketAddress& mediatorStunUdpEndpoint)
    {
        auto modulesXml = lm(
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            "<sequence>\r\n"
                "<sequence>\r\n"
                    "<set resName=\"hpm.tcpUrl\" resValue=\"http://%1%2\"/>\r\n"
                    "<set resName=\"hpm.udpUrl\" resValue=\"stun://%3\"/>\r\n"
                "</sequence>\r\n"
            "</sequence>\r\n"
            ).args(mediatorHttpEndpoint, kMediatorApiPrefix, mediatorStunUdpEndpoint).toUtf8();
        return modulesXml;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    UsingMediatorHttpEndpoint,
    MediatorConnector,
    MediatorHttpEndpointListGenerator);

//-------------------------------------------------------------------------------------------------

class MediatorHttpAndUdpEndpointsOnDifferentHostsListGenerator:
    public MediatorHttpEndpointListGenerator
{
    using base_type = MediatorHttpEndpointListGenerator;

public:
    MediatorHttpAndUdpEndpointsOnDifferentHostsListGenerator()
    {
        m_httpHost = nx::utils::generateRandomName(7).toLower().toStdString();
        m_stunHost = nx::utils::generateRandomName(7).toLower().toStdString();

        nx::network::SocketGlobals::addressResolver().dnsResolver().addEtcHost(
            m_httpHost.c_str(), { nx::network::HostAddress::localhost });
        nx::network::SocketGlobals::addressResolver().dnsResolver().addEtcHost(
            m_stunHost.c_str(), { nx::network::HostAddress::localhost });
    }

    ~MediatorHttpAndUdpEndpointsOnDifferentHostsListGenerator()
    {
        nx::network::SocketGlobals::addressResolver().dnsResolver().removeEtcHost(
            m_stunHost.c_str());
        nx::network::SocketGlobals::addressResolver().dnsResolver().removeEtcHost(
            m_httpHost.c_str());
    }

    nx::Buffer get(
        nx::utils::test::ModuleLauncher<MediatorProcessPublic>* mediator)
    {
        auto mediatorHttpEndpoint =
            mediator->moduleInstance()->impl()->httpEndpoints().front();
        auto mediatorStunEndpoint =
            mediator->moduleInstance()->impl()->stunUdpEndpoints().front();

        return base_type::get(
            nx::network::SocketAddress(m_httpHost, mediatorHttpEndpoint.port),
            nx::network::SocketAddress(m_stunHost, mediatorStunEndpoint.port));
    }

    nx::Buffer get(
        const nx::network::SocketAddress& mediatorHttpEndpoint,
        const nx::network::SocketAddress& mediatorStunUdpEndpoint)
    {
        return base_type::get(mediatorHttpEndpoint, mediatorStunUdpEndpoint);
    }

private:
    std::string m_httpHost;
    std::string m_stunHost;
};

class MediatorConnectorAndStunOverHttpEndpoint:
    public MediatorConnector<MediatorHttpAndUdpEndpointsOnDifferentHostsListGenerator>
{
protected:
    void givenPeerConnectedToMediator()
    {
        whenStartMediator();

        waitForPeerToRegisterOnMediator();
    }

    void assertMediatorUdpHostMatchesThatInCloudModulesXml()
    {
        fetchMediatorUrls();

        ASSERT_EQ(
            nx::network::url::getEndpoint(m_udpUrl),
            getMediatorAddress().stunUdpEndpoint);
    }

private:
    nx::utils::Url m_tcpUrl;
    nx::utils::Url m_udpUrl;

    void fetchMediatorUrls()
    {
        nx::network::cloud::ConnectionMediatorUrlFetcher urlFetcher;
        urlFetcher.setModulesXmlUrl(cloudModulesXmlUrl());
        std::promise<std::tuple<
            nx::network::http::StatusCode::Value,
            nx::utils::Url,
            nx::utils::Url>> done;
        urlFetcher.get(
            [&done](
                nx::network::http::StatusCode::Value resultCode,
                nx::utils::Url tcpUrl,
                nx::utils::Url udpUrl)
            {
                done.set_value(std::make_tuple(resultCode, tcpUrl, udpUrl));
            });
        auto result = done.get_future().get();
        ASSERT_EQ(nx::network::http::StatusCode::ok, std::get<0>(result));

        m_tcpUrl = std::get<1>(result);
        m_udpUrl = std::get<2>(result);
    }
};

TEST_F(
    MediatorConnectorAndStunOverHttpEndpoint,
    correct_udp_endpoint_is_reported)
{
    givenPeerConnectedToMediator();
    assertMediatorUdpHostMatchesThatInCloudModulesXml();
}

} // namespace test
} // namespace api
} // namespace hpm
} // namespace nx
