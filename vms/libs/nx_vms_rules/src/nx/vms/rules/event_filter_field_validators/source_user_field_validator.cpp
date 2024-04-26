// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_field_validator.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/validation_policy.h>

#include "../event_filter.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../event_filter_fields/source_user_field.h"
#include "../rule.h"
#include "../utils/field.h"

namespace nx::vms::rules {

ValidationResult SourceUserFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto sourceUserField = dynamic_cast<const SourceUserField*>(field);
    if (!NX_ASSERT(sourceUserField))
        return {QValidator::State::Invalid, {tr("Invalid field type is provided")}};

    const auto sourceUserFieldProperties = sourceUserField->properties();

    if (sourceUserFieldProperties.validationPolicy == kUserInputValidationPolicy)
    {
        auto cameraField =
            rule->eventFilters().front()->fieldByName<SourceCameraField>(utils::kCameraIdFieldName);
        if (!NX_ASSERT(cameraField))
        {
            return {
                QValidator::State::Invalid,
                tr("Source camera field must be provided for the given validation policy")};
        }

        QnVirtualCameraResourceList cameras;
        if (cameraField->acceptAll())
        {
            cameras = context->resourcePool()->getAllCameras();
        }
        else
        {
            cameras = context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                cameraField->ids());
        }

        QnRequiredAccessRightPolicy policy{
            context,
            vms::api::AccessRight::userInput,
            sourceUserFieldProperties.allowEmptySelection};

        policy.setCameras(cameras);

        const auto validity =
            policy.validity(sourceUserField->acceptAll(), sourceUserField->ids());

        if (validity == QValidator::State::Acceptable)
            return {};

        return {
            validity,
            policy.calculateAlert(sourceUserField->acceptAll(), sourceUserField->ids())};
    }

    const auto isSelectionEmpty =
        !sourceUserField->acceptAll() && sourceUserField->ids().empty();
    if (!sourceUserFieldProperties.allowEmptySelection && isSelectionEmpty)
        return {QValidator::State::Invalid, tr("Select at least one user")};

    return {};
}

} // namespace nx::vms::rules
