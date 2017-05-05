#include "test_options.h"

#include <QDebug>

#include <nx/utils/log/assert.h>

#ifdef _WIN32
#include "../crash_dump/systemexcept_win.h"
#endif

namespace nx {
namespace utils {

QString TestOptions::sTemporaryDirectoryPath;

void TestOptions::setTimeoutMultiplier(size_t value)
{
    s_timeoutMultiplier = value;
    qWarning() << ">>> TestOptions::setTimeoutMultiplier(" << value << ") <<<";
}

size_t TestOptions::timeoutMultiplier()
{
    return s_timeoutMultiplier;
}

void TestOptions::disableTimeAsserts(bool areDisabled)
{
    s_disableTimeAsserts = areDisabled;
    qWarning() << ">>> TestOptions::disableTimeAsserts(" << areDisabled << ") <<<";
}

bool TestOptions::areTimeAssertsDisabled()
{
    return s_disableTimeAsserts;
}

void TestOptions::setLoadMode(const QString& mode)
{
    qWarning() << ">>> TestOptions::setLoadMode(" << mode << ") <<<";

    if (mode == QLatin1String("light"))
        s_loadMode = LoadMode::light;
    else if (mode == QLatin1String("normal"))
        s_loadMode = LoadMode::normal;
    else if (mode == QLatin1String("stress"))
        s_loadMode = LoadMode::stress;
    else
        NX_CRITICAL(false, lm("Unrecognized load mode: %1").arg(mode));
}

TestOptions::LoadMode TestOptions::getLoadMode()
{
    return s_loadMode;
}

void TestOptions::applyArguments(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get<size_t>("timeout-multiplier"))
        setTimeoutMultiplier(*value);

    if (arguments.get("disable-time-asserts"))
        disableTimeAsserts();

    if (const auto value = arguments.get("load-mode"))
        setLoadMode(*value);

    if (const auto value = arguments.get("tmp"))
        setTemporaryDirectoryPath(*value);

#ifdef _WIN32
    bool enableCrashDump = true;
    if (arguments.get("disable-crash-dump"))
        enableCrashDump = false;

    bool generateFullCrashDump = false;
    if (arguments.get("enable-full-crash-dump"))
        generateFullCrashDump = true;

    if (enableCrashDump)
    {
        win32_exception::installGlobalUnhandledExceptionHandler();
        win32_exception::setCreateFullCrashDump(generateFullCrashDump);
    }
#endif
}

void TestOptions::setTemporaryDirectoryPath(const QString& path)
{
    sTemporaryDirectoryPath = path;
}

QString TestOptions::temporaryDirectoryPath()
{
    return sTemporaryDirectoryPath;
}

std::atomic<size_t> TestOptions::s_timeoutMultiplier(1);
std::atomic<bool> TestOptions::s_disableTimeAsserts(false);
std::atomic<TestOptions::LoadMode> TestOptions::s_loadMode(TestOptions::LoadMode::normal);

} // namespace utils
} // namespace nx
