#include <gtest/gtest.h>

#include <nx/clusterdb/engine/api/client.h>

#include "cluster_test_fixture.h"

namespace nx::clusterdb::engine::test {

class HttpApi:
    public ::testing::Test,
    public ClusterTestFixture
{
protected:
    virtual void SetUp() override
    {
        addPeer();

        m_client = std::make_unique<api::Client>(
            peer(0).syncronizationUrl(),
            "systemId");
    }

    void whenRequestInfo()
    {
        m_prevInfoResponse = m_client->getInfo();
    }

    void thenRequestSucceeded()
    {
        ASSERT_EQ(nx::network::http::StatusCode::ok, std::get<0>(m_prevInfoResponse));
    }

    void andPeerIdIsPresent()
    {
        ASSERT_EQ(
            peer(0).process().moduleInstance()->syncronizationEngine()
                .peerId().toSimpleString().toStdString(),
            std::get<1>(m_prevInfoResponse).id);
    }

private:
    std::unique_ptr<api::Client> m_client;
    std::tuple<nx::network::http::StatusCode::Value, api::PeerInfo> m_prevInfoResponse;
};

TEST_F(HttpApi, peer_id_is_present_in_info_response)
{
    whenRequestInfo();

    thenRequestSucceeded();
    andPeerIdIsPresent();
}

} // namespace nx::clusterdb::engine::test
