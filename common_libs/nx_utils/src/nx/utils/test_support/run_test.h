#pragma once

#include <gtest/gtest.h>

#ifdef USE_GMOCK
    #include <gmock/gmock.h>
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/test_support/test_options.h>

namespace nx {
namespace utils {

inline int runTest(
    int argc, const char* argv[],
    std::function<void(const ArgumentParser& args)> extraInit = nullptr)
{
    #ifdef USE_GMOCK
        ::testing::InitGoogleMock(&argc, (char**)argv);
    #else
        ::testing::InitGoogleTest(&argc, (char**)argv);
    #endif

    ArgumentParser args(argc, argv);
    TestOptions::applyArguments(args);
    QnLog::applyArguments(args);

    #ifdef NX_NETWORK_SOCKET_GLOBALS
        network::SocketGlobalsHolder sgGuard;
        network::SocketGlobals::applyArguments(args);
    #endif

    if (extraInit)
        extraInit(args);

    const int result = RUN_ALL_TESTS();
    return result;
}

inline int runTest(
    int argc, char* argv[],
    std::function<void(const ArgumentParser& args)> extraInit = nullptr)
{
    return runTest(argc, (const char**)argv, std::move(extraInit));
}

} // namespace utils
} // namespace nx
