#pragma once

#include <gtest/gtest.h>

#if defined(USE_GMOCK)
    #include <gmock/gmock.h>
#endif

#include <nx/kit/ini_config.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/move_only_func.h>

#include "test_options.h"

namespace nx {
namespace utils {
namespace test {

typedef std::vector<MoveOnlyFunc<void()>> DeinitFunctions;
typedef MoveOnlyFunc<DeinitFunctions(const ArgumentParser& args)> InitFunction;

/**
 * Sets up environment and runs Google Tests. Should be used for all unit tests.
 */
inline int runTest(int argc, const char* argv[], InitFunction extraInit = nullptr)
{
    nx::utils::setOnAssertHandler([&](const log::Message& m) { FAIL() << m.toStdString(); });
    nx::kit::IniConfig::setOutput(nullptr);

    // NOTE: On osx InitGoogleTest(...) should be called independent of InitGoogleMock(...).
    ::testing::InitGoogleTest(&argc, (char**) argv);

    #if defined(USE_GMOCK)
        ::testing::InitGoogleMock(&argc, (char**) argv);
    #endif

    ArgumentParser args(argc, argv);
    TestOptions::applyArguments(args);
    nx::utils::log::initializeGlobally(args);

    DeinitFunctions deinitFunctions;
    if (extraInit)
        deinitFunctions = extraInit(args);

    const int result = RUN_ALL_TESTS();
    for (const auto& deinit: deinitFunctions)
        deinit();

    return result;
}

inline int runTest(int argc, char* argv[], InitFunction extraInit = nullptr)
{
    return runTest(argc, (const char**) argv, std::move(extraInit));
}

} // namespace test
} // namespace utils
} // namespace nx
