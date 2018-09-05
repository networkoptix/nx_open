#pragma once

#include <nx/api/common/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

struct /*NX_PLUGIN_UTILS_API*/ Analytics
{
    Q_GADGET
    Q_ENUMS(EventTypeFlag)
    Q_FLAGS(EventTypeFlags)

  public:
    enum EventTypeFlag
    {
        noFlags = 0,
        stateDependent = 1 << 0,  //< Prolonged event with active and non-active states.
        regionDependent = 1 << 1, //< Event has reference to a region.
    };
    Q_DECLARE_FLAGS(EventTypeFlags, EventTypeFlag)

    struct NamedItem
    {
        QString id;
        TranslatableString name;
    };
    #define AnalyticsNamedItem_Fields (id)(name)

    /**
     * Description of the analytics event.
     */
    struct EventType: NamedItem
    {
        EventTypeFlags flags = EventTypeFlag::noFlags;
        QString groupId;
        bool isStateful() const noexcept { return flags.testFlag(EventTypeFlag::stateDependent); }
    };
    #define AnalyticsEventType_Fields AnalyticsNamedItem_Fields (flags)(groupId)

    /**
     * Description of the analytics object.
     */
    struct ObjectType: NamedItem
    {
    };
    #define AnalyticsObjectType_Fields AnalyticsNamedItem_Fields

    /**
     * Description of the analytics event groups.
     */
    struct Group: NamedItem
    {
    };
    #define AnalyticsEventGroup_Fields AnalyticsNamedItem_Fields
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Analytics::EventTypeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Analytics::EventTypeFlag)

bool /*NX_PLUGIN_UTILS_API*/ operator==(
    const Analytics::EventType& lh, const Analytics::EventType& rh);

QN_FUSION_DECLARE_FUNCTIONS(Analytics::EventType, (json))
QN_FUSION_DECLARE_FUNCTIONS(Analytics::ObjectType, (json))
QN_FUSION_DECLARE_FUNCTIONS(Analytics::Group, (json))

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::api::Analytics::EventTypeFlag)(nx::api::Analytics::EventTypeFlags),
    (metatype)(numeric)(lexical))
