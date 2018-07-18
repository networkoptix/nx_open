#pragma once

#include <vector>

#include <gtest/gtest.h>

#if defined(USE_GMOCK)
    #include <gmock/gmock.h>
#endif

#include <nx/kit/ini_config.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/rlimit.h>

#include "test_options.h"

namespace nx {
namespace utils {
namespace test {

enum GtestRunFlag
{
    gtestThrowOnFailure = 1
};

typedef std::vector<MoveOnlyFunc<void()>> DeinitFunctions;
typedef MoveOnlyFunc<DeinitFunctions(const ArgumentParser& args)> InitFunction;

/**
 * Sets up environment and runs Google Tests. Should be used for all unit tests.
 */
inline int runTest(
    int argc,
    const char* argv[],
    InitFunction extraInit = nullptr,
    int gtestRunFlags = 0)
{
    nx::utils::setOnAssertHandler([&](const log::Message& m) { FAIL() << m.toStdString(); });
    nx::kit::IniConfig::setOutput(nullptr);

    std::vector<const char*> extendedArgs;
    for (int i = 0; i < argc; ++i)
        extendedArgs.push_back(argv[i]);

    if (gtestRunFlags & GtestRunFlag::gtestThrowOnFailure)
        extendedArgs.push_back("--gtest_throw_on_failure");
    argc = (int)extendedArgs.size();

    // NOTE: On osx InitGoogleTest(...) should be called independent of InitGoogleMock(...).
    ::testing::InitGoogleTest(&argc, (char**) extendedArgs.data());

    #if defined(USE_GMOCK)
        ::testing::InitGoogleMock(&argc, (char**) extendedArgs.data());
    #endif

    ArgumentParser args(argc, extendedArgs.data());
    TestOptions::applyArguments(args);
    nx::utils::log::initializeGlobally(args);
    nx::utils::log::lockConfiguration();

    DeinitFunctions deinitFunctions;
    if (extraInit)
        deinitFunctions = extraInit(args);

    nx::utils::rlimit::setDefaultMaxFileDescriptiors();

    const int result = RUN_ALL_TESTS();
    for (const auto& deinit: deinitFunctions)
        deinit();

    return result;
}

inline int runTest(
    int argc,
    char* argv[],
    InitFunction extraInit = nullptr,
    int gtestRunFlags = 0)
{
    return runTest(
        argc,
        (const char**) argv,
        std::move(extraInit),
        gtestRunFlags);
}

} // namespace test
} // namespace utils
} // namespace nx
