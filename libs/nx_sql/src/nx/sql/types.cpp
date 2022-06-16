// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

#include <chrono>
#include <thread>

#include <nx/utils/timer_manager.h>
#include <nx/utils/deprecated_settings.h>

namespace nx::sql {

const char* toString(DBResult result)
{
    switch (result)
    {
        case DBResult::ok:
            return "ok";
        case DBResult::statementError:
            return "statementError";
        case DBResult::ioError:
            return "ioError";
        case DBResult::notFound:
            return "notFound";
        case DBResult::cancelled:
            return "cancelled";
        case DBResult::retryLater:
            return "retryLater";
        case DBResult::uniqueConstraintViolation:
            return "uniqueConstraintViolation";
        case DBResult::connectionError:
            return "connectionError";
        case DBResult::logicError:
            return "logicError";
        case DBResult::endOfData:
            return "endOfData";
        default:
            return "unknown";
    }
}

const char* toString(RdbmsDriverType value)
{
    switch (value)
    {
        case RdbmsDriverType::sqlite:
            return "QSQLITE";
        case RdbmsDriverType::mysql:
            return "QMYSQL";
        case RdbmsDriverType::postgresql:
            return "QPSQL";
        case RdbmsDriverType::oracle:
            return "QOCI";
        default:
            return "bad_driver_name";
    }
}

RdbmsDriverType rdbmsDriverTypeFromString(const std::string_view& str)
{
    if (str == "QSQLITE")
        return RdbmsDriverType::sqlite;
    else if (str == "QMYSQL")
        return RdbmsDriverType::mysql;
    else if (str == "QPSQL")
        return RdbmsDriverType::postgresql;
    else if (str == "QOCI")
        return RdbmsDriverType::oracle;
    else
        return RdbmsDriverType::unknown;
}

//-------------------------------------------------------------------------------------------------

namespace {

static constexpr char kDbDriverName[] = "driverName";
static constexpr char kDbHostName[] = "hostName";
static constexpr char kDbPort[] = "port";
static constexpr char kDbName[] = "name";
static constexpr char kDbUserName[] = "userName";
static constexpr char kDbPassword[] = "password";
static constexpr char kDbConnectOptions[] = "connectOptions";
static constexpr char kDbEncoding[] = "encoding";
static constexpr char kDbMaxConnections[] = "maxConnections";
static constexpr char kDbInactivityTimeout[] = "inactivityTimeout";

static constexpr char kDbMaxPeriodQueryWaitsForAvailableConnection[] =
    "maxPeriodQueryWaitsForAvailableConnection";

static constexpr char kDbFailOnDbTuneError[] = "failOnDbTuneError";
static constexpr char kDbConcurrentModificationQueryLimit[] = "concurrentModificationQueryLimit";

} // namespace

ConnectionOptions::ConnectionOptions():
    hostName("127.0.0.1"),
    encoding("utf8")
{
}

void ConnectionOptions::loadFromSettings(const QnSettings& settings, const QString& groupName)
{
    using namespace std::chrono;

    QnSettingsGroupReader settingsReader(settings, groupName);

    if (settingsReader.contains(kDbDriverName))
    {
        driverType = rdbmsDriverTypeFromString(
            settingsReader.value(kDbDriverName).toString().toStdString().c_str());
    }

    if (settingsReader.contains(kDbHostName))
        hostName = settingsReader.value(kDbHostName).toString();

    if (settingsReader.contains(kDbPort))
        port = settingsReader.value(kDbPort).toInt();

    if (settingsReader.contains(kDbName))
        dbName = settingsReader.value(kDbName).toString();

    if (settingsReader.contains(kDbUserName))
        userName = settingsReader.value(kDbUserName).toString();

    if (settingsReader.contains(kDbPassword))
        password = settingsReader.value(kDbPassword).toString();

    if (settingsReader.contains(kDbConnectOptions))
        connectOptions = settingsReader.value(kDbConnectOptions).toString();

    if (settingsReader.contains(kDbEncoding))
        encoding = settingsReader.value(kDbEncoding).toString();

    if (settingsReader.contains(kDbMaxConnections))
    {
        maxConnectionCount = settingsReader.value(kDbMaxConnections).toInt();
        if (maxConnectionCount <= 0)
            maxConnectionCount = std::thread::hardware_concurrency();
    }

    if (settingsReader.contains(kDbInactivityTimeout))
    {
        inactivityTimeout = duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                settingsReader.value(kDbInactivityTimeout).toString()));
    }

    if (settingsReader.contains(kDbMaxPeriodQueryWaitsForAvailableConnection))
    {
        maxPeriodQueryWaitsForAvailableConnection = duration_cast<seconds>(
            nx::utils::parseTimerDuration(settingsReader.value(kDbMaxPeriodQueryWaitsForAvailableConnection).toString()));
    }

    if (settingsReader.contains(kDbFailOnDbTuneError))
        failOnDbTuneError = settingsReader.value(kDbFailOnDbTuneError).toBool();

    if (settingsReader.contains(kDbConcurrentModificationQueryLimit))
        concurrentModificationQueryLimit = settingsReader.value(kDbConcurrentModificationQueryLimit).toInt();
}

//-------------------------------------------------------------------------------------------------

Exception::Exception(DBResult dbResult):
    base_type(toString(dbResult)),
    m_dbResult(dbResult)
{
}

Exception::Exception(
    DBResult dbResult,
    const std::string& errorDescription)
    :
    base_type(errorDescription),
    m_dbResult(dbResult)
{
}

DBResult Exception::dbResult() const
{
    return m_dbResult;
}

} // namespace nx::sql
