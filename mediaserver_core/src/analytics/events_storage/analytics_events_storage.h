#pragma once

#include <vector>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include <analytics/common/object_detection_metadata.h>

#include "analytics_events_storage_cursor.h"
#include "analytics_events_storage_settings.h"
#include "analytics_events_storage_types.h"

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

    virtual void store(
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
     * This is actually a convenience method on top of AbstractEventsStorage::createLookupCursor.
     */
    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) = 0;

    /**
     * In some undefined time after this call deprecated data will be removed from persistent storage.
     */
    virtual void deprecateData(
        boost::optional<QnUuid> deviceId,
        qint64 oldestNeededDataTimestamp) = 0;
};

//-------------------------------------------------------------------------------------------------

class EventsStorage:
    public AbstractEventsStorage
{
public:
    EventsStorage(const Settings& settings);

    virtual void store(
        common::metadata::ConstDetectionMetadataPacketPtr packet,
        StoreCompletionHandler completionHandler) override;

    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) override;

    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) override;

    virtual void deprecateData(
        boost::optional<QnUuid> deviceId,
        qint64 oldestNeededDataTimestamp) override;

private:
    const Settings& m_settings;
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
