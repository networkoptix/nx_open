#include "identified_supported_event.h"

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

} // namespace

namespace nx {
namespace mediaserver {
namespace plugins {

IdentifiedSupportedEvent::IdentifiedSupportedEvent(const nx::axis::SupportedEvent& supportedEvent) :
    nx::axis::SupportedEvent(supportedEvent)
{
    m_internalTypeId = guidFromEventName(supportedEvent.fullName().c_str());
    m_externalTypeId = nxpt::fromQnUuidToPluginGuid(m_internalTypeId);
}

QString serializeEvent(const IdentifiedSupportedEvent& identifiedSupportedEvent)
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

    const auto& supportedEvent = identifiedSupportedEvent.base();
    const QString description = !supportedEvent.description.empty() ?
        QString(supportedEvent.description.c_str()) :
        ignoreNamespaces(QString(supportedEvent.name.c_str()));

    return lm(kEventPattern).args(
        identifiedSupportedEvent.internalTypeId().toSimpleString(),
        description,
        supportedEvent.fullName(),
        supportedEvent.stateful ? kStateful : kStateless);
}

QString serializeEvents(const QList<IdentifiedSupportedEvent>& identifiedSupportedEvents)
{
    QStringList serializedEvents;
    for (const auto& identifiedSupportedEvent : identifiedSupportedEvents)
    {
        serializedEvents << serializeEvent(identifiedSupportedEvent);
    }
    return serializedEvents.join(',');
}

} // plugins
} // mediaserver
} // nx

