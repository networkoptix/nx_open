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

RdbmsDriverType rdbmsDriverTypeFromString(const char* str)
{
    if (strcmp(str, "QSQLITE") == 0)
        return RdbmsDriverType::sqlite;
    else if (strcmp(str, "QMYSQL") == 0)
        return RdbmsDriverType::mysql;
    else if (strcmp(str, "QPSQL") == 0)
        return RdbmsDriverType::postgresql;
    else if (strcmp(str, "QOCI") == 0)
        return RdbmsDriverType::oracle;
    else
        return RdbmsDriverType::unknown;
}

//-------------------------------------------------------------------------------------------------

namespace {

const QLatin1String kDbDriverName("driverName");

const QLatin1String kDbHostName("hostName");

const QLatin1String kDbPort("port");

const QLatin1String kDbName("name");

const QLatin1String kDbUserName("userName");

const QLatin1String kDbPassword("password");

const QLatin1String kDbConnectOptions("connectOptions");

const QLatin1String kDbEncoding("encoding");

const QLatin1String kDbMaxConnections("maxConnections");

const QLatin1String kDbInactivityTimeout("inactivityTimeout");

const QLatin1String kDbMaxPeriodQueryWaitsForAvailableConnection(
    "maxPeriodQueryWaitsForAvailableConnection");

} // namespace

ConnectionOptions::ConnectionOptions():
    driverType(RdbmsDriverType::sqlite),
    hostName("127.0.0.1"),
    port(3306),
    encoding("utf8"),
    maxConnectionCount(1),
    inactivityTimeout(std::chrono::minutes(10)),
    maxPeriodQueryWaitsForAvailableConnection(std::chrono::minutes(1)),
    maxErrorsInARowBeforeClosingConnection(7)
{
}

void ConnectionOptions::loadFromSettings(const QnSettings& settings, const QString& groupName)
{
    using namespace std::chrono;

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbDriverName); settings.contains(str))
    {
        driverType = rdbmsDriverTypeFromString(
            settings.value(str).toString().toStdString().c_str());
    }

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbHostName); settings.contains(str))
        hostName = settings.value(str).toString();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbPort); settings.contains(str))
        port = settings.value(str).toInt();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbName); settings.contains(str))
        dbName = settings.value(str).toString();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbUserName); settings.contains(str))
        userName = settings.value(str).toString();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbPassword); settings.contains(str))
        password = settings.value(str).toString();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbConnectOptions); settings.contains(str))
        connectOptions = settings.value(str).toString();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbEncoding); settings.contains(str))
        encoding = settings.value(str).toString();

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbMaxConnections); settings.contains(str))
    {
        maxConnectionCount = settings.value(str).toInt();
        if (maxConnectionCount <= 0)
            maxConnectionCount = std::thread::hardware_concurrency();
    }

    if (auto str = nx::format("%1/%2").arg(groupName).arg(kDbInactivityTimeout); settings.contains(str))
    {
        inactivityTimeout = duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                settings.value(str).toString()));
    }

    if (
        auto str = nx::format("%1/%2").arg(groupName).arg(kDbMaxPeriodQueryWaitsForAvailableConnection);
        settings.contains(str))
    {
        maxPeriodQueryWaitsForAvailableConnection = duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                settings.value(str).toString()));
    }
}

bool ConnectionOptions::operator==(const ConnectionOptions& rhs) const
{
    return
        driverType == rhs.driverType &&
        hostName == rhs.hostName &&
        port == rhs.port &&
        dbName == rhs.dbName &&
        userName == rhs.userName &&
        password == rhs.password &&
        connectOptions == rhs.connectOptions &&
        encoding == rhs.encoding &&
        maxConnectionCount == rhs.maxConnectionCount &&
        inactivityTimeout == rhs.inactivityTimeout &&
        maxPeriodQueryWaitsForAvailableConnection == rhs.maxPeriodQueryWaitsForAvailableConnection &&
        maxErrorsInARowBeforeClosingConnection == rhs.maxErrorsInARowBeforeClosingConnection;
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
