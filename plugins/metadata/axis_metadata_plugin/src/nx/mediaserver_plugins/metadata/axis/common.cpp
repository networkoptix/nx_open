#include "common.h"

#include <QCryptographicHash>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

namespace {

using namespace nx::mediaserver_plugins::metadata::axis;

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

} // namespace

EventType::EventType(const nx::axis::SupportedEventType& supportedEventType)
{
    const QString eventTypeId = QString::fromLatin1(supportedEventType.fullName().c_str());
    name.value = supportedEventType.description.c_str();
    if (name.value.simplified().isEmpty())
    {
        name.value = supportedEventType.fullName().c_str();
        name.value = ignoreNamespaces(name.value);
    }
    flags = (supportedEventType.stateful)
        ? nx::vms::api::analytics::EventTypeFlag::stateDependent
        : nx::vms::api::analytics::EventTypeFlag::noFlags;

    topic = supportedEventType.topic.c_str();
    caption = supportedEventType.name.c_str();
    eventTypeIdExternal = eventTypeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    AxisEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginManifest, (json),
    AxisPluginManifest_Fields, (brief, true))

} // axis
} // metadata
} // mediaserver_plugins
} // nx
