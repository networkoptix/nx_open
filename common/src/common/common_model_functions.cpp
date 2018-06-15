#include "common_globals.h"

#include <core/ptz/ptz_constants.h>

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCommand)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzObjectType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, StatisticsDeviceType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PanicMode)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, RebuildState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, BackupState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, BitratePerGopType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ServerFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, Permissions)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, GlobalPermissions)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, TTHeaderFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, IOPortTypes)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, AuditRecordType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, AuthResult)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, StreamIndex)

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

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, MediaStreamEvent,
    (Qn::NoEvent,                       "NoEvent")
    (Qn::TooManyOpenedConnections,      "TooManyOpenedConnections")
    (Qn::ForbiddenWithDefaultPassword,  "ForbiddenWithDefaultPassword")
    (Qn::ForbiddenWithNoLicense,        "ForbiddenWithNoLicense")
    (Qn::oldFirmware,                   "oldFirmare")
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

QString toString(MediaStreamEvent value)
{
    switch (value)
    {
        case NoEvent:
            return QString();
        case TooManyOpenedConnections:
            return lit("Too many opened connections");
        case ForbiddenWithDefaultPassword:
            return lit("Please setup camera password");
        case ForbiddenWithNoLicense:
            return lit("No license");
        case oldFirmware:
            return lit("Cameras has too old firmware");
        default:
            return lit("Unknown error");
    }
}

} // namespace Qn
