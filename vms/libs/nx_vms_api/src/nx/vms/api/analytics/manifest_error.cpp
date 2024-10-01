// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manifest_error.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::api::analytics {

QString toString(ManifestErrorType errorType)
{
    switch (errorType)
    {
        case ManifestErrorType::noError:
            return "";
        case ManifestErrorType::emptyIntegrationId:
            return "Integration id is empty";
        case ManifestErrorType::emptyIntegrationName:
            return "Integration name is empty";
        case ManifestErrorType::emptyIntegrationDescription:
            return "Integration description is empty";
        case ManifestErrorType::emptyIntegrationVersion:
            return "Integration version is empty";
        case ManifestErrorType::emptyIntegrationVendor:
            return "Integration vendor is empty";
        case ManifestErrorType::emptyObjectActionId:
            return "Object Action id is empty";
        case ManifestErrorType::emptyObjectActionName:
            return "Object Action name is empty";
        case ManifestErrorType::duplicatedObjectActionId:
            return "Multiple Object Actions have the same id";
        case ManifestErrorType::duplicatedObjectActionName:
            return "Multiple Object Actions have the same id";
        case ManifestErrorType::emptyEventTypeId:
            return "Event Type id is empty";
        case ManifestErrorType::emptyEventTypeName:
            return "Event Type name is empty";
        case ManifestErrorType::duplicatedEventTypeId:
            return "Multiple Event Types have the same id";
        case ManifestErrorType::duplicatedEventTypeName:
            return "Multiple Event Types have the same name";
        case ManifestErrorType::emptyObjectTypeId:
            return "Object Type id is empty";
        case ManifestErrorType::emptyObjectTypeName:
            return "Object Type name is empty";
        case ManifestErrorType::duplicatedObjectTypeId:
            return "Multiple Object Types have the same id";
        case ManifestErrorType::duplicatedObjectTypeName:
            return "Multiple Object Types have the same name";
        case ManifestErrorType::emptyGroupId:
            return "Group id is empty";
        case ManifestErrorType::emptyGroupName:
            return "Group name is empty";
        case ManifestErrorType::duplicatedGroupId:
            return "Multiple Groups have the same id";
        case ManifestErrorType::duplicatedGroupName:
            return "Multiple Groups have the same name";
        case ManifestErrorType::deviceAgentSettingsModelIsIncorrect:
            return "DeviceAgent settings model is defined but is not a correct JSON object";
        case ManifestErrorType::uncompressedFramePixelFormatIsNotSpecified:
        {
            return "Uncompressed video stream is requested by the Integration but its pixel format is "
                "not specified";
        }
        case ManifestErrorType::excessiveUncompressedFramePixelFormatSpecification:
            return "Pixel format is specified but uncompressed video stream is not requested";
        default:
            NX_ASSERT(false);
            return QString();
    }
}

QString toString(ManifestError manifestError)
{
    return "Manifest error: " + toString(manifestError.errorType)
        + (manifestError.additionalInfo.isEmpty()
            ? ""
            : (". Details: " + manifestError.additionalInfo));
}

} // namespace nx::vms::api::analytics
