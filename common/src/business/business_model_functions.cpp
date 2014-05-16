#include "business_fwd.h"

#include <utils/serialization/json_functions.h>
#include <utils/serialization/lexical_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, EventReason)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, EventState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, EventType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, ActionType)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (QnBusiness::EventReason)(QnBusiness::EventState)(QnBusiness::EventType)(QnBusiness::ActionType),
    (json_lexical)
)

