#pragma once

#include <vector>

#include <nx/utils/basic_factory.h>
#include <nx/utils/db/query.h>
#include <nx/utils/move_only_func.h>

#include <analytics/common/object_detection_metadata.h>

#include "analytics_events_storage_cursor.h"
#include "analytics_events_storage_settings.h"
#include "analytics_events_storage_types.h"
#include "analytics_events_storage_db_controller.h"

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

using StoreCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/)>;

using LookupCompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode /*resultCode*/,
        std::vector<common::metadata::DetectionMetadataPacket> /*eventsFound*/)>;

using CreateCursorCompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode /*resultCode*/,
        std::unique_ptr<AbstractCursor> /*cursor*/)>;

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
    virtual bool initialize() = 0;

    virtual void save(
        common::metadata::ConstDetectionMetadataPacketPtr packet,
        StoreCompletionHandler completionHandler) = 0;

    /**
     * Newly-created cursor points just before the first element.
     * So, AbstractCursor::next has to be called to get the first element.
     */
    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) = 0;

    /**
     * Selects all events that satisfy to the filter.
     */
    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) = 0;

    /**
     * In some undefined time after this call deprecated data will be removed from persistent storage.
     * @param deviceId Can be null.
     */
    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        qint64 oldestNeededDataTimestamp) = 0;
};

//-------------------------------------------------------------------------------------------------

class EventsStorage:
    public AbstractEventsStorage
{
public:
    EventsStorage(const Settings& settings);

    virtual bool initialize() override;

    virtual void save(
        common::metadata::ConstDetectionMetadataPacketPtr packet,
        StoreCompletionHandler completionHandler) override;

    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) override;

    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) override;

    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        qint64 oldestNeededDataTimestamp) override;

private:
    const Settings& m_settings;
    DbController m_dbController;

    nx::utils::db::DBResult savePacket(
        nx::utils::db::QueryContext*,
        common::metadata::ConstDetectionMetadataPacketPtr packet);
    /**
     * @return Inserted event id.
     * Throws on error.
     */
    std::int64_t insertEvent(
        nx::utils::db::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject);
    /**
     * Throws on error.
     */
    void insertEventAttributes(
        nx::utils::db::QueryContext* queryContext,
        std::int64_t eventId,
        const std::vector<common::metadata::Attribute>& eventAttributes);

    nx::utils::db::DBResult selectEvents(
        nx::utils::db::QueryContext* queryContext,
        const Filter& filter,
        std::vector<common::metadata::DetectionMetadataPacket>* result);
    void loadPackets(
        nx::utils::db::SqlQuery& selectEventsQuery,
        std::vector<common::metadata::DetectionMetadataPacket>* result);
    void loadPacket(
        nx::utils::db::SqlQuery& selectEventsQuery,
        common::metadata::DetectionMetadataPacket* result);
};

//-------------------------------------------------------------------------------------------------

using EventsStorageFactoryFunction =
    std::unique_ptr<AbstractEventsStorage>(const Settings&);

class EventsStorageFuncionFactory:
    public nx::utils::BasicFactory<EventsStorageFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<EventsStorageFactoryFunction>;

public:
    EventsStorageFuncionFactory();

    static EventsStorageFuncionFactory& instance();

private:
    std::unique_ptr<AbstractEventsStorage> defaultFactoryFunction(const Settings&);
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
