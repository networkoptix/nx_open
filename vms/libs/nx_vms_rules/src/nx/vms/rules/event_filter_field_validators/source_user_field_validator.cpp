// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_field_validator.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "../event_filter.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../event_filter_fields/source_user_field.h"
#include "../rule.h"
#include "../strings.h"
#include "../utils/field.h"

namespace nx::vms::rules {

ValidationResult SourceUserFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto sourceUserField = dynamic_cast<const SourceUserField*>(field);
    if (!NX_ASSERT(sourceUserField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto sourceUserFieldProperties = sourceUserField->properties();
    const bool isValidSelection = sourceUserField->acceptAll()
        || !sourceUserField->ids().empty()
        || sourceUserFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, Strings::selectUser()};

    if (!sourceUserFieldProperties.validationPolicy.isEmpty())
    {
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

            const auto cameraIds = cameraField->ids();
            if (!cameraIds.empty())
            {
                QnVirtualCameraResourceList cameras =
                    context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                        cameraField->ids());

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
        }
        else
        {
            return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
        }
    }

    return {};
}

} // namespace nx::vms::rules
