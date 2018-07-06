#include <gtest/gtest.h>

#include <nx/network/cloud/connection_mediator_url_fetcher.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/client/cdb_request_path.h>

#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class ModuleDiscovery:
    public CdbFunctionalTest
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void whenRequestNewModuleXml()
    {
        using namespace std::placeholders;

        m_urlFetcher.setModulesXmlUrl(nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(endpoint())
            .setPath(kDiscoveryCloudModuleXmlPath));

        m_urlFetcher.get(std::bind(&ModuleDiscovery::saveMediatorUrls, this, _1, _2, _3));
    }

    void thenXmlIsProvided()
    {
        m_prevResult = m_results.pop();
        ASSERT_EQ(nx::network::http::StatusCode::ok, m_prevResult.resultCode);
    }

    void andXmlContainsMultipleMediatorPorts()
    {
        ASSERT_NE(m_prevResult.tcpUrl, m_prevResult.udpUrl);
        ASSERT_EQ(nx::network::http::kUrlSchemeName, m_prevResult.tcpUrl.scheme());
        ASSERT_EQ(nx::network::stun::kUrlSchemeName, m_prevResult.udpUrl.scheme());
    }

private:
    struct Result
    {
        nx::network::http::StatusCode::Value resultCode = nx::network::http::StatusCode::ok;
        nx::utils::Url tcpUrl;
        nx::utils::Url udpUrl;
    };

    nx::network::cloud::ConnectionMediatorUrlFetcher m_urlFetcher;
    nx::utils::SyncQueue<Result> m_results;
    Result m_prevResult;

    void saveMediatorUrls(
        nx::network::http::StatusCode::Value resultCode,
        nx::utils::Url tcpUrl,
        nx::utils::Url udpUrl)
    {
        Result result;
        result.resultCode = resultCode;
        result.tcpUrl = tcpUrl;
        result.udpUrl = udpUrl;
        m_results.push(std::move(result));
    }
};

TEST_F(ModuleDiscovery, cloud_modules_xml_with_multiple_mediator_ports)
{
    whenRequestNewModuleXml();

    thenXmlIsProvided();
    andXmlContainsMultipleMediatorPorts();
}

} // namespace test
} // namespace cdb
} // namespace nx
