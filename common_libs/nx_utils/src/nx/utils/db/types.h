#pragma once

#include <chrono>
#include <stdexcept>

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

class QnSettings;

namespace nx {
namespace utils {
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
    connectionError,
    logicError,
};

NX_UTILS_API const char* toString(DBResult value);

enum class RdbmsDriverType
{
    unknown,
    sqlite,
    mysql,
    postgresql,
    oracle
};

NX_UTILS_API const char* toString(RdbmsDriverType value);
NX_UTILS_API RdbmsDriverType rdbmsDriverTypeFromString(const char* str);

class NX_UTILS_API ConnectionOptions
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
     * NOTE: Set to zero to disable this timeout.
     */
    std::chrono::milliseconds maxPeriodQueryWaitsForAvailableConnection;
    int maxErrorsInARowBeforeClosingConnection;

    ConnectionOptions();

    void loadFromSettings(const QnSettings& settings);

    bool operator==(const ConnectionOptions&) const;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API Exception:
    public std::runtime_error
{
    using base_type = std::runtime_error;

public:
    Exception(DBResult dbResult);
    Exception(DBResult dbResult, const std::string& errorDescription);

    DBResult dbResult() const;

private:
    DBResult m_dbResult;
};

} // namespace db
} // namespace utils
} // namespace nx
