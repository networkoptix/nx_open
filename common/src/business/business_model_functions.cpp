#include "business_fwd.h"

#include <utils/common/enum_name_mapper.h>
#include <utils/serialization/json_functions.h>
#include <utils/serialization/lexical_functions.h>

QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(QnBusiness, EventReason)
QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(QnBusiness, EventState)
QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(QnBusiness, EventType)
QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(QnBusiness, ActionType)

QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnBusiness::EventReason)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnBusiness::EventState)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnBusiness::EventType)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnBusiness::ActionType)

