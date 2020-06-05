#pragma once

#include "abstract_storage.h"

class QnCommonModule;

namespace nx::analytics::db {

class AbstractIframeSearchHelper;

class NX_ANALYTICS_DB_API MovableAnalyticsDb:
    public AbstractEventsStorage
{
public:
    using ActualAnalyticsDbFactoryFunc =
        nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractEventsStorage>()>;

    MovableAnalyticsDb(ActualAnalyticsDbFactoryFunc func);

    virtual InitResult initialize(const Settings& settings) override;

    virtual bool initialized() const override;

    virtual void save(common::metadata::ConstObjectMetadataPacketPtr packet) override;

    virtual std::vector<ObjectPosition> lookupTrackDetailsSync(const ObjectTrack& track) override;

    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) override;

    virtual void lookupTimePeriods(
        Filter filter,
        TimePeriodsLookupOptions options,
        TimePeriodsLookupCompletionHandler completionHandler) override;

    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp) override;

    virtual void flush(StoreCompletionHandler completionHandler) override;

    virtual bool readMinimumEventTimestamp(std::chrono::milliseconds* outResult) override;

    virtual std::optional<nx::sql::QueryStatistics> statistics() const override;

private:
    ActualAnalyticsDbFactoryFunc m_factoryFunc;
    mutable QnMutex m_mutex;
    std::shared_ptr<AbstractEventsStorage> m_db;

    std::shared_ptr<AbstractEventsStorage> getDb();
    std::shared_ptr<const AbstractEventsStorage> getDb() const;
};

} // namespace nx::analytics::db
