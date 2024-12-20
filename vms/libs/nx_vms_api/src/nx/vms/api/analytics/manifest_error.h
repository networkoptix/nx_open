// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/flags.h>

namespace nx::vms::api::analytics {

NX_REFLECTION_ENUM_CLASS(ManifestErrorType,
    noError = 0,
    emptyIntegrationId = 1 << 0,
    emptyIntegrationName = 1 << 1,
    emptyIntegrationDescription = 1 << 2,
    obsolete_emptyIntegrationVersion = 1 << 3, //< No longer used; kept for backward compatibility.
    obsolete_emptyIntegrationVendor = 1 << 4, //< No longer used; kept for backward compatibility.

    emptyObjectActionId = 1 << 5,
    emptyObjectActionName = 1 << 6,
    duplicatedObjectActionId = 1 << 7,
    duplicatedObjectActionName = 1 << 8,

    emptyEventTypeId = 1 << 9,
    emptyEventTypeName = 1 << 10,
    duplicatedEventTypeId = 1 << 11,
    duplicatedEventTypeName = 1 << 12,

    emptyObjectTypeId = 1 << 13,
    obsolete_emptyObjectTypeName = 1 << 14, //< No longer used; kept for backward compatibility.
    duplicatedObjectTypeId = 1 << 15,
    duplicatedObjectTypeName = 1 << 16,

    emptyGroupId = 1 << 17,
    emptyGroupName = 1 << 18,
    duplicatedGroupId = 1 << 19,
    duplicatedGroupName = 1 << 20,

    deviceAgentSettingsModelIsIncorrect = 1 << 21,

    uncompressedFramePixelFormatIsNotSpecified = 1 << 22,
    excessiveUncompressedFramePixelFormatSpecification = 1 << 23
)
Q_DECLARE_FLAGS(ManifestErrorTypes, ManifestErrorType)
Q_DECLARE_OPERATORS_FOR_FLAGS(ManifestErrorTypes)

NX_VMS_API QString toString(ManifestErrorType errorType);

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

NX_VMS_API QString toString(ManifestError manifestError);

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
