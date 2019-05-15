#include "test_fixture.h"

namespace udt::test {

class Ipv6:
    public BasicFixture
{
public:
    Ipv6()
    {
        setIpVersion(AF_INET6);

        initializeUdt();
    }

    ~Ipv6()
    {
        deinitializeUdt();
    }
};

TEST_F(Ipv6, listening_on_ipv6_socket)
{
    givenListeningServerSocket();
}

} // namespace udt::test
