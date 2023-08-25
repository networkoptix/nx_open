// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_options.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/utils/log/assert.h>
#include <nx/utils/uuid.h>

#ifdef _WIN32
    #include "../crash_dump/systemexcept_win.h"
#endif

namespace nx {
namespace utils {

#define TEST_OPTIONS_SET_VALUE(VARIABLE, VALUE) \
    VARIABLE = VALUE; \
    qDebug() << nx::format(">>>>> %1: %2 = %3").args(typeid(TestOptions), #VARIABLE, VALUE);

void TestOptions::setTimeoutMultiplier(size_t value)
{
    TEST_OPTIONS_SET_VALUE(s_timeoutMultiplier, value);
}

size_t TestOptions::timeoutMultiplier()
{
    return s_timeoutMultiplier;
}

void TestOptions::setLoadFactor(double value)
{
    s_loadFactor = value;
}

double TestOptions::loadFactor()
{
    return s_loadFactor;
}

void TestOptions::disableTimeAsserts(bool areDisabled)
{
    TEST_OPTIONS_SET_VALUE(s_disableTimeAsserts, areDisabled);
}

bool TestOptions::areTimeAssertsDisabled()
{
    return s_disableTimeAsserts;
}

bool TestOptions::keepTemporaryDirectory()
{
    return s_keepTemporaryDirectory;
}

void TestOptions::setKeepTemporaryDirectory(bool value)
{
    TEST_OPTIONS_SET_VALUE(s_keepTemporaryDirectory, value);
}

void TestOptions::setTemporaryDirectoryPath(const QString& path)
{
    tmpDirInstance().setPath(path);
}

QString TestOptions::temporaryDirectoryPath(bool canCreate)
{
   return tmpDirInstance().path(canCreate);
}

void TestOptions::setModuleName(const QString& value)
{
    s_moduleName = value;
}

QString TestOptions::moduleName()
{
    return s_moduleName;
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
        NX_CRITICAL(false, nx::format("Unrecognized load mode: %1").arg(mode));
}

static void printHelp()
{
    // ATTENTION: The format of this help is similar to the gtest's help which is printed before.
    // The CI scripts parse this help text in order to understand which options are supported.

    std::cout << //< The initial newline separates this printout from the gtest's one.
R"(
The following command line arguments are supported by this particular unit test suite:
  -h, --help
      Show this help, and exit with a successful (zero) exit status.
  -t, --tmp=[TMP_DIR]
      Set the path to a directory for temporary files.
  --enable-discovery
      Enable the discovery.
  --timeout-multiplier=[VALUE]
      Set the timeout multiplier to speed up realtime tests.
  --load-factor=[VALUE]
      Increase the amount of work for some tests. Default value is 1.0.
  --disable-time-asserts
      Do not check the calendar time.
  --keep-temporary-directory
      Do not delete the temporary directory after the test is finished.
  --load-mode=(light|normal|stress)
      Set the load mode.
  --log-level=[LOG_LEVEL]
      Set the log level using the same language as in the respective Server and Client command-line
      argument: a comma-separated list of items, each being a log level name (none, error, warning,
      info, debug, verbose, trace) possibly followed by a bracketed tag prefix.
  --log-file=[ABSOLUTE_FILE_PATH]
      Redirect logging to the file with the specified absolute path.
)";

    #if defined(_WIN32)
        std::cout << /*suppress newline*/ 1 + (const char*)
R"(
  --disable-crash-dump
      Do not create a crash dump if the test fails.
  --enable-full-crash-dump
      Generate full crash dump if the test fails.
)";
    #endif
}

static void showHelpAndExitIfNeeded(const utils::ArgumentParser& argumentParser)
{
    // ATTENTION: The arguments which trigger the help are consistent with those of the gtest
    // itself, so that the gtest's help is printed before our help.

    if (argumentParser.get("help", "h", "?"))
    {
        printHelp();
        exit(0);
    }

    for (const auto& arg: argumentParser.getPositionalArgs())
    {
        if (arg == "/?")
        {
            printHelp();
            exit(0);
        }
    }
}

void TestOptions::applyArguments(const utils::ArgumentParser& arguments)
{
    showHelpAndExitIfNeeded(arguments);

    if (const auto value = arguments.get<size_t>("timeout-multiplier"))
        setTimeoutMultiplier(*value);

    if (const auto value = arguments.get<double>("load-factor"))
        setLoadFactor(*value);

    if (arguments.get("disable-time-asserts"))
        disableTimeAsserts();

    if (const auto value = arguments.get("tmp"))
        setTemporaryDirectoryPath(*value);

    if (arguments.get("keep-temporary-directory"))
        setKeepTemporaryDirectory(true);

    if (const auto value = arguments.get("load-mode"))
        setLoadMode(*value);

    #if defined(_WIN32)
        if (!arguments.get("disable-crash-dump"))
        {
            win32_exception::installGlobalUnhandledExceptionHandler();
            win32_exception::setCreateFullCrashDump(
                arguments.get("enable-full-crash-dump").has_value());
        }
    #endif
}

TestOptions::TemporaryDirectory& TestOptions::tmpDirInstance()
{
    static TemporaryDirectory instance;
    return instance;
}

//-------------------------------------------------------------------------------------------------

TestOptions::TemporaryDirectory::TemporaryDirectory()
{
    const auto kModuleSubdirectory =
        QString("nx_unit_tests/%1").arg(QnUuid::createUuid().toSimpleString());

    const auto root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    m_path = root.absoluteFilePath(kModuleSubdirectory);
}

TestOptions::TemporaryDirectory::~TemporaryDirectory()
{
    if (!keepTemporaryDirectory())
        QDir(m_path).removeRecursively();
}

QString TestOptions::TemporaryDirectory::path(bool canCreate) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (canCreate)
    {
        const bool created = QDir().mkpath(m_path);
        NX_ASSERT(created, "Temporary directory cannot be created");
    }
    return m_path;
}

void TestOptions::TemporaryDirectory::setPath(const QString& path)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QDir(m_path).removeRecursively();
    TEST_OPTIONS_SET_VALUE(m_path, path);
}

std::atomic<size_t> TestOptions::s_timeoutMultiplier(1);
std::atomic<double> TestOptions::s_loadFactor(1.0);
std::atomic<bool> TestOptions::s_disableTimeAsserts(false);
std::atomic<bool> TestOptions::s_keepTemporaryDirectory(false);
std::atomic<size_t> TestOptions::s_loadMode(1);

QString TestOptions::s_moduleName(QnUuid::createUuid().toSimpleString());

} // namespace utils
} // namespace nx
