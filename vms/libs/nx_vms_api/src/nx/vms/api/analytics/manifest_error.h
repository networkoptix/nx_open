#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

enum class ManifestErrorType
{
    noError = 0,
    emptyPluginId = 1 << 0,
    emptyPluginName = 1 << 1,
    emptyPluginDescription = 1 << 2,
    emptyPluginVersion = 1 << 3,
    emptyPluginVendor = 1 << 4,

    emptyObjectActionId = 1 << 5,
    emptyObjectActionName = 1 << 6,
    duplicatedObjectActionId = 1 << 7,
    duplicatedObjectActionName = 1 << 8,

    emptyEventTypeId = 1 << 9,
    emptyEventTypeName = 1 << 10,
    duplicatedEventTypeId = 1 << 11,
    duplicatedEventTypeName = 1 << 12,

    emptyObjectTypeId = 1 << 13,
    emptyObjectTypeName = 1 << 14,
    duplicatedObjectTypeId = 1 << 15,
    duplicatedObjectTypeName = 1 << 16,

    emptyGroupId = 1 << 17,
    emptyGroupName = 1 << 18,
    duplicatedGroupId = 1 << 19,
    duplicatedGroupName = 1 << 20,
};

Q_DECLARE_FLAGS(ManifestErrorTypes, ManifestErrorType)
Q_DECLARE_OPERATORS_FOR_FLAGS(ManifestErrorTypes)

NX_VMS_API QString toHumanReadableString(ManifestErrorType errorType);

struct ManifestError
{
    ManifestError() = default;
    ManifestError(ManifestErrorType errorType, QString additionalInfo = QString()):
        errorType(errorType),
        additionalInfo(additionalInfo)
    {
    }

    ManifestErrorType errorType = ManifestErrorType::noError;
    QString additionalInfo;
};

struct ListManifestErrorTypes
{
    ManifestErrorType emptyId;
    ManifestErrorType emptyName;
    ManifestErrorType duplicatedId;
    ManifestErrorType duplicatedName;

    QString listEntryTypeName;
};

struct EntryFieldManifestErrorTypes
{
    ManifestErrorType emptyField;
    ManifestErrorType duplicatedField;

    QString listEntryTypeName;
};

} // nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::ManifestErrorType,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::ManifestErrorTypes,
    (metatype)(numeric)(lexical), NX_VMS_API)
