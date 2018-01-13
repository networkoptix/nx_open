#include "common_globals.h"

#include <core/ptz/ptz_constants.h>

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCommand)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzObjectType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, MotionType)
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
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, MediaStreamEvent);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, IOPortType,
(Qn::PT_Unknown,  "Unknown")
(Qn::PT_Disabled, "Disabled")
(Qn::PT_Input,    "Input")
(Qn::PT_Output,   "Output")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, IODefaultState,
(Qn::IO_OpenCircuit,  "Open Circuit")
(Qn::IO_GroundedCircuit, "Grounded circuit")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, FailoverPriority,
(Qn::FP_Never,      "Never")
(Qn::FP_Low,        "Low")
(Qn::FP_Medium,     "Medium")
(Qn::FP_High,       "High")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, BackupType,
    (Qn::Backup_Manual,            "BackupManual")
    (Qn::Backup_RealTime,          "BackupRealTime")
    (Qn::Backup_Schedule,          "BackupSchedule")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, StorageInitResult,
    (Qn::StorageInit_Ok,            "Ok")
    (Qn::StorageInit_CreateFailed,  "CreateFailed")
    (Qn::StorageInit_WrongPath,     "InitFailed_WrongPath")
    (Qn::StorageInit_WrongAuth,     "InitFailed_WrongAuth")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, CameraBackupQualities,
    (Qn::CameraBackup_Disabled,          "CameraBackupDisabled")
    (Qn::CameraBackup_HighQuality,       "CameraBackupHighQuality")
    (Qn::CameraBackup_LowQuality,        "CameraBackupLowQuality")
    (Qn::CameraBackup_Both,              "CameraBackupBoth")
    (Qn::CameraBackup_Default,           "CameraBackupDefault")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, CameraBackupQuality,
    (Qn::CameraBackup_Disabled,          "CameraBackupDisabled")
    (Qn::CameraBackup_HighQuality,       "CameraBackupHighQuality")
    (Qn::CameraBackup_LowQuality,        "CameraBackupLowQuality")
    (Qn::CameraBackup_Both,              "CameraBackupBoth")
    (Qn::CameraBackup_Default,           "CameraBackupDefault")
)

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, RecordingType)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, StreamQuality,
    (Qn::QualityLowest,     "lowest")
    (Qn::QualityLow,        "low")
    (Qn::QualityNormal,     "normal")
    (Qn::QualityHigh,       "high")
    (Qn::QualityHighest,    "highest")
    (Qn::QualityPreSet,     "preset")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, BookmarkSortField,
    (Qn::BookmarkName,          "name")
    (Qn::BookmarkStartTime,     "startTime")
    (Qn::BookmarkDuration,      "duration")
    (Qn::BookmarkCreationTime,  "created")
    (Qn::BookmarkCreator,       "creator")
    (Qn::BookmarkTags,          "tags")
    (Qn::BookmarkCameraName,    "cameraName")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qt, SortOrder,
    (Qt::AscendingOrder,     "asc")
    (Qt::DescendingOrder,    "desc")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, RebuildAction,
(Qn::RebuildAction_Start,   "start")
(Qn::RebuildAction_Cancel,  "stop")
(Qn::RebuildAction_ShowProgress, QString())
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, BackupAction,
(Qn::BackupAction_Start,   "start")
(Qn::BackupAction_Cancel,  "stop")
(Qn::BackupAction_ShowProgress, QString())
)

namespace Qn {

QString toString(AuthResult value)
{
    return QnLexical::serialized(value);
}

} // namespace Qn
