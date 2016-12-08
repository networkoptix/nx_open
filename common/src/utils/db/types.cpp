#include "types.h"

#include <chrono>

#include <nx/fusion/model_functions.h>
#include <nx/utils/timer_manager.h>

#include "utils/common/settings.h"

// Using Qt plugin names here.

namespace nx {
namespace db {

namespace {

//DB options
const QLatin1String kDbDriverName("db/driverName");
const QLatin1String kDefaultDbDriverName("QMYSQL");

const QLatin1String kDbHostName("db/hostName");
const QLatin1String kDefaultDbHostName("127.0.0.1");

const QLatin1String kDbPort("db/port");
const int kDefaultDbPort = 3306;

const QLatin1String kDbName("db/name");
const QLatin1String kDefaultDbName("nx_cloud");

const QLatin1String kDbUserName("db/userName");
const QLatin1String kDefaultDbUserName("root");

const QLatin1String kDbPassword("db/password");
const QLatin1String kDefaultDbPassword("");

const QLatin1String kDbConnectOptions("db/connectOptions");
const QLatin1String kDefaultDbConnectOptions("");

const QLatin1String kDbEncoding("db/encoding");
const QLatin1String kDefaultDbEncoding("utf8");

const QLatin1String kDbMaxConnections("db/maxConnections");
const QLatin1String kDefaultDbMaxConnections("1");

const QLatin1String kDbInactivityTimeout("db/inactivityTimeout");
const std::chrono::seconds kDefaultDbInactivityTimeout = std::chrono::minutes(10);

const QLatin1String kDbMaxPeriodQueryWaitsForAvailableConnection(
    "db/maxPeriodQueryWaitsForAvailableConnection");
const std::chrono::seconds kDefaultDbMaxPeriodQueryWaitsForAvailableConnection =
    std::chrono::minutes(1);

} // namespace

ConnectionOptions::ConnectionOptions():
    driverType(RdbmsDriverType::sqlite),
    port(0),
    maxConnectionCount(1),
    inactivityTimeout(std::chrono::minutes(10)),
    maxPeriodQueryWaitsForAvailableConnection(std::chrono::minutes(1)),
    maxErrorsInARowBeforeClosingConnection(7)
{
}

void ConnectionOptions::loadFromSettings(QnSettings* const settings)
{
    using namespace std::chrono;

    driverType = QnLexical::deserialized<nx::db::RdbmsDriverType>(
        settings->value(kDbDriverName, kDefaultDbDriverName).toString(),
        nx::db::RdbmsDriverType::unknown);

    //< Ignoring error here since connection to DB will not be established anyway.
    hostName = settings->value(kDbHostName, kDefaultDbHostName).toString();
    port = settings->value(kDbPort, kDefaultDbPort).toInt();
    dbName = settings->value(kDbName, kDefaultDbName).toString();
    userName = settings->value(kDbUserName, kDefaultDbUserName).toString();
    password = settings->value(kDbPassword, kDefaultDbPassword).toString();
    connectOptions = settings->value(kDbConnectOptions, kDefaultDbConnectOptions).toString();
    encoding = settings->value(kDbEncoding, kDefaultDbEncoding).toString();

    maxConnectionCount = settings->value(kDbMaxConnections, kDefaultDbMaxConnections).toInt();
    if (maxConnectionCount <= 0)
        maxConnectionCount = std::thread::hardware_concurrency();

    inactivityTimeout = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            settings->value(kDbInactivityTimeout).toString(),
            kDefaultDbInactivityTimeout));

    maxPeriodQueryWaitsForAvailableConnection = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            settings->value(kDbMaxPeriodQueryWaitsForAvailableConnection).toString(),
            kDefaultDbMaxPeriodQueryWaitsForAvailableConnection));
}

} // namespace db
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::db, RdbmsDriverType,
    (nx::db::RdbmsDriverType::unknown, "bad_driver_name")
    (nx::db::RdbmsDriverType::sqlite, "QSQLITE")
    (nx::db::RdbmsDriverType::mysql, "QMYSQL")
    (nx::db::RdbmsDriverType::postgresql, "QPSQL")
    (nx::db::RdbmsDriverType::oracle, "QOCI")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::db, DBResult,
    (nx::db::DBResult::ok, "ok")
    (nx::db::DBResult::statementError, "statementError")
    (nx::db::DBResult::ioError, "ioError")
    (nx::db::DBResult::notFound, "notFound")
    (nx::db::DBResult::cancelled, "cancelled")
    (nx::db::DBResult::retryLater, "retryLater")
    (nx::db::DBResult::uniqueConstraintViolation, "uniqueConstraintViolation")
    (nx::db::DBResult::connectionError, "connectionError")
)
