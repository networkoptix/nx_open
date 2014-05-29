#include "common_globals.h"

#include <utils/common/json.h>
#include <utils/common/lexical.h>
#include <utils/common/enum_name_mapper.h>

QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING(Qn::PtzTrait,
    ((Qn::FourWayPtzTrait,          "FourWayPtz"))
    ((Qn::EightWayPtzTrait,         "EightWayPtz"))
    ((Qn::VistaSmartFocusPtzTrait,  "VistaSmartFocus"))
)

QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(Qn, PtzCommand)
QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(Qn, PtzObjectType)

QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzCommand)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzCoordinateSpace)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzObjectType)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzTrait)

QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzDataFields)
QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzCapabilities)
