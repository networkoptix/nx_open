#include "types.h"

#include <chrono>
#include <thread>

#include <nx/utils/timer_manager.h>
#include <nx/utils/settings.h>

namespace nx {
namespace utils {
namespace db {

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

const QLatin1String kDbDriverName("db/driverName");

const QLatin1String kDbHostName("db/hostName");

const QLatin1String kDbPort("db/port");

const QLatin1String kDbName("db/name");

const QLatin1String kDbUserName("db/userName");

const QLatin1String kDbPassword("db/password");

const QLatin1String kDbConnectOptions("db/connectOptions");

const QLatin1String kDbEncoding("db/encoding");

const QLatin1String kDbMaxConnections("db/maxConnections");

const QLatin1String kDbInactivityTimeout("db/inactivityTimeout");

const QLatin1String kDbMaxPeriodQueryWaitsForAvailableConnection(
    "db/maxPeriodQueryWaitsForAvailableConnection");

} // namespace

ConnectionOptions::ConnectionOptions():
    driverType(RdbmsDriverType::sqlite),
    hostName(lit("127.0.0.1")),
    port(3306),
    encoding(lit("utf8")),
    maxConnectionCount(1),
    inactivityTimeout(std::chrono::minutes(10)),
    maxPeriodQueryWaitsForAvailableConnection(std::chrono::minutes(1)),
    maxErrorsInARowBeforeClosingConnection(7)
{
}

void ConnectionOptions::loadFromSettings(const QnSettings& settings)
{
    using namespace std::chrono;

    if (settings.contains(kDbDriverName))
    {
        const auto str = settings.value(kDbDriverName).toString().toStdString();
        driverType = rdbmsDriverTypeFromString(str.c_str());
    }

    if (settings.contains(kDbHostName))
        hostName = settings.value(kDbHostName).toString();
    if (settings.contains(kDbPort))
        port = settings.value(kDbPort).toInt();
    if (settings.contains(kDbName))
        dbName = settings.value(kDbName).toString();
    if (settings.contains(kDbUserName))
        userName = settings.value(kDbUserName).toString();
    if (settings.contains(kDbPassword))
        password = settings.value(kDbPassword).toString();
    if (settings.contains(kDbConnectOptions))
        connectOptions = settings.value(kDbConnectOptions).toString();
    if (settings.contains(kDbEncoding))
        encoding = settings.value(kDbEncoding).toString();

    if (settings.contains(kDbMaxConnections))
    {
        maxConnectionCount = settings.value(kDbMaxConnections).toInt();
        if (maxConnectionCount <= 0)
            maxConnectionCount = std::thread::hardware_concurrency();
    }

    if (settings.contains(kDbInactivityTimeout))
    {
        inactivityTimeout = duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                settings.value(kDbInactivityTimeout).toString()));
    }

    if (settings.contains(kDbMaxPeriodQueryWaitsForAvailableConnection))
    {
        maxPeriodQueryWaitsForAvailableConnection = duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                settings.value(kDbMaxPeriodQueryWaitsForAvailableConnection).toString()));
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

} // namespace db
} // namespace utils
} // namespace nx
