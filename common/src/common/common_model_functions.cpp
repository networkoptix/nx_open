#include "common_globals.h"

#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, PtzCommand)
QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, PtzObjectType)
QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, MotionType)
QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, SecondStreamQuality)
QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, PanicMode)
QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qn, RecordingType)

QN_DEFINE_EXPLICIT_LEXICAL_ENUM_FUNCTIONS(Qn::StreamQuality, 
    (Qn::QualityLowest,     "lowest")
    (Qn::QualityLow,        "low")
    (Qn::QualityNormal,     "normal")
    (Qn::QualityHigh,       "high")
    (Qn::QualityHighest,    "highest")
    (Qn::QualityPreSet,     "preset")
)

QN_DEFINE_EXPLICIT_LEXICAL_ENUM_FUNCTIONS(Qn::SerializationFormat,
    (Qn::JsonFormat,        "json")
    (Qn::BnsFormat,         "bns")
)


QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzCommand)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzCoordinateSpace)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzObjectType)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::MotionType)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::StreamQuality)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::SecondStreamQuality)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PanicMode)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::RecordingType)
QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::SerializationFormat)

QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzDataFields)
QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::PtzCapabilities)
QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::ServerFlags)


