#include "common_globals.h"

#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCommand)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzObjectType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, MotionType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, SecondStreamQuality)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PanicMode)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, RecordingType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PropertyDataType)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::StreamQuality, 
    (Qn::QualityLowest,     "lowest")
    (Qn::QualityLow,        "low")
    (Qn::QualityNormal,     "normal")
    (Qn::QualityHigh,       "high")
    (Qn::QualityHighest,    "highest")
    (Qn::QualityPreSet,     "preset")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::SerializationFormat,
    (Qn::JsonFormat,        "json")
    (Qn::BnsFormat,         "bns")
    (Qn::CsvFormat,         "csv")
)


QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::PtzObjectType)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::PtzCommand)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::PtzCoordinateSpace)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::MotionType)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::StreamQuality)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::SecondStreamQuality)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::PanicMode)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::RecordingType)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::PropertyDataType)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::SerializationFormat)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qn::ServerFlags)
