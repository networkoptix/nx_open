#include "analytics_events_storage.h"

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

EventsStorage::EventsStorage(const Settings& settings):
    m_settings(settings)
{
}

void EventsStorage::store(
    common::metadata::ConstDetectionMetadataPacketPtr /*packet*/,
    StoreCompletionHandler /*completionHandler*/)
{
    // TODO
}

void EventsStorage::createLookupCursor(
    Filter /*filter*/,
    CreateCursorCompletionHandler /*completionHandler*/)
{
    // TODO
}

void EventsStorage::lookup(
    Filter /*filter*/,
    LookupCompletionHandler /*completionHandler*/)
{
    // TODO
}

void EventsStorage::deprecateData(
    boost::optional<QnUuid> /*deviceId*/,
    qint64 /*oldestNeededDataTimestamp*/)
{
    // TODO
}

//-------------------------------------------------------------------------------------------------

EventsStorageFuncionFactory::EventsStorageFuncionFactory():
    base_type(std::bind(&EventsStorageFuncionFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

EventsStorageFuncionFactory& EventsStorageFuncionFactory::instance()
{
    static EventsStorageFuncionFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractEventsStorage> EventsStorageFuncionFactory::defaultFactoryFunction(
    const Settings& settings)
{
    return std::make_unique<EventsStorage>(settings);
}

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
