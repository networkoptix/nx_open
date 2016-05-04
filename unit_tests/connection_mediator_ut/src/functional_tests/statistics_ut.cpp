/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/http/httpclient.h>

#include <utils/common/string.h>
#include <utils/serialization/json.h>

#include <data/listening_peer.h>
#include <http/get_listening_peer_list_handler.h>

#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, statistics)
{
    startAndWaitUntilStarted();

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, generateRandomName(16));
    
    nx_http::HttpClient httpClient;
    const auto urlStr = 
        lm("http://%1%2").arg(httpEndpoint().toString())
            .arg(nx::hpm::http::GetListeningPeerListHandler::kHandlerPath);
    ASSERT_TRUE(httpClient.doGet(QUrl(urlStr)));
    ASSERT_EQ(
        nx_http::StatusCode::ok,
        httpClient.response()->statusLine.statusCode);
    QByteArray responseBody;
    while (!httpClient.eof())
        responseBody += httpClient.fetchMessageBodyBuffer();

    auto listeningPeers =
        QJson::deserialized<nx::hpm::data::ListeningPeersBySystem>(responseBody);
    
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
