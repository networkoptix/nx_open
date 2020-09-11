#include "common.h"

#include <QCryptographicHash>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>

namespace nx::vms_server_plugins::analytics::axis {

namespace {

QString withoutNamespace(const QString& tag)
{
    // "ns:tag" -> "tag"
    // "tag" -> "tag"
    const auto parts = tag.split(':');
    return parts.back();
}

QString withoutNamespaces(const QString& tags)
{
    // "ns1:tag1/ns2:tag2.tag3" -> "tag1/tag2/tag3"
    // "tag1/ns2:tag2" -> "tag1/tag2"
    const QStringList fullParts = tags.split('/');
    QStringList shortParts;
    for (const auto& part: fullParts)
    {
        shortParts << withoutNamespace(part);
    }
    return shortParts.join('/');
}

std::optional<int> parseFenceGuardProfileIndex(QString topic, QString caption)
{
    topic = withoutNamespaces(topic);
    if (topic != "CameraApplicationPlatform")
        return std::nullopt;
    
    static const QString kCaptionPrefix = "FenceGuard/Camera";
    caption = withoutNamespaces(caption);
    if (!caption.startsWith(kCaptionPrefix))
        return std::nullopt;

    const auto splitCaption = caption.mid(kCaptionPrefix.length()).split("Profile");
    if (splitCaption.size() != 2)
        return std::nullopt;

    bool ok = false;
    splitCaption[0].toUInt(&ok);
    if (!ok)
        return std::nullopt;

    if (splitCaption[1] == "ANY")
        return -1;

    int index = splitCaption[1].toUInt(&ok);
    if (ok)
        return index;

    return std::nullopt;
}

} // namespace

EventType::EventType(const nx::axis::SupportedEventType& supportedEventType)
{
    name = supportedEventType.description.c_str();
    if (name.simplified().isEmpty())
    {
        name = supportedEventType.fullName().c_str();
        name = withoutNamespaces(name);
    }

    const QString eventFullNameWithoutNamespaces =
        withoutNamespaces(QString::fromStdString(supportedEventType.fullName()));

    id = "nx.axis." + QnUuid::fromArbitraryData(eventFullNameWithoutNamespaces).toString();

    flags = (supportedEventType.stateful)
        ? nx::vms::api::analytics::EventTypeFlag::stateDependent
        : nx::vms::api::analytics::EventTypeFlag::noFlags;

    topic = supportedEventType.topic.c_str();
    caption = supportedEventType.name.c_str();

    fenceGuardProfileIndex = parseFenceGuardProfileIndex(topic, caption);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json),
    AxisEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json),
    AxisEngineManifest_Fields, (brief, true))

} // nx::vms_server_plugins::analytics::axis
