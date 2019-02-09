#if !defined(_WIN32)
#   include <arpa/inet.h>
#endif

#include <thread>

#include "test_fixture.h"

namespace udt::test {

class Deinitialization:
    public BasicFixture
{
    using base_type = BasicFixture;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        initializeUdt();
    }

    void whenRemoveAllSockets()
    {
        closeServerSocket();
        closeClientSocket();
    }
};

TEST_F(Deinitialization, does_not_crash_the_process)
{
    givenListeningServerSocket();
    startAcceptingAsync();

    givenConnectingClientSocket();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(rand() % 5));

    whenRemoveAllSockets();
    deinitializeUdt();

    // thenProcessDoesNotCrash();
}

TEST_F(Deinitialization, pending_send)
{
    givenListeningServerSocket();
    givenTwoConnectedSockets();

    whenClientSendsAsync("Test");

    std::this_thread::sleep_for(
        std::chrono::milliseconds(rand() % 5));

    whenRemoveAllSockets();
    deinitializeUdt();

    // thenProcessDoesNotCrash();
}

} // namespace udt::test
