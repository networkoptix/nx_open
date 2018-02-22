#include "common.h"

#include <QCryptographicHash>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>
//#include <utils/common/id.h>

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

QnUuid guidFromEventName(const char* eventFullName)
{
    // TODO: Use guidFromArbitraryData here after moving QnUuid functions from common to common_libs
    const QString kTrimmedEventFullName = ignoreNamespaces(QString(eventFullName));
    const char* bytesToHash = kTrimmedEventFullName.toUtf8().constData();

    QCryptographicHash md5Hash(QCryptographicHash::Md5);
    md5Hash.addData(bytesToHash);
    QByteArray hashResult = md5Hash.result();
    return QnUuid::fromRfc4122(hashResult);
}

} // namespace

AnalyticsEventType::AnalyticsEventType(const nx::axis::SupportedEvent& supportedEvent)
{
    eventTypeId = guidFromEventName(supportedEvent.fullName().c_str());
    eventName.value = supportedEvent.description.c_str();
    if (eventName.value.simplified().isEmpty())
    {
        eventName.value = supportedEvent.fullName().c_str();
        eventName.value = ignoreNamespaces(eventName.value);
    }
    flags = (supportedEvent.stateful)
        ? nx::api::Analytics::EventTypeFlag::stateDependent
        : nx::api::Analytics::EventTypeFlag::noFlags;

    topic = supportedEvent.topic.c_str();
    name = supportedEvent.name.c_str();
    eventTypeIdExternal = nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(eventTypeId);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEventType, (json),
    AxisAnalyticsEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsDriverManifest, (json),
    AxisAnalyticsDriverManifest_Fields, (brief, true))

} // axis
} // metadata
} // mediaserver_plugins
} // nx

