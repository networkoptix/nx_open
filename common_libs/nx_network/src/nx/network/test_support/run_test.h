#pragma once

#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/socket_global.h>
#include <nx/utils/test_support/run_test.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace test {

/**
 * Sets up environment and runs Google Tests. Should be used for all unit tests which use network.
 */
inline int runTest(
    int& argc, const char* argv[],
    utils::test::InitFunction extraInit = nullptr,
    int socketGlobalsFlags = 0)
{
    return utils::test::runTest(
        argc, argv,
        [extraInit = std::move(extraInit), socketGlobalsFlags](const utils::ArgumentParser& args)
        {
            nx::utils::DetachedThreads detachedThreadsGuard;

            auto sgGuard = std::make_unique<SocketGlobalsHolder>(socketGlobalsFlags);
            SocketGlobals::applyArguments(args);
            SocketGlobals::outgoingTunnelPool().assignOwnPeerId("ut", QnUuid::createUuid());
            cloud::OutgoingTunnelPool::allowOwnPeerIdChange();

            utils::test::DeinitFunctions deinitFunctions;
            if (extraInit)
                deinitFunctions = extraInit(args);

            deinitFunctions.push_back(
                [sgGuard = std::move(sgGuard)]() mutable
                {
                    sgGuard.reset();
                });

            return deinitFunctions;
        });
}

inline int runTest(
    int& argc, char* argv[],
    utils::test::InitFunction extraInit = nullptr,
    int socketGlobalsFlags = 0)
{
    return runTest(argc, (const char**)argv, std::move(extraInit), socketGlobalsFlags);
}

} // namespace test
} // namespace network
} // namespace nx
