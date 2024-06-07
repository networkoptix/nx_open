// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_field_validator.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "../action_builder.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/layout_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../event_filter.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../rule.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetUserFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto targetUserField = dynamic_cast<const TargetUserField*>(field);
    if (!NX_ASSERT(targetUserField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto targetUserFieldProperties = targetUserField->properties();
    const bool isValidSelection = targetUserField->acceptAll()
        || !targetUserField->ids().empty()
        || targetUserFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, Strings::selectUser()};

    if (!targetUserFieldProperties.validationPolicy.isEmpty())
    {
        if (targetUserFieldProperties.validationPolicy == kBookmarkManagementValidationPolicy)
        {
            auto acknowledgeField = rule->actionBuilders().front()->fieldByName<ActionFlagField>(
                utils::kAcknowledgeFieldName);
            if (!NX_ASSERT(acknowledgeField))
            {
                return {
                    QValidator::State::Invalid,
                    Strings::fieldValueMustBeProvided(utils::kAcknowledgeFieldName)};
            }

            if (acknowledgeField->value() == true)
            {
                auto cameraField = rule->eventFilters().front()->fieldByName<SourceCameraField>(
                    utils::kCameraIdFieldName);
                if (!NX_ASSERT(cameraField))
                {
                    return {
                        QValidator::State::Invalid,
                        Strings::fieldValueMustBeProvided(utils::kCameraIdFieldName)};
                }

                QnRequiredAccessRightPolicy policy{
                    context,
                    vms::api::AccessRight::manageBookmarks,
                    targetUserFieldProperties.allowEmptySelection};

                QnVirtualCameraResourceList cameras =
                    context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
                        cameraField->ids());

                policy.setCameras(cameras);

                return utils::usersValidity(
                    policy, targetUserField->acceptAll(), targetUserField->ids());
            }
        }
        else if (targetUserFieldProperties.validationPolicy == kUserWithEmailValidationPolicy)
        {
            // TODO: #mmalofeev check additional recipients.
            QnUserWithEmailValidationPolicy policy{
                context, targetUserFieldProperties.allowEmptySelection};

            return utils::usersValidity(
                policy, targetUserField->acceptAll(), targetUserField->ids());
        }
        else if (targetUserFieldProperties.validationPolicy == kCloudUserValidationPolicy)
        {
            QnCloudUsersValidationPolicy policy{
                context, targetUserFieldProperties.allowEmptySelection};

            return utils::usersValidity(
                policy, targetUserField->acceptAll(), targetUserField->ids());
        }
        else if (targetUserFieldProperties.validationPolicy == kLayoutAccessValidationPolicy)
        {
            auto layoutField = rule->actionBuilders().front()->fieldByName<LayoutField>(
                utils::kLayoutIdFieldName);
            if (!NX_ASSERT(layoutField))
            {
                return {
                    QValidator::State::Invalid,
                    Strings::fieldValueMustBeProvided(utils::kLayoutIdFieldName)};
            }

            QnLayoutAccessValidationPolicy policy{
                context, targetUserFieldProperties.allowEmptySelection};
            policy.setLayout(
                context->resourcePool()->getResourceById<QnLayoutResource>(layoutField->value()));

            return utils::usersValidity(
                policy, targetUserField->acceptAll(), targetUserField->ids());
        }
        else
        {
            return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
        }
    }

    QnDefaultSubjectValidationPolicy policy{context, targetUserFieldProperties.allowEmptySelection};
    return utils::usersValidity(policy, targetUserField->acceptAll(), targetUserField->ids());
}

} // namespace nx::vms::rules
