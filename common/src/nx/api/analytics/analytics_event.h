#pragma once

#include <nx/api/common/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

struct Analytics
{
    Q_GADGET
    Q_ENUMS(EventTypeFlag)
    Q_FLAGS(EventTypeFlags)

public:
    enum EventTypeFlag
    {
        noFlags = 0,
        stateDependent = 1 << 0, //< Prolonged event with active and non-active states.
        regionDependent = 1 << 1, //< Event has reference to a region.
    };
    Q_DECLARE_FLAGS(EventTypeFlags, EventTypeFlag)

    /**
     * Description of the analytics event.
     */
    struct EventType
    {
        QnUuid eventTypeId;
        TranslatableString name;
        EventTypeFlags flags = EventTypeFlag::noFlags;
        bool isStateful() const noexcept { return flags.testFlag(EventTypeFlag::stateDependent); }
    };
};
#define AnalyticsEventType_Fields (eventTypeId)(name)(flags)

Q_DECLARE_OPERATORS_FOR_FLAGS(Analytics::EventTypeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Analytics::EventTypeFlag)

bool operator==(const Analytics::EventType& lh, const Analytics::EventType& rh);

QN_FUSION_DECLARE_FUNCTIONS(Analytics::EventType, (json))

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::api::Analytics::EventTypeFlag)
    (nx::api::Analytics::EventTypeFlags),
    (metatype)(numeric)(lexical))
