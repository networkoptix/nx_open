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

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzObjectType)(Qn::PtzCommand)(Qn::PtzCoordinateSpace)(Qn::MotionType)(Qn::StreamQuality)
    (Qn::SecondStreamQuality)(Qn::PanicMode)(Qn::RecordingType)(Qn::PropertyDataType)(Qn::SerializationFormat),
    (json_lexical)
)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (Qn::ServerFlags)(Qn::PtzDataFields)(Qn::PtzCapabilities),
    (lexical_numeric_enum)(json_numeric_enum)
)
