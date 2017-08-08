#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_stream_socket.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class GeneralConnectivity:
    public BasicTestFixture
{
    using base_type = BasicTestFixture;

protected:
    void givenListeningCloudServer()
    {
        assertConnectionCanBeEstablished();
    }

    void whenConnectToThePeerUsingDomainName()
    {
        setRemotePeerName(cloudSystemCredentials().systemId);
        assertConnectionCanBeEstablished();
    }

    void whenConnectionToMediatorIsBroken()
    {
        restartMediator();
    }

    void thenClientSocketProvidesRemotePeerFullName()
    {
        auto cloudStreamSocket = dynamic_cast<const CloudStreamSocket*>(clientSocket().get());
        ASSERT_NE(nullptr, cloudStreamSocket);
        ASSERT_EQ(
            cloudSystemCredentials().hostName(),
            cloudStreamSocket->getForeignHostName());
    }

    void thenConnectionToMediatorIsRestored()
    {
        waitUntilServerIsRegisteredOnMediator();
    }

private:
    virtual void SetUp() override
    {
        base_type::SetUp();

        startServer();
    }
};

TEST_F(GeneralConnectivity, connect_works)
{
    assertConnectionCanBeEstablished();
}

TEST_F(GeneralConnectivity, remote_peer_full_name_is_known_on_client_side)
{
    whenConnectToThePeerUsingDomainName();
    thenClientSocketProvidesRemotePeerFullName();
}

TEST_F(GeneralConnectivity, server_accepts_connections_after_reconnect_to_mediator)
{
    givenListeningCloudServer();

    whenConnectionToMediatorIsBroken();

    thenConnectionToMediatorIsRestored();
    assertConnectionCanBeEstablished();
}

//-------------------------------------------------------------------------------------------------

class GeneralConnectivityCompatibilityRawStunConnection:
    public GeneralConnectivity
{
    using base_type = GeneralConnectivity;

public:
    GeneralConnectivityCompatibilityRawStunConnection()
    {
        setMediatorApiProtocol(MediatorApiProtocol::stun);
    }
};

TEST_F(GeneralConnectivityCompatibilityRawStunConnection, cloud_connection_can_be_established)
{
    assertConnectionCanBeEstablished();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
