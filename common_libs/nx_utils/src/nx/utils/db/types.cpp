#include "types.h"

#include <chrono>
#include <thread>

#include <nx/fusion/model_functions.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/settings.h>

namespace nx {
namespace utils {
namespace db {

QString toString(DBResult result)
{
    return QnLexical::serialized(result);
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
        driverType = QnLexical::deserialized<nx::utils::db::RdbmsDriverType>(
            settings.value(kDbDriverName).toString(),
            nx::utils::db::RdbmsDriverType::unknown);
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
    base_type(QnLexical::serialized(dbResult).toStdString()),
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

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::utils::db, RdbmsDriverType,
    (nx::utils::db::RdbmsDriverType::unknown, "bad_driver_name")
    (nx::utils::db::RdbmsDriverType::sqlite, "QSQLITE")
    (nx::utils::db::RdbmsDriverType::mysql, "QMYSQL")
    (nx::utils::db::RdbmsDriverType::postgresql, "QPSQL")
    (nx::utils::db::RdbmsDriverType::oracle, "QOCI")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::utils::db, DBResult,
    (nx::utils::db::DBResult::ok, "ok")
    (nx::utils::db::DBResult::statementError, "statementError")
    (nx::utils::db::DBResult::ioError, "ioError")
    (nx::utils::db::DBResult::notFound, "notFound")
    (nx::utils::db::DBResult::cancelled, "cancelled")
    (nx::utils::db::DBResult::retryLater, "retryLater")
    (nx::utils::db::DBResult::uniqueConstraintViolation, "uniqueConstraintViolation")
    (nx::utils::db::DBResult::connectionError, "connectionError")
)
