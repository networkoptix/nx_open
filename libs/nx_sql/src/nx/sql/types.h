// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <stdexcept>

#include <nx/reflect/instrument.h>

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

class QnSettings;

namespace nx::sql {

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
    endOfData,
};

NX_SQL_API const char* toString(DBResult value);

NX_REFLECTION_ENUM_CLASS(RdbmsDriverType,
    unknown,
    sqlite,
    mysql,
    postgresql,
    oracle
);

NX_SQL_API const char* toString(RdbmsDriverType value);
NX_SQL_API RdbmsDriverType rdbmsDriverTypeFromString(const char* str);

class NX_SQL_API ConnectionOptions
{
public:
    RdbmsDriverType driverType = RdbmsDriverType::sqlite;
    QString hostName;
    int port = 3306;
    QString dbName;
    QString userName;
    QString password;
    QString connectOptions;
    QString encoding;
    int maxConnectionCount = 1;
    /** Connection is closed if not used for this interval. */
    std::chrono::seconds inactivityTimeout = std::chrono::minutes(10);
    /**
     * If scheduled request has not received DB connection during this timeout
     * it will be cancelled with DBResult::cancelled error code.
     * By default it is one minute.
     * NOTE: Set to zero to disable this timeout.
     */
    std::chrono::milliseconds maxPeriodQueryWaitsForAvailableConnection = std::chrono::minutes(1);
    int maxErrorsInARowBeforeClosingConnection = 7;
    bool failOnDbTuneError = false;
    /**
     * If specified, then no more than this amount of data modification queries will be run concurrently.
     * Otherwise, there is no limit (if DB type supports it).
     */
    std::optional<int> concurrentModificationQueryLimit;

    ConnectionOptions();

    void loadFromSettings(const QnSettings& settings, const QString& groupName = "db");

    bool operator==(const ConnectionOptions&) const = default;
};

NX_REFLECTION_INSTRUMENT(ConnectionOptions, (driverType)(hostName)(port)(dbName)(userName) \
    (password)(connectOptions)(encoding)(maxConnectionCount)(inactivityTimeout) \
    (maxPeriodQueryWaitsForAvailableConnection)(maxErrorsInARowBeforeClosingConnection) \
    (failOnDbTuneError)(concurrentModificationQueryLimit))

enum class NX_SQL_API QueryType
{
    lookup,
    modification,
};

//-------------------------------------------------------------------------------------------------

class NX_SQL_API Exception:
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

} // namespace nx::sql
