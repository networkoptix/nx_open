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

void TestOptions::applyArguments(const utils::ArgumentParser& arguments)
{
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
