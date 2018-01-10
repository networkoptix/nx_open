#include "axis_common.h"

#include <plugins/plugin_internal_tools.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>
#include <utils/common/id.h>

namespace {

using namespace nx::mediaserver::plugins;

QString ignoreNamespace(const QString& tag)
{
    // "ns:tag" -> "tag"
    // "tag" -> "tag"
    const auto parts = tag.split(':');
    return parts.back();
}

QString ignoreNamespaces(const QString& tags)
{
    // "ns1:tag1/ns2:tag2.tag3" -> "tag1/tag2/tag3"
    // "tag1/ns2:tag2" -> "tag1/tag2"
    const QStringList fullParts = tags.split('/');
    QStringList shortParts;
    for (const auto& part: fullParts)
    {
        shortParts << ignoreNamespace(part);
    }
    return shortParts.join('/');
}

QnUuid guidFromEventName(const char* eventFullName)
{
    const QString line(eventFullName);
    return guidFromArbitraryData(ignoreNamespaces(line).toLatin1());
}

QnUuid guidFromEvent(const AxisEvent& axisEvent)
{
    const QString line(axisEvent.fullEventName);
    return guidFromArbitraryData(ignoreNamespaces(line).toLatin1());
}

void InitTypeId(AxisEvent& axisEvent)
{
    QnUuid uuid = guidFromEvent(axisEvent);
    axisEvent.typeId = nxpt::fromQnUuidToPluginGuid(uuid);
}

} // namespace

namespace nx {
namespace mediaserver {
namespace plugins {

QString serializeEvent(const AxisEvent& event)
{
    static const QString kEventPattern = R"json(
    {
        "eventTypeId": "%1",
        "eventName": {
            "value": "%2",
            "localization": {
            }
        },
        "internalName": "%3",
        "stateMode": "%4",
        "_": "machine-generated",
        "source": "Axis P1405-E"
    }
    )json";
    static const QString kStateful = "stateful";
    static const QString kStateless = "stateless";

    return lm(kEventPattern).args(
        nxpt::fromPluginGuidToQnUuid(event.typeId).toSimpleString(),
        !event.description.isEmpty() ? event.description : event.fullEventName.split('/').back(),
        event.fullEventName,
        event.isStatefull ? kStateful : kStateless);
}

QString serializeEvents(const QList<AxisEvent>& events)
{
    QStringList serializedEvents;
    for (const auto event: events)
    {
        serializedEvents << serializeEvent(event);
    }
    return serializedEvents.join(',');
}

AxisEvent convertEvent(const nx::axis::SupportedEvent& supportedEvent)
{
    AxisEvent axisEvent;
    axisEvent.caption = supportedEvent.name.c_str();
    axisEvent.description = supportedEvent.description.c_str();
    axisEvent.fullEventName = (supportedEvent.topic + '/' + supportedEvent.name).c_str();
    axisEvent.isStatefull = supportedEvent.stateful;
    InitTypeId(axisEvent);
    return axisEvent;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Axis::EventDescriptor, (json), EventDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Axis::DriverManifest, (json), DriverManifest_Fields)

} // plugins
} // mediaserver
} // nx
