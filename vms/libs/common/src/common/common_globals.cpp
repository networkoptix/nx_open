#include "common_globals.h"

#include <core/ptz/ptz_constants.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCoordinateSpace)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzObjectType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, StatisticsDeviceType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, PanicMode)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, RebuildState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, BackupState)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, Permissions)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, TTHeaderFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, IOPortTypes)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, AuditRecordType)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, AuthResult)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ResourceStatus)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, StatusChangeReason)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, StorageStatuses)

// ATTENTION: Assigning backwards-compatible misspelled values with `Auxilary` in them. Such values
// should go before the correct ones, so both versions are supported on input, and only deprecated
// version is supported on output.
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, PtzCommand,
    (Qn::ContinuousMovePtzCommand, "ContinuousMovePtzCommand")
    (Qn::ContinuousFocusPtzCommand, "ContinuousFocusPtzCommand")
    (Qn::AbsoluteDeviceMovePtzCommand, "AbsoluteDeviceMovePtzCommand")
    (Qn::AbsoluteLogicalMovePtzCommand, "AbsoluteLogicalMovePtzCommand")
    (Qn::ViewportMovePtzCommand, "ViewportMovePtzCommand")
    (Qn::GetDevicePositionPtzCommand, "GetDevicePositionPtzCommand")
    (Qn::GetLogicalPositionPtzCommand, "GetLogicalPositionPtzCommand")
    (Qn::GetDeviceLimitsPtzCommand, "GetDeviceLimitsPtzCommand")
    (Qn::GetLogicalLimitsPtzCommand, "GetLogicalLimitsPtzCommand")
    (Qn::GetFlipPtzCommand, "GetFlipPtzCommand")
    (Qn::CreatePresetPtzCommand, "CreatePresetPtzCommand")
    (Qn::UpdatePresetPtzCommand, "UpdatePresetPtzCommand")
    (Qn::RemovePresetPtzCommand, "RemovePresetPtzCommand")
    (Qn::ActivatePresetPtzCommand, "ActivatePresetPtzCommand")
    (Qn::GetPresetsPtzCommand, "GetPresetsPtzCommand")
    (Qn::CreateTourPtzCommand, "CreateTourPtzCommand")
    (Qn::RemoveTourPtzCommand, "RemoveTourPtzCommand")
    (Qn::ActivateTourPtzCommand, "ActivateTourPtzCommand")
    (Qn::GetToursPtzCommand, "GetToursPtzCommand")
    (Qn::GetActiveObjectPtzCommand, "GetActiveObjectPtzCommand")
    (Qn::UpdateHomeObjectPtzCommand, "UpdateHomeObjectPtzCommand")
    (Qn::GetHomeObjectPtzCommand, "GetHomeObjectPtzCommand")
    (Qn::GetAuxiliaryTraitsPtzCommand, "GetAuxilaryTraitsPtzCommand") //< Intentional typo.
    (Qn::GetAuxiliaryTraitsPtzCommand, "GetAuxiliaryTraitsPtzCommand") //< For deserialization.
    (Qn::RunAuxiliaryCommandPtzCommand, "RunAuxilaryCommandPtzCommand") //< Intentional typo.
    (Qn::RunAuxiliaryCommandPtzCommand, "RunAuxiliaryCommandPtzCommand") //< For deserialization.
    (Qn::GetDataPtzCommand, "GetDataPtzCommand")
    (Qn::RelativeMovePtzCommand, "RelativeMovePtzCommand")
    (Qn::RelativeFocusPtzCommand, "RelativeFocusPtzCommand")
    (Qn::InvalidPtzCommand, "InvalidPtzCommand")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, IOPortType,
    (Qn::PT_Unknown, "Unknown")
    (Qn::PT_Disabled, "Disabled")
    (Qn::PT_Input, "Input")
    (Qn::PT_Output, "Output")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, IODefaultState,
    (Qn::IO_OpenCircuit, "Open Circuit")
    (Qn::IO_GroundedCircuit, "Grounded circuit")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, StorageInitResult,
    (Qn::StorageInit_Ok, "Ok")
    (Qn::StorageInit_CreateFailed, "CreateFailed")
    (Qn::StorageInit_WrongPath, "InitFailed_WrongPath")
    (Qn::StorageInit_WrongAuth, "InitFailed_WrongAuth")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, MediaStreamEvent,
    (Qn::NoEvent, "NoEvent")
    (Qn::TooManyOpenedConnections, "TooManyOpenedConnections")
    (Qn::ForbiddenWithDefaultPassword, "ForbiddenWithDefaultPassword")
    (Qn::ForbiddenWithNoLicense, "ForbiddenWithNoLicense")
    (Qn::oldFirmware, "oldFirmare")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, BookmarkSortField,
    (Qn::BookmarkName, "name")
    (Qn::BookmarkStartTime, "startTime")
    (Qn::BookmarkDuration, "duration")
    (Qn::BookmarkCreationTime, "created")
    (Qn::BookmarkCreator, "creator")
    (Qn::BookmarkTags, "tags")
    (Qn::BookmarkCameraName, "cameraName")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qt, SortOrder,
    (Qt::AscendingOrder, "asc")
    (Qt::DescendingOrder, "desc")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, RebuildAction,
(Qn::RebuildAction_Start, "start")
(Qn::RebuildAction_Cancel, "stop")
(Qn::RebuildAction_ShowProgress, QString())
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, BackupAction,
(Qn::BackupAction_Start, "start")
(Qn::BackupAction_Cancel, "stop")
(Qn::BackupAction_ShowProgress, QString())
)

namespace Qn {

QString toString(AuthResult value)
{
    return QnLexical::serialized(value);
}

QString toErrorMessage(AuthResult value)
{
    switch (value)
    {
        case Auth_OK:
            NX_ASSERT(false, "This value is not an error");
            break;

        case Auth_WrongLogin:
        case Auth_WrongInternalLogin:
        case Auth_WrongDigest:
        case Auth_WrongPassword:
            return "Wrong username or password.";

        case Auth_PasswordExpired:
            return "The password is expired. Please, contact your system administrator.";

        case Auth_LDAPConnectError:
            return "The LDAP server is not accessible. Please, try again later.";

        case Auth_CloudConnectError:
            return nx::network::AppInfo::cloudName()
                + " is not accessible yet. Please, try again later.";

        case Auth_DisabledUser:
            return "The user is disabled. Please, contact your system administrator.";

        case Auth_LockedOut:
            return "The user is locked out due to several failed attempts. Please, try again later.";

        case Auth_Forbidden:
        case Auth_InvalidCsrfToken:
            return "This authorization method is forbidden. Please, contact your system administrator.";
    }

    NX_ASSERT(false, lm("Unhandled value: %1").arg(value));
    return lm("Internal server error (%1). Please, contact your system administrator.").arg(value);
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
            return lit("Please set up camera password");
        case ForbiddenWithNoLicense:
            return lit("No license");
        case oldFirmware:
            return lit("Cameras has too old firmware");
        default:
            return lit("Unknown error");
    }
}

QString toString(ResourceStatus status)
{
    return QnLexical::serialized(status);
}

QString toString(StatusChangeReason reason)
{
    return QnLexical::serialized(reason);
}

} // namespace Qn
