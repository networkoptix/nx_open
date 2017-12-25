#include "hikvision_bytestream_filter.h"
#include "hikvision_common.h"
#include "hikvision_string_helper.h"

#include <iostream>
#include "hikvision_attributes_parser.h"

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

namespace {

static const std::chrono::seconds kExpiredEventTimeout(5);


} // namespace

BytestreamFilter::BytestreamFilter(
    const Hikvision::DriverManifest& manifest,
    BytestreamFilter::Handler handler)
    :
    m_manifest(manifest),
    m_handler(handler)
{
}

BytestreamFilter::~BytestreamFilter()
{

}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_ASSERT(m_handler, lit("No handler is set."));
    if (!m_handler)
        return false;

    bool success = false;
    using namespace nx::mediaserver::plugins::hikvision;
    auto hikvisionEvent = AttributesParser::parseEventXml(buffer, m_manifest, &success);
    if (!success)
        return false;
    std::vector<HikvisionEvent> result;
    if (!hikvisionEvent.typeId.isNull())
        result.push_back(hikvisionEvent);

    auto getEventKey = [](const HikvisionEvent& event)
    {
        QString result = event.typeId.toString();
        if (event.region)
            result += QString::number(*event.region) + lit("_");
        if (event.channel)
            result += QString::number(*event.channel);
        return result;
    };

    auto eventDescriptor = m_manifest.eventDescriptorById(hikvisionEvent.typeId);
    if (eventDescriptor.flags.testFlag(Hikvision::EventTypeFlag::stateDependent))
    {
        QString key = getEventKey(hikvisionEvent);
        if (hikvisionEvent.isActive)
            m_startedEvents[key] = StartedEvent(hikvisionEvent);
        else
            m_startedEvents.remove(key);
    }
    addExpiredEvents(result);

    if (!result.empty())
        m_handler(result);
    return true;
}

void BytestreamFilter::addExpiredEvents(std::vector<HikvisionEvent>& result)
{
    for (auto itr = m_startedEvents.begin(); itr != m_startedEvents.end();)
    {
        if (itr.value().timer.hasExpired(kExpiredEventTimeout))
        {
            auto& event = itr.value().event;
            event.isActive = false;
            event.caption = HikvisionStringHelper::buildCaption(m_manifest, event);
            event.description = HikvisionStringHelper::buildDescription(m_manifest, event);
            result.push_back(std::move(itr.value().event));
            itr = m_startedEvents.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
