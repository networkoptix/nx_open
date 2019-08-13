#include "manifest_error.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, ManifestErrorType,
    (nx::vms::api::analytics::ManifestErrorType::noError, "")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginId, "emptyPluginId")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginName, "emptyPluginName")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginDescription, "emptyPluginDescription")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginVersion, "emptyPluginVersion")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginVendor, "emptyPluginVendor")
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectActionId, "emptyObjectActionId")
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectActionName, "emptyObjectActionName")
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedObjectActionId,
        "duplicatedObjectActionId"
    )
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedObjectActionName,
        "duplicatedObjectActionName"
    )
    (nx::vms::api::analytics::ManifestErrorType::emptyEventTypeId, "emptyEventTypeId")
    (nx::vms::api::analytics::ManifestErrorType::emptyEventTypeName, "emptyEventTypeName")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedEventTypeId, "duplicatedEventTypeId")
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedEventTypeName,
        "duplicatedEventTypeName"
    )
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectTypeId, "emptyObjectTypeId")
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectTypeName, "emptyObjectTypeName")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedObjectTypeId, "duplicatedObjectTypeId")
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedObjectTypeName,
        "duplicatedObjectTypeName"
    )
    (nx::vms::api::analytics::ManifestErrorType::emptyGroupId, "emptyGroupId")
    (nx::vms::api::analytics::ManifestErrorType::emptyGroupName, "emptyGroupName")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedGroupId, "duplicatedGroupId")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedGroupName, "duplicatedGroupName")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::ManifestErrorType, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, ManifestErrorTypes,
    (nx::vms::api::analytics::ManifestErrorType::noError, "")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginId, "emptyPluginId")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginName, "emptyPluginName")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginDescription, "emptyPluginDescription")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginVersion, "emptyPluginVersion")
    (nx::vms::api::analytics::ManifestErrorType::emptyPluginVendor, "emptyPluginVendor")
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectActionId, "emptyObjectActionId")
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectActionName, "emptyObjectActionName")
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedObjectActionId,
        "duplicatedObjectActionId"
    )
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedObjectActionName,
        "duplicatedObjectActionName"
    )
    (nx::vms::api::analytics::ManifestErrorType::emptyEventTypeId, "emptyEventTypeId")
    (nx::vms::api::analytics::ManifestErrorType::emptyEventTypeName, "emptyEventTypeName")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedEventTypeId, "duplicatedEventTypeId")
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedEventTypeName,
        "duplicatedEventTypeName"
    )
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectTypeId, "emptyObjectTypeId")
    (nx::vms::api::analytics::ManifestErrorType::emptyObjectTypeName, "emptyObjectTypeName")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedObjectTypeId, "duplicatedObjectTypeId")
    (
        nx::vms::api::analytics::ManifestErrorType::duplicatedObjectTypeName,
        "duplicatedObjectTypeName"
    )
    (nx::vms::api::analytics::ManifestErrorType::emptyGroupId, "emptyGroupId")
    (nx::vms::api::analytics::ManifestErrorType::emptyGroupName, "emptyGroupName")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedGroupId, "duplicatedGroupId")
    (nx::vms::api::analytics::ManifestErrorType::duplicatedGroupName, "duplicatedGroupName")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::ManifestErrorTypes, (numeric)(debug))


namespace nx::vms::api::analytics {

QString toHumanReadableString(ManifestErrorType errorType)
{
    switch (errorType)
    {
    case nx::vms::api::analytics::ManifestErrorType::noError:
        return "";
    case nx::vms::api::analytics::ManifestErrorType::emptyPluginId:
        return "Plugin id is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyPluginName:
        return "Plugin name is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyPluginDescription:
        return "Plugin description is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyPluginVersion:
        return "Plugin version is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyPluginVendor:
        return "Plugin vendor is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyObjectActionId:
        return "Object Action id is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyObjectActionName:
        return "Object Action name is empty";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedObjectActionId:
        return "Multiple Object Actions have the same id";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedObjectActionName:
        return "Multiple Object Actions have the same id";
    case nx::vms::api::analytics::ManifestErrorType::emptyEventTypeId:
        return "Event Type id is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyEventTypeName:
        return "Event Type name is empty";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedEventTypeId:
        return "Multiple Event Types have the same id";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedEventTypeName:
        return "Multiple Event Types have the same name";
    case nx::vms::api::analytics::ManifestErrorType::emptyObjectTypeId:
        return "Object Type id is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyObjectTypeName:
        return "Object Type name is empty";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedObjectTypeId:
        return "Multiple Object Types have the same id";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedObjectTypeName:
        return "Multiple Object Types have the same name";
    case nx::vms::api::analytics::ManifestErrorType::emptyGroupId:
        return "Group id is empty";
    case nx::vms::api::analytics::ManifestErrorType::emptyGroupName:
        return "Group name is empty";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedGroupId:
        return "Multiple Groups have the same id";
    case nx::vms::api::analytics::ManifestErrorType::duplicatedGroupName:
        return "Multiple Groups have the same name";
    default:
        NX_ASSERT(false);
        return QString();
    }
}

} // namespace nx::vms::api::analytics
