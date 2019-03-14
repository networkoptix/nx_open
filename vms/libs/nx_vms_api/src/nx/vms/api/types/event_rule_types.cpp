#include "event_rule_types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, EventState,
    (nx::vms::api::EventState::active, "Active")
    (nx::vms::api::EventState::inactive, "Inactive")
    (nx::vms::api::EventState::undefined, "Undefined"))

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, EventReason)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, EventType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ActionType)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (nx::vms::api::EventReason) \
    (nx::vms::api::EventType) \
    (nx::vms::api::ActionType) \
    (nx::vms::api::EventState),
    (numeric)(debug))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::EventLevel, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, EventLevel,
    (nx::vms::api::UndefinedEventLevel, "")
    (nx::vms::api::InfoEventLevel, "info")
    (nx::vms::api::WarningEventLevel, "warning")
    (nx::vms::api::ErrorEventLevel, "error"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::EventLevels, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, EventLevels,
    (nx::vms::api::UndefinedEventLevel, "")
    (nx::vms::api::InfoEventLevel, "info")
    (nx::vms::api::WarningEventLevel, "warning")
    (nx::vms::api::ErrorEventLevel, "error"))