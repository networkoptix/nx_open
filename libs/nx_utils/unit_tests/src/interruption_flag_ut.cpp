// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/interruption_flag.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/std_string_utils.h>

namespace nx::utils::test {

TEST(InterruptionFlag, Watcher)
{
    nx::utils::InterruptionFlag flag;
    nx::utils::InterruptionFlag::Watcher watcher(&flag);

    flag.interrupt();

    ASSERT_TRUE(watcher.interrupted());
}

TEST(InterruptionFlag, toString)
{
    static const std::string kFunctionName =
#ifdef _WIN32
        "void __cdecl nx::utils::test::InterruptionFlag_toString_Test::TestBody(void)";
#else
        "virtual void nx::utils::test::InterruptionFlag_toString_Test::TestBody()";
#endif //#ifdef _WIN32

    nx::utils::InterruptionFlag flag;
    nx::utils::InterruptionFlag::Watcher watcher(&flag);
    nx::utils::InterruptionFlag::Watcher watcher2(&flag);

    auto interruptedFlagString = NX_FMT("%1", flag).toStdString();

    auto expected = nx::format(
        "[{interrupted: false, func: %1}, {interrupted: false, func: %2}]",
        kFunctionName, kFunctionName).toStdString();

    ASSERT_EQ(expected, interruptedFlagString);

    //-----------------------------------------------------------------------

    flag.interrupt();

    interruptedFlagString = NX_FMT("%1", flag).toStdString();

    ASSERT_EQ("[]", interruptedFlagString);

    //-----------------------------------------------------------------------

    const auto watcherString = NX_FMT("%1", watcher).toStdString();

    expected = nx::format("{interrupted: true, func: %1}", kFunctionName).toStdString();

    ASSERT_EQ(expected, watcherString);
}

}
