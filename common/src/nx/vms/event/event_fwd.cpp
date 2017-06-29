#include "event_fwd.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::event, EventState,
    (nx::vms::event::EventState::active, "Active")
    (nx::vms::event::EventState::inactive, "Inactive")
    (nx::vms::event::EventState::undefined, "Undefined"))

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::event, EventReason)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::event, EventType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms::event, ActionType)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(QN_VMS_EVENT_ENUM_TYPES, (numeric))
