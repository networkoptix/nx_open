// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/test_support/run_test.h>
#include <nx/utils/std/thread.h>

namespace nx::network::test {

// NOTE: Everything is implemented in the header file so that nx_network does not depend on gtest.

class EventRecordingListener:
    public ::testing::EmptyTestEventListener
{
protected:
    void OnTestStart(const ::testing::TestInfo& test_info) override
    {
        NX_INFO(this, "======== Test %1.%2 =========",
            test_info.test_suite_name(), test_info.name());

        nx::network::http::tunneling::detail::ClientFactory::instance().reset();
    }
};

/**
 * Sets up environment and runs Google Tests. Should be used for all unit tests which use network.
 */
inline int runTest(
    int argc, const char* argv[],
    utils::test::InitFunction extraInit = nullptr,
    int socketGlobalsFlags = 0,
    int gtestRunFlags = 0)
{
    return utils::test::runTest(
        argc, argv,
        [extraInit = std::move(extraInit), socketGlobalsFlags](
            const utils::ArgumentParser& args)
        {
            ::testing::UnitTest::GetInstance()->listeners().Append(
                new EventRecordingListener());

            nx::utils::DetachedThreads detachedThreadsGuard;

            auto sgGuard = std::make_unique<SocketGlobalsHolder>(args, socketGlobalsFlags);
            SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId("ut", QnUuid::createUuid());
            cloud::OutgoingTunnelPool::ignoreOwnPeerIdChange();

            utils::test::DeinitFunctions deinitFunctions;
            if (extraInit)
                deinitFunctions = extraInit(args);

            deinitFunctions.push_back(
                [sgGuard = std::move(sgGuard)]() mutable
                {
                    sgGuard.reset();
                });

            // This is to increase tests stability in case rare network problems.
            http::AsyncClient::Timeouts::setDefaults({
                /*sendTimeout*/ std::chrono::seconds(61),
                /*responseReadTimeout*/ std::chrono::seconds(62),
                /*messageBodyReadTimeout*/ std::chrono::seconds(103),
            });

            return deinitFunctions;
        },
        gtestRunFlags);
}

inline int runTest(
    int argc, char* argv[],
    utils::test::InitFunction extraInit = nullptr,
    int socketGlobalsFlags = 0,
    int gtestRunFlags = 0)
{
    return runTest(
        argc, (const char**)argv,
        std::move(extraInit),
        socketGlobalsFlags,
        gtestRunFlags);
}

} // namespace nx::network::test
