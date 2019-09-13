#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/statistics_provider.h>

#include "basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class Https:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    ~Https()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
        if (m_httpClient)
            m_httpClient->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        m_certificateFilePath =
			lm("%1/%2").args(testDataDir(), "traffic_relay.cert").toStdString();

        ASSERT_TRUE(nx::network::ssl::Engine::useOrCreateCertificate(
            m_certificateFilePath.c_str(),
            "traffic_relay/https test", "US", "Nx"));

        addRelayInstance({
            "--https/listenOn=0.0.0.0:0",
			"--https/certificateMonitorTimeout=20ms",
            "-https/certificatePath",
			m_certificateFilePath.c_str()});
    }

    nx::utils::Url basicHttpsUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setHost("127.0.0.1").setPort(relay().moduleInstance()->httpsEndpoints()[0].port)
            .toUrl();
    }

    void whenEstablishSecureConnection()
    {
        m_connection = std::make_unique<network::ssl::ClientStreamSocket>(
            std::make_unique<network::TCPSocket>(AF_INET));
        ASSERT_TRUE(m_connection->setNonBlockingMode(true));

        m_connection->connectAsync(
            network::url::getEndpoint(basicHttpsUrl()),
            std::bind(&Https::saveConnectResult, this, std::placeholders::_1));
    }

    void whenPerformApiCallUsingHttps()
    {
        using namespace std::placeholders;

        m_httpClient = std::make_unique<GetStatisticsHttpClient>(
            nx::network::url::Builder(basicHttpsUrl())
            .setPath(api::kRelayStatisticsMetricsPath).toUrl(),
            nx::network::http::AuthInfo());
        m_httpClient->execute(
            std::bind(&Https::saveStatisticsRequestResult, this, _1, _2, _3));
    }

	void whenCertificateFileIsModified()
	{
		relay().moduleInstance()->setOnStartedEventHandler(
			[this](bool isStarted) { m_relayRestartedEvent.push(isStarted); });

		// Sleep is needed to allow SslCertificateWatcher to fully initialize.
		// Otherwise, it may initialize with the contents of the modified file,
		// and detection will fail.
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		modifyCertificateFile();
	}

	void thenRelayIsRestarted()
	{
		ASSERT_TRUE(m_relayRestartedEvent.pop());
	}

    void thenConnectionIsEstablished()
    {
        ASSERT_EQ(SystemError::noError, m_connectResultQueue.pop());
    }

    void thenApiCallSucceeded()
    {
        ASSERT_EQ(api::ResultCode::ok, m_apiResponseQueue.pop());
    }

private:
    using GetStatisticsHttpClient =
        nx::network::http::FusionDataHttpClient<void, nx::cloud::relay::Statistics>;

    std::unique_ptr<GetStatisticsHttpClient> m_httpClient;
    std::unique_ptr<network::ssl::ClientStreamSocket> m_connection;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResultQueue;
    nx::utils::SyncQueue<api::ResultCode> m_apiResponseQueue;
	std::string m_certificateFilePath;
	nx::utils::SyncQueue<bool> m_relayRestartedEvent;

    void saveConnectResult(SystemError::ErrorCode systemErrorCode)
    {
        m_connectResultQueue.push(systemErrorCode);
    }

    void saveStatisticsRequestResult(
        SystemError::ErrorCode /*systemErrorCode*/,
        const nx::network::http::Response* response,
        nx::cloud::relay::Statistics /*statistics*/)
    {
        m_apiResponseQueue.push(
            response
            ? api::fromHttpStatusCode(static_cast<nx::network::http::StatusCode::Value>(
                response->statusLine.statusCode))
            : api::ResultCode::networkError);
    }

	void modifyCertificateFile()
	{
		NX_DEBUG(this, "Modifying certificate file");
		std::ofstream file(m_certificateFilePath, std::ios::app);
		file << ' ';
		file.close();
	}
};

TEST_F(Https, ssl_connections_are_accepted_as_configured)
{
    whenEstablishSecureConnection();
    thenConnectionIsEstablished();
}

TEST_F(Https, http_api_is_available_via_ssl)
{
    whenPerformApiCallUsingHttps();
    thenApiCallSucceeded();
}

TEST_F(Https, relay_restarts_if_certificate_file_is_modified)
{
	whenCertificateFileIsModified();
	thenRelayIsRestarted();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
