#pragma once

#include <QtCore/QString>
#include <QtCore/QFlag>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/mediaserver_plugins/utils/plugin_manifest_base.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

struct Hanwha //< Used as a namespace; struct is required for Qt serialization of inner types.
{
    Q_GADGET
    Q_ENUMS(EventItemType)

public:

    enum class EventItemType
    {
        // TODO: #dmishin: Populate with proper event item types.
        none,
        t1,
        t2,
        t3,
    };

    struct EventType: public nx::vms::api::analytics::EventType
    {
        QString internalName;
        QString internalMonitoringName;
        QString description;
        QString positiveState;
        QString negativeState;
        QString regionDescription;
    };
    #define HanwhaEventType_Fields EventType_Fields \
        (internalName) \
        (internalMonitoringName) \
        (description) \
        (positiveState) \
        (negativeState) \
        (regionDescription)

    struct PluginManifest: nx::mediaserver_plugins::utils::PluginManifestBase
    {
        QList<EventType> outputEventTypes;

        QString eventTypeIdByName(const QString& eventName) const;
        const Hanwha::EventType& eventTypeDescriptorById(const QString& id) const;

    private:
        mutable QMap<QString, QString> m_eventTypeIdByInternalName;
        mutable QMap<QString, EventType> m_eventTypeDescriptorById;

    };
    #define HanwhaPluginManifest_Fields PluginManifestBase_Fields (outputEventTypes)
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Hanwha::EventItemType)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Hanwha::EventType)
    (Hanwha::PluginManifest),
    (json)
)

struct Event
{
    QString typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    Hanwha::EventItemType itemType; //< e.g Gunshot for sound classification.
    QString fullEventName;
};

using EventList = std::vector<Event>;

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::mediaserver_plugins::metadata::hanwha::Hanwha::EventItemType),
    (metatype)(numeric)(lexical)
)
