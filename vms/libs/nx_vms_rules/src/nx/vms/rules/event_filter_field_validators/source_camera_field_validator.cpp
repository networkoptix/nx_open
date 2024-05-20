// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_camera_field_validator.h"

#include <core/resource/device_dependent_strings.h>
#include <nx/vms/rules/camera_validation_policy.h>

#include "../event_filter_fields/source_camera_field.h"
#include "../manifest.h"
#include "../utils/resource.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult SourceCameraFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto sourceCameraField = dynamic_cast<const SourceCameraField*>(field);
    if (!NX_ASSERT(sourceCameraField))
        return {QValidator::State::Invalid, {tr("Invalid field type is provided")}};

    const auto camerasSelection = sourceCameraField->selection();
    const auto sourceCameraFieldProperties = sourceCameraField->properties();

    if (!camerasSelection.isEmpty() && !sourceCameraFieldProperties.validationPolicy.isEmpty())
    {
        auto cameras = utils::cameras(camerasSelection, context);

        ValidationResult validationResult;
        if (sourceCameraFieldProperties.validationPolicy == kCameraAnalyticsValidationPolicy)
            return utils::camerasValidity<QnCameraAnalyticsPolicy>(context, cameras);

        if (sourceCameraFieldProperties.validationPolicy == kCameraInputValidationPolicy)
            return utils::camerasValidity<QnCameraInputPolicy>(context, cameras);

        if (sourceCameraFieldProperties.validationPolicy == kCameraMotionValidationPolicy)
            return utils::camerasValidity<QnCameraMotionPolicy>(context, cameras);

        return {QValidator::State::Invalid, tr("Unexpected validation policy")};
    }

    if (!sourceCameraFieldProperties.allowEmptySelection && camerasSelection.isEmpty())
    {
        return {
            QValidator::State::Invalid,
            QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(),
                tr("Select at least one device"),
                tr("Select at least one camera"))};
    }

    return {};
}

} // namespace nx::vms::rules
