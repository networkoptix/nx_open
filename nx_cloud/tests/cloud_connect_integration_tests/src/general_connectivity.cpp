#include <gtest/gtest.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

class GeneralConnectivity:
    public BasicTestFixture
{
    using base_type = BasicTestFixture;

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
