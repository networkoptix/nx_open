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
#include "../utils/validity.h"

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
                    Strings::fieldValueMustBeProvided(utils::kCameraIdFieldName)};
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

                return utils::usersValidity(
                    policy, sourceUserField->acceptAll(), sourceUserField->ids());
            }
        }
        else
        {
            return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
        }
    }

    QnDefaultSubjectValidationPolicy policy{context, sourceUserFieldProperties.allowEmptySelection};
    return utils::usersValidity(policy, sourceUserField->acceptAll(), sourceUserField->ids());
}

} // namespace nx::vms::rules
