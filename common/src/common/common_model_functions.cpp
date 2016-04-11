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
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, RebuildState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, BackupState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PeerType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ResourceStatus)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, BitratePerGopType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ServerFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, Permissions)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, GlobalPermissions)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, TimeFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, CameraStatusFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, TTHeaderFlag)
//QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, IOPortType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, IOPortTypes)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, AuditRecordType)
//QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, IODefaultState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, AuthResult)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::IOPortType,
(Qn::PT_Unknown,  "Unknown")
(Qn::PT_Disabled, "Disabled")
(Qn::PT_Input,    "Input")
(Qn::PT_Output,   "Output")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::IODefaultState,
(Qn::IO_OpenCircuit,  "Open Circuit")
(Qn::IO_GroundedCircuit, "Grounded circuit")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::FailoverPriority,
(Qn::FP_Never,      "Never")
(Qn::FP_Low,        "Low")
(Qn::FP_Medium,     "Medium")
(Qn::FP_High,       "High")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::PtzTraits,
    (Qn::FourWayPtzTrait,          "FourWayPtz")
    (Qn::EightWayPtzTrait,         "EightWayPtz")
    (Qn::ManualAutoFocusPtzTrait,  "ManualAutoFocus")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::BackupType,
    (Qn::Backup_Manual,            "BackupManual")
    (Qn::Backup_RealTime,          "BackupRealTime")
    (Qn::Backup_Schedule,          "BackupSchedule")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::CameraBackupQuality,
    (Qn::CameraBackup_Disabled,          "CameraBackupDisabled")
    (Qn::CameraBackup_HighQuality,       "CameraBackupHighQuality")
    (Qn::CameraBackup_LowQuality,        "CameraBackupLowQuality")
    (Qn::CameraBackup_Both,              "CameraBackupBoth")
)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, CameraBackupQualities)


// TODO: #Elric #2.3 code duplication ^v
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::PtzTrait,
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
    (Qn::CompressedPeriodsFormat, "periods")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::BookmarkSortField,
    (Qn::BookmarkName,          "name")
    (Qn::BookmarkStartTime,     "startTime")
    (Qn::BookmarkDuration,      "duration")
    (Qn::BookmarkTags,          "tags")
    (Qn::BookmarkCameraName,    "cameraName")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qt::SortOrder,
    (Qt::AscendingOrder,     "asc")
    (Qt::DescendingOrder,    "desc")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::RebuildAction,
(Qn::RebuildAction_Start,   "start")
(Qn::RebuildAction_Cancel,  "stop")
(Qn::RebuildAction_ShowProgress, QString())
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::BackupAction,
(Qn::BackupAction_Start,   "start")
(Qn::BackupAction_Cancel,  "stop")
(Qn::BackupAction_ShowProgress, QString())
)
