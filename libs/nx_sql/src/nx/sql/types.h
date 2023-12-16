// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>

class QnSettings;

namespace nx::sql {

NX_REFLECTION_ENUM_CLASS(DBResultCode,
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
    endOfData
);

struct NX_SQL_API DBResult
{
    DBResultCode code = DBResultCode::ok;
    std::string text;

    DBResult(DBResultCode code = DBResultCode::ok, std::string text = std::string());

    bool ok() const;
    std::string toString() const;

    bool operator==(const DBResult&) const = default;
    bool operator==(DBResultCode) const;

    static DBResult success() { return DBResult(DBResultCode::ok); };
};

//-------------------------------------------------------------------------------------------------

//NX_SQL_API const char* toString(DBResult value);

NX_REFLECTION_ENUM_CLASS(RdbmsDriverType,
    unknown,
    sqlite,
    mysql,
    postgresql,
    oracle
);

NX_SQL_API const char* toString(RdbmsDriverType value);
NX_SQL_API RdbmsDriverType rdbmsDriverTypeFromString(const std::string_view& str);

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
     * it will be cancelled with DBResultCode::cancelled error code.
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

    /**
     * Delay between retries to restore a lost DB connection.
     */
    std::chrono::milliseconds reconnectAfterFailureDelay = std::chrono::seconds(1);

    /**
     * If application code tells SQL subsystem that some queries can be aggregated under a single
     * DB transaction, then no more than this amount of queries are aggregated anyway.
     */
    int maxQueriesAggregatedUnderSingleTransaction = 9999;

    ConnectionOptions();

    void loadFromSettings(const QnSettings& settings, const QString& groupName = "db");

    bool operator==(const ConnectionOptions&) const = default;
};

NX_REFLECTION_INSTRUMENT(ConnectionOptions, (driverType)(hostName)(port)(dbName)(userName) \
    (password)(connectOptions)(encoding)(maxConnectionCount)(inactivityTimeout) \
    (maxPeriodQueryWaitsForAvailableConnection)(maxErrorsInARowBeforeClosingConnection) \
    (failOnDbTuneError)(concurrentModificationQueryLimit))

enum class QueryType
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
    Exception(DBResultCode code, const std::string& error);

    DBResult dbResult() const;

private:
    DBResult m_dbResult;
};

//-------------------------------------------------------------------------------------------------
// Statistics.

struct QueryQueueStats
{
    int pendingQueryCount = 0;
    std::chrono::milliseconds oldestQueryAge = std::chrono::milliseconds::zero();
};

NX_REFLECTION_INSTRUMENT(QueryQueueStats, (pendingQueryCount)(oldestQueryAge))

struct DurationStatistics
{
    std::chrono::milliseconds min = std::chrono::milliseconds::max();
    std::chrono::milliseconds max = std::chrono::milliseconds::min();
    std::chrono::milliseconds average = std::chrono::milliseconds::zero();
};

NX_REFLECTION_INSTRUMENT(DurationStatistics, (min)(max)(average))

struct QueryStatistics
{
    std::chrono::milliseconds statisticalPeriod;
    int requestsSucceeded = 0;
    int requestsFailed = 0;
    int requestsCancelled = 0;
    std::size_t dbThreadPoolSize = 0;
    DurationStatistics requestExecutionTimes;
    DurationStatistics waitingForExecutionTimes;
    QueryQueueStats queryQueue;
};

NX_REFLECTION_INSTRUMENT(QueryStatistics,
    (statisticalPeriod)(requestsSucceeded)(requestsFailed)(requestsCancelled) \
    (dbThreadPoolSize)(requestExecutionTimes)(waitingForExecutionTimes)(queryQueue))

} // namespace nx::sql
