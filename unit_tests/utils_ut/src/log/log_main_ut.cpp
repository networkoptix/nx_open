#include <gtest/gtest.h>

#include <nx/utils/log/log_main.h>

namespace nx {
namespace utils {
namespace log {
namespace test {

TEST(LogMain, DISABLED_Console)
{
    const auto mainLog = main();
    mainLog->setDefaultLevel(levelFromString("INFO"));
    mainLog->setWriters();
    mainLog->setExceptionFilters({lit("nx::utils::log::test")});

    const auto testTag = lit("SomeTag");
    NX_ALWAYS(testTag, "Always");
    NX_ERROR(testTag, "Error message");
    NX_WARNING(testTag, "Warning message");
    NX_INFO(testTag, "Info message");
    NX_DEBUG(testTag, "Debug message");
    NX_VERBOSE(testTag, "Verbose message");

    NX_ALWAYS(this, "Always");
    NX_ERROR(this, "Error message");
    NX_WARNING(this, "Warning message");
    NX_INFO(this, "Info message");
    NX_DEBUG(this, "Debug message");
    NX_VERBOSE(this, "Verbose message");
}

} // namespace test
} // namespace log
} // namespace utils
} // namespace nx
