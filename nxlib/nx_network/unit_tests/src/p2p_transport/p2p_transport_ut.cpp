#include <gtest/gtest.h>
#include <nx/network/p2p_transport/p2p_http_server_transport.h>
#include <nx/network/p2p_transport/p2p_http_client_transport.h>

namespace nx::network::test {

class P2PHttpTransport: public ::testing::Test
{
protected:
private:
    std::unique_ptr<P2PHttpServerTransport> m_server;
    std::unique_ptr<P2PHttpClientTransport> m_client;
};

TEST_F(P2PHttpTransport, ClientReads)
{

}

} // namespace nx::network::test