#include <cmath>

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <rest/server/json_rest_result.h>
#include <nx_ec/ec_api.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/custom_headers.h>
#include <nx/vms/api/data/module_information.h>

using namespace nx::utils::test;

namespace nx {
namespace mediaserver {
namespace proxy {
namespace test {

class ProxyTest: public ::testing::Test
{
public:
    ProxyTest()
    {
        NX_VERBOSE(this, lm("Start proxy test"));
        ec2::Appserver2Process::resetInstanceCounter();
    }
    ~ProxyTest()
    {
        NX_VERBOSE(this, lm("End proxy test"));
    }

public:
    std::vector<ec2::Appserver2Ptr> m_peers;

    void startPeers(int peerCount)
    {
        while(m_peers.size() < peerCount)
        {
            auto peer = ec2::Appserver2Launcher::createAppserver();
            peer->start();
            m_peers.push_back(std::move(peer));
        }
        for (const auto& peer: m_peers)
            ASSERT_TRUE(peer->waitUntilStarted());
    }

    void connectPeers()
    {
        for (int i = 0; i < m_peers.size(); ++i)
        {
            for (int j = 0; j < m_peers.size(); ++j)
            {
                if (i != j)
                    m_peers[i]->moduleInstance()->connectTo(m_peers[j]->moduleInstance().get());
            }
        }
    }

    nx::utils::Url serverUrl(int index, const QString& path)
    {
        const auto endpoint = m_peers[index]->moduleInstance()->endpoint();
        nx::utils::Url url = nx::utils::Url(lit("http://") + endpoint.toString());
        url.setPath(path);
        return url;
    }
};

TEST_F(ProxyTest, proxyToAnotherThenToThemself)
{
    startPeers(2);
    connectPeers();

    auto client = std::make_unique<nx::network::http::HttpClient>();

    ASSERT_TRUE(client->doGet(serverUrl(0, "/api/moduleInformation")));
    ASSERT_TRUE(client->response());
    ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);

    for (int i = m_peers.size()-1; i >=0; --i)
    {
        const auto guid = m_peers[i]->moduleInstance()->commonModule()->moduleGUID();
        client->removeAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME);
        client->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, guid.toByteArray());
        ASSERT_TRUE(client->doGet(serverUrl(0, "/api/moduleInformation")));
        ASSERT_TRUE(client->response());
        ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);

        bool success = false;
        auto response = client->fetchEntireMessageBody();
        ASSERT_TRUE(response);
        QnJsonRestResult json =
            QJson::deserialized<QnJsonRestResult>(*response, QnJsonRestResult(), &success);
        auto moduleInformation = json.deserialized<nx::vms::api::ModuleInformation>();

        ASSERT_EQ(guid, moduleInformation.id);
    }
    int totalRequests = client->totalRequestsSent();
    int viaCurrentConnection = client->totalRequestsSentViaCurrentConnection();
    ASSERT_EQ(totalRequests, viaCurrentConnection);
}
} // namespace test
} // namespace proxy
} // namespace mediaserver
} // namespace nx
