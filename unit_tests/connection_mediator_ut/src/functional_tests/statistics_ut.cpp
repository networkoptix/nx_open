/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <utils/common/string.h>

#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

class Statistics
:
    public MediatorFunctionalTest
{
};

TEST_F(Statistics, listening_peer_list)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, generateRandomName(16));

    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
    data::ListeningPeersBySystem listeningPeers;
    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx_http::StatusCode::ok, statusCode);

    ASSERT_EQ(1, listeningPeers.systems.size());
    const auto systemIter = listeningPeers.systems.find(system1.id);
    ASSERT_NE(listeningPeers.systems.end(), systemIter);
    ASSERT_EQ(1, systemIter->second.peers.size());
    ASSERT_EQ(server1->serverId(), systemIter->second.peers[0].id);

    client->pleaseStopSync();
}

} // namespace test
} // namespace hpm
} // namespase nx
