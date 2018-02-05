#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/string.h>
#include <nx/network/cloud/data/client_bind_data.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class StatisticsApi:
    public MediatorFunctionalTest
{
};

TEST_F(StatisticsApi, listening_peer_list)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto client = clientConnection();
    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, QnUuid::createUuid().toSimpleString().toUtf8());

    hpm::api::ClientBindRequest request;
    request.originatingPeerID = "someClient";
    request.tcpReverseEndpoints.push_back(nx::network::SocketAddress("12.34.56.78:1234"));
    nx::utils::promise<void> bindPromise;
    client->send(
        request,
        [&](nx::hpm::api::ResultCode code)
        {
            ASSERT_EQ(code, nx::hpm::api::ResultCode::ok);
            bindPromise.set_value();
        });
    bindPromise.get_future().wait();

    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
    api::ListeningPeers listeningPeers;
    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx::network::http::StatusCode::ok, statusCode);

    ASSERT_EQ(1U, listeningPeers.systems.size());
    const auto systemIter = listeningPeers.systems.find(system1.id);
    ASSERT_NE(listeningPeers.systems.end(), systemIter);

    ASSERT_EQ(1U, systemIter->second.size());
    const auto serverIter = systemIter->second.find(server1->serverId());
    ASSERT_NE(systemIter->second.end(), serverIter);

    ASSERT_EQ(1U, listeningPeers.clients.size());
    const auto& boundClient = *listeningPeers.clients.begin();
    ASSERT_EQ("someClient", boundClient.first);
    ASSERT_EQ(1U, boundClient.second.tcpReverseEndpoints.size());
    ASSERT_EQ(nx::network::SocketAddress("12.34.56.78:1234"), boundClient.second.tcpReverseEndpoints.front());

    client->pleaseStopSync();
}

} // namespace test
} // namespace hpm
} // namespace nx
