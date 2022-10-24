// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <gtest/gtest.h>

#if defined(USE_GMOCK)
    #include <gmock/gmock.h>
#endif

#include <QtCore/QFileInfo>

#include <nx/kit/ini_config.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/rlimit.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/filesystem.h>

#include "test_options.h"
#include <nx/utils/nx_utils_ini.h>

namespace nx {
namespace utils {
namespace test {

enum TestRunFlag
{
    /**
     * If set, each gtest assert failure will throw.
     * This is achieved be installing a custom gtest listener (by inheriting
     * testing::EmptyTestEventListener) and throwing on each failure from it.
     * NOTE: This is different from --gtest_throw_on_failure:
     * gtest_throw_on_failure exists to pass exceptions to third-party test frameworks.
     * So, it throws unhandled exception terminating the process as a result.
     * On the other hand, this flag exists to propagate fatal test failures from subroutines
     * to make such subroutines reusable in multiple tests.
     * The process termination is undesirable in this case.
     * See
     * https://github.com/google/googletest/blob/main/docs/advanced.md#propagating-fatal-failures
     * for more information.
     */
    throwOnFailure = 1,
};

class ThrowListener:
    public testing::EmptyTestEventListener
{
    virtual void OnTestPartResult(const testing::TestPartResult& result) override
    {
        using namespace ::testing;

        if (result.type() == TestPartResult::kFatalFailure &&
            !GTEST_FLAG(break_on_failure))  //< Throwing exception would cancel the interrupt signal.
        {
            throw AssertionException(result);
        }
    }
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
    nx::utils::TestOptions::setModuleName(QFileInfo(QString::fromLocal8Bit(argv[0])).baseName());

    nx::utils::setOnAssertHandler([&](const QString& m) { FAIL() << m.toStdString(); });
    nx::utils::enableQtMessageAsserts();
    nx::kit::IniConfig::setOutput(nullptr);

    std::vector<const char*> extendedArgs;
    for (int i = 0; i < argc; ++i)
        extendedArgs.push_back(argv[i]);

    /* Disable log rotation for unit tests. */
    if (std::find_if(extendedArgs.begin(), extendedArgs.end(),
        [](const auto& elem) { return QString(elem).startsWith("--log"); }) != extendedArgs.end())
    {
        extendedArgs.insert(extendedArgs.end(),
            {"--log/maxLogVolumeSizeB=10737418240", "--log/maxLogFileSizeB=10737418240"});
    }

    if (gtestRunFlags & TestRunFlag::throwOnFailure)
        testing::UnitTest::GetInstance()->listeners().Append(new ThrowListener);
    argc = (int)extendedArgs.size();
    extendedArgs.push_back(nullptr);

    // NOTE: On osx InitGoogleTest(...) should be called independent of InitGoogleMock(...).
    ::testing::InitGoogleTest(&argc, (char**) extendedArgs.data());
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    #if defined(USE_GMOCK)
        ::testing::InitGoogleMock(&argc, (char**) extendedArgs.data());
    #endif

    ArgumentParser args(argc, extendedArgs.data());

    TestOptions::setLoadFactor(nx::utils::Ini().loadFactor);
    TestOptions::applyArguments(args);

    nx::utils::log::initializeGlobally(args);
    nx::utils::log::lockConfiguration();

    DeinitFunctions deinitFunctions;
    if (extraInit)
        deinitFunctions = extraInit(args);

    nx::utils::rlimit::setMaxFileDescriptors();

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
