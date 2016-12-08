#pragma once

#include <chrono>

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

#include <nx/fusion/model_functions_fwd.h>

class QnSettings;

namespace nx {
namespace db {

enum class DBResult
{
    ok,
    statementError,
    ioError,
    notFound,
    /**
     * Business logic decided to cancel operation and rollback transaction. 
     * This is not an error.
     */
    cancelled, 
    retryLater,
    uniqueConstraintViolation,
    connectionError
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
    QString encoding;
    int maxConnectionCount;
    /** Connection is closed if not used for this interval. */
    std::chrono::seconds inactivityTimeout;
    /**
     * If scheduled request has not received DB connection during this timeout 
     * it will be cancelled with DBResult::cancelled error code.
     * By default it is one minute.
     * @note Set to zero to disable this timeout.
     */
    std::chrono::milliseconds maxPeriodQueryWaitsForAvailableConnection;
    int maxErrorsInARowBeforeClosingConnection;

    ConnectionOptions();

    void loadFromSettings(QnSettings* const settings);
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::db::DBResult)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::db::RdbmsDriverType)

} // namespace db
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::db::DBResult), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::db::RdbmsDriverType), (lexical))
