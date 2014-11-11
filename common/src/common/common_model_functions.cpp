#include "common_globals.h"

#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCommand)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzObjectType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCapabilities)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, MotionType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, SecondStreamQuality)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, StatisticsDeviceType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PanicMode)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PeerType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ResourceStatus)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ServerFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, CameraStatusFlags)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::PtzTraits,
    (Qn::FourWayPtzTrait,          "FourWayPtz")
    (Qn::EightWayPtzTrait,         "EightWayPtz")
    (Qn::ManualAutoFocusPtzTrait,  "ManualAutoFocus")
)

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
    (Qn::UbjsonFormat,      "ubjson")
    (Qn::BnsFormat,         "bns")
    (Qn::CsvFormat,         "csv")
    (Qn::XmlFormat,         "xml")
)


