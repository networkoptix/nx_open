#pragma once

#include <gtest/gtest.h>

#ifdef USE_GMOCK
    #include <gmock/gmock.h>
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/flag_config.h>

#include "test_options.h"

namespace nx {
namespace utils {

inline int runTest(
    int argc, const char* argv[],
    std::function<void(const ArgumentParser& args)> extraInit = nullptr,
    int socketInitializationFlags = 0)
{
    nx::utils::setErrorMonitor([&](const QnLogMessage& m) { FAIL() << m.toStdString(); });
    nx::utils::FlagConfig::setOutputAllowed(false);

    // NOTE: On osx InitGoogleTest(...) should be called independent of InitGoogleMock(...)
    ::testing::InitGoogleTest(&argc, (char**)argv);

    #ifdef USE_GMOCK
        ::testing::InitGoogleMock(&argc, (char**)argv);
    #endif

    ArgumentParser args(argc, argv);
    TestOptions::applyArguments(args);
    QnLog::applyArguments(args);

    network::SocketGlobalsHolder sgGuard(socketInitializationFlags);
    network::SocketGlobals::applyArguments(args);
    network::SocketGlobals::outgoingTunnelPool().assignOwnPeerId("ut", QnUuid::createUuid());
    network::cloud::OutgoingTunnelPool::allowOwnPeerIdChange();

    if (extraInit)
        extraInit(args);

    const int result = RUN_ALL_TESTS();
    return result;
}

inline int runTest(
    int argc, char* argv[],
    std::function<void(const ArgumentParser& args)> extraInit = nullptr,
    int socketInitializationFlags = 0)
{
    return runTest(argc, (const char**)argv, std::move(extraInit), socketInitializationFlags);
}

} // namespace utils
} // namespace nx
