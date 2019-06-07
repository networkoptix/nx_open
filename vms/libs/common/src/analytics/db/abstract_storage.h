#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>

#include <recording/time_period_list.h>

#include "abstract_cursor.h"
#include "analytics_db_settings.h"
#include "analytics_db_types.h"

namespace nx::analytics::db {

using StoreCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/)>;

using LookupCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/, LookupResult)>;

using TimePeriodsLookupCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/, QnTimePeriodList)>;

using CreateCursorCompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode /*resultCode*/,
        std::shared_ptr<AbstractCursor> /*cursor*/)>;

/**
 * NOTE: Every method of this class is asynchronous if other not specified.
 * Generally, any error does not mean storage cannot continue.
 * New operations may be completed successfully.
 */
class AbstractEventsStorage
{
public:
    virtual ~AbstractEventsStorage() = default;

    /**
     * Initializes internal database. MUST be called just after object instantiation.
     */
    virtual bool initialize(const Settings& settings) = 0;

    /**
     * Packet is saved asynchronously.
     * To make sure the data is written call AbstractEventsStorage::flush.
     */
    virtual void save(common::metadata::ConstDetectionMetadataPacketPtr packet) = 0;

    /**
     * Newly-created cursor points just before the first element.
     * So, AbstractCursor::next has to be called to get the first element.
     */
    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) = 0;

    /**
     * Close created cursor.
     */
    virtual void closeCursor(const std::shared_ptr<AbstractCursor>& cursor) = 0;

    /**
     * Selects all objects with non-empty track that satisfy to the filter.
     * Output is sorted by timestamp with order defined by filter.sortOrder.
     */
    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) = 0;

    /**
     * Select periods of time which contain any data suitable for filter. If some periods in the
     * result period list have zero duration that means their durations are unspecified.
     */
    virtual void lookupTimePeriods(
        Filter filter,
        TimePeriodsLookupOptions options,
        TimePeriodsLookupCompletionHandler completionHandler) = 0;

    /**
     * In some undefined time after this call deprecated data will be removed from persistent storage.
     * NOTE: Call is non-blocking.
     * @param deviceId Can be null.
     */
    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp) = 0;

    /**
     * Flushes to the DB all data passed with AbstractEventsStorage::save before this call.
     */
    virtual void flush(StoreCompletionHandler completionHandler) = 0;

    /**
     * Select minumal timestamp of the data in the database.
     */
    virtual bool readMinimumEventTimestamp(std::chrono::milliseconds* outResult) = 0;
};

} // namespace nx::analytics::db
