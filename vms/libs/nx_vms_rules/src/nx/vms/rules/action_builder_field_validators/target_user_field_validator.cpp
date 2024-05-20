// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_field_validator.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "../action_builder.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/layout_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../event_filter.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../rule.h"
#include "../utils/field.h"
#include "../utils/resource.h"

namespace nx::vms::rules {

namespace {

ValidationResult getValidity(
    const QnSubjectValidationPolicy& policy, bool acceptAll, const UuidSet& subjects)
{
    const auto validity = policy.validity(acceptAll, subjects);
    if (validity == QValidator::State::Acceptable)
        return {};

    return {
        validity,
        policy.calculateAlert(acceptAll, subjects)
    };
}

} // namespace

ValidationResult TargetUserFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto targetUserField = dynamic_cast<const TargetUserField*>(field);
    if (!NX_ASSERT(targetUserField))
        return {QValidator::State::Invalid, tr("Invalid field type is provided")};

    const auto targetUserFieldProperties = targetUserField->properties();

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
                    tr("Acknowledge field must be provided for the given validation policy")};
            }

            if (acknowledgeField->value() == true)
            {
                auto cameraField = rule->eventFilters().front()->fieldByName<SourceCameraField>(
                    utils::kCameraIdFieldName);
                if (cameraField)
                {
                    QnRequiredAccessRightPolicy policy{
                        context,
                        vms::api::AccessRight::manageBookmarks,
                        targetUserFieldProperties.allowEmptySelection};

                    QnVirtualCameraResourceList cameras =
                        utils::cameras(cameraField->selection(), context);

                    policy.setCameras(cameras);

                    return getValidity(policy, targetUserField->acceptAll(), targetUserField->ids());
                }
            }
        }

        if (targetUserFieldProperties.validationPolicy == kUserWithEmailValidationPolicy)
        {
            // TODO: #mmalofeev check additional recipients.
            QnUserWithEmailValidationPolicy policy{
                context, targetUserFieldProperties.allowEmptySelection};

            return getValidity(policy, targetUserField->acceptAll(), targetUserField->ids());
        }

        if (targetUserFieldProperties.validationPolicy == kCloudUserValidationPolicy)
        {
            QnCloudUsersValidationPolicy policy{
                context, targetUserFieldProperties.allowEmptySelection};

            return getValidity(policy, targetUserField->acceptAll(), targetUserField->ids());
        }

        if (targetUserFieldProperties.validationPolicy == kLayoutAccessValidationPolicy)
        {
            auto layoutField = rule->actionBuilders().front()->fieldByName<LayoutField>(
                utils::kLayoutIdFieldName);
            if (!NX_ASSERT(layoutField))
            {
                return {
                    QValidator::State::Invalid,
                    tr("Layout field must be provided for the given validation policy")};
            }

            QnLayoutAccessValidationPolicy policy{
                context, targetUserFieldProperties.allowEmptySelection};

            policy.setLayout(
                context->resourcePool()->getResourceById<QnLayoutResource>(layoutField->value()));

            return getValidity(policy, targetUserField->acceptAll(), targetUserField->ids());
        }
    }

    if (!targetUserFieldProperties.allowEmptySelection && targetUserField->selection().isEmpty())
        return {QValidator::State::Invalid, tr("Select at least one user")};

    return {};
}

} // namespace nx::vms::rules
