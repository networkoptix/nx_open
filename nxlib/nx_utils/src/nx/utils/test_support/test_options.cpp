#include "test_options.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>

#ifdef _WIN32
    #include "../crash_dump/systemexcept_win.h"
#endif

namespace nx {
namespace utils {

#define TEST_OPTIONS_SET_VALUE(VARIABLE, VALUE) \
    VARIABLE = VALUE; \
    qDebug() << lm(">>>>> %1: %2 = %3").args(typeid(TestOptions), #VARIABLE, VALUE);

void TestOptions::setTimeoutMultiplier(size_t value)
{
    TEST_OPTIONS_SET_VALUE(s_timeoutMultiplier, value);
}

size_t TestOptions::timeoutMultiplier()
{
    return s_timeoutMultiplier;
}

void TestOptions::disableTimeAsserts(bool areDisabled)
{
    TEST_OPTIONS_SET_VALUE(s_disableTimeAsserts, areDisabled);
}

bool TestOptions::areTimeAssertsDisabled()
{
    return s_disableTimeAsserts;
}

void TestOptions::setTemporaryDirectoryPath(const QString& path)
{
    QnMutexLocker lock(&s_mutex);
    TEST_OPTIONS_SET_VALUE(s_temporaryDirectoryPath, path);
}

QString TestOptions::temporaryDirectoryPath()
{
    QnMutexLocker lock(&s_mutex);
    QDir().mkpath(s_temporaryDirectoryPath);
    return s_temporaryDirectoryPath;
}

void TestOptions::setLoadMode(const QString& mode)
{
    QString loadMode;
    TEST_OPTIONS_SET_VALUE(loadMode, mode.toLower());

    bool isOk = 0;
    s_loadMode = mode.toUInt(&isOk);
    if (isOk)
        return;

    if (loadMode == QLatin1String("light"))
        s_loadMode = 0;
    else if (loadMode == QLatin1String("normal"))
        s_loadMode = 1;
    else if (loadMode == QLatin1String("stress"))
        s_loadMode = 100;
    else
        NX_CRITICAL(false, lm("Unrecognized load mode: %1").arg(mode));
}

void TestOptions::applyArguments(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get<size_t>("timeout-multiplier"))
        setTimeoutMultiplier(*value);

    if (arguments.get("disable-time-asserts"))
        disableTimeAsserts();

    if (const auto value = arguments.get("tmp"))
        setTemporaryDirectoryPath(*value);

    if (const auto value = arguments.get("load-mode"))
        setLoadMode(*value);

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

std::atomic<size_t> TestOptions::s_timeoutMultiplier(1);
std::atomic<bool> TestOptions::s_disableTimeAsserts(false);
std::atomic<size_t> TestOptions::s_loadMode(1);

QnMutex TestOptions::s_mutex;
QString TestOptions::s_temporaryDirectoryPath =
    lm("%1/nx_unit_tests/%2").args(QDir::homePath(), nx::utils::random::number());

} // namespace utils
} // namespace nx
