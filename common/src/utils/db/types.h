#pragma once

#include <chrono>

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace db {

enum class DBResult
{
    ok,
    statementError,
    ioError,
    notFound,
    cancelled,
    retryLater,
};


enum class RdbmsDriverType
{
    unknown,
    sqlite,
    mysql,
    postgresql,
    oracle
};

class ConnectionOptions
{
public:
    RdbmsDriverType driverType;
    QString hostName;
    int port;
    QString dbName;
    QString userName;
    QString password;
    QString connectOptions;
    size_t maxConnectionCount;
    /** Connection is closed if not used for this interval. */
    std::chrono::seconds inactivityTimeout;

    ConnectionOptions():
        driverType(RdbmsDriverType::sqlite),
        port(0),
        maxConnectionCount(1),
        inactivityTimeout(std::chrono::minutes(10))
    {
    }
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::db::DBResult)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::db::RdbmsDriverType)

} // namespace db
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::db::DBResult), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::db::RdbmsDriverType), (lexical))
