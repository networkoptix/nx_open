#include "common.h"

#include <QCryptographicHash>

#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace axis {

namespace {

using namespace nx::vms_server_plugins::analytics::axis;

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
    name = supportedEventType.description.c_str();
    if (name.simplified().isEmpty())
    {
        name = supportedEventType.fullName().c_str();
        name = ignoreNamespaces(name);
    }

    id = supportedEventType.fullName().c_str();
//    id = ignoreNamespaces(id);
//    id.replace(QChar('/'), QChar('-'));
    id = QString("nx.axis.") + id;

    flags = (supportedEventType.stateful)
        ? nx::vms::api::analytics::EventTypeFlag::stateDependent
        : nx::vms::api::analytics::EventTypeFlag::noFlags;

    topic = supportedEventType.topic.c_str();
    caption = supportedEventType.name.c_str();
    eventTypeIdExternal = id;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    AxisEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    AxisEngineManifest_Fields, (brief, true))

} // namespace axis
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
