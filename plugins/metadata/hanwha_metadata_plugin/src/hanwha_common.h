#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver {
namespace plugins {

struct Hanwha
{
    Q_GADGET
    Q_ENUMS(EventItemType)

public:

    enum class EventItemType
    {
        // TODO: #dmishin fill proper event item types
        none,
        ein,
        zwei,
        drei
    };

    struct EventDescriptor: public nx::api::Analytics::EventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;
    };
    #define EventDescriptor_Fields AnalyticsEventType_Fields (internalName)\
        (internalMonitoringName)\
        (description)\
        (positiveState)\
        (negativeState)\
        (regionDescription)

    struct DriverManifest: public nx::api::AnalyticsDriverManifestBase
    {
        QList<EventDescriptor> outputEventTypes;

        QnUuid eventTypeByName(const QString& eventName) const;
        const Hanwha::EventDescriptor& eventDescriptorById(const QnUuid& id) const;
    private:
        mutable QMap<QString, QnUuid> m_idByInternalName;
        mutable QMap<QnUuid, EventDescriptor> m_recordById;

    };
    #define DriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hanwha::EventItemType)

struct HanwhaEvent
{
    nxpl::NX_GUID typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    Hanwha::EventItemType itemType; //< e.g Gunshot for sound classification
    QString fullEventName;
};

using HanwhaEventList = std::vector<HanwhaEvent>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hanwha::EventDescriptor)
    (Hanwha::DriverManifest),
    (json)
)

} // namespace plugins
} // namespace mediaserver
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::mediaserver::plugins::Hanwha::EventItemType),
    (metatype)(numeric)(lexical)
)
