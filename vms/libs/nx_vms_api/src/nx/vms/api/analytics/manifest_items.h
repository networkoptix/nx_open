#pragma once

#include <nx/vms/api/analytics/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

enum EventTypeFlag
{
    noFlags = 0,
    stateDependent = 1 << 0, //< Prolonged event with active and non-active states.
    regionDependent = 1 << 1, //< Event has reference to a region on a video frame.
};
Q_DECLARE_FLAGS(EventTypeFlags, EventTypeFlag)

struct NamedItem
{
    QString id;
    TranslatableString name;
};
#define NamedItem_Fields (id)(name)

/**
 * Description of the analytics event.
 */
struct EventType: NamedItem
{
    EventTypeFlags flags = EventTypeFlag::noFlags;
    QString groupId;
    bool isStateful() const noexcept { return flags.testFlag(EventTypeFlag::stateDependent); }
};
#define EventType_Fields NamedItem_Fields (flags)(groupId)
uint NX_VMS_API qHash(const EventType& eventType);

/**
 * Description of the analytics object.
 */
struct ObjectType: NamedItem
{
};
#define ObjectType_Fields NamedItem_Fields

/**
 * Named group which is referenced from a "groupId" attribute of other types to group them.
 */
struct Group: NamedItem
{
};
#define Group_Fields NamedItem_Fields

Q_DECLARE_OPERATORS_FOR_FLAGS(EventTypeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventTypeFlag)

bool NX_VMS_API operator==(const EventType& lh, const EventType& rh);

QN_FUSION_DECLARE_FUNCTIONS(EventType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(Group, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EventTypeFlag, (metatype)(numeric)(lexical),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EventTypeFlags, (metatype)(numeric)(lexical),
    NX_VMS_API)
        