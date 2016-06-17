#include "business_fwd.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnBusiness::EventState,
                                          (QnBusiness::ActiveState,     "Active")
                                          (QnBusiness::InactiveState,   "Inactive")
                                          (QnBusiness::UndefinedState,  "Undefined")
);


QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, EventReason)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, EventType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, ActionType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnBusiness, UserGroup)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(QN_BUSINESS_ENUM_TYPES, (numeric))
