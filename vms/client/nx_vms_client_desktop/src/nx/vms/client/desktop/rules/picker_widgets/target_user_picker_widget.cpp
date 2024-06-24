// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_picker_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/event_devices_field.h>
#include <nx/vms/rules/action_builder_fields/flag_field.h>
#include <nx/vms/rules/action_builder_fields/layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/actions/open_layout_action.h>
#include <nx/vms/rules/actions/push_notification_action.h>
#include <nx/vms/rules/actions/send_email_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop::rules {

TargetUserPicker::TargetUserPicker(
    vms::rules::TargetUserField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    ResourcePickerWidgetBase<vms::rules::TargetUserField>{field, context, parent}
{
    const auto validationPolicy = field->properties().validationPolicy;
    const auto allowEmptySelection = field->properties().allowEmptySelection;

    if (validationPolicy == vms::rules::kUserWithEmailValidationPolicy)
    {
        m_policy = std::make_unique<QnUserWithEmailValidationPolicy>(
            systemContext(), allowEmptySelection);
    }
    else if (validationPolicy == vms::rules::kCloudUserValidationPolicy)
    {
        m_policy = std::make_unique<QnCloudUsersValidationPolicy>(
            systemContext(), allowEmptySelection);
    }
    else if (validationPolicy == vms::rules::kBookmarkManagementValidationPolicy)
    {
        m_policy = std::make_unique<QnRequiredAccessRightPolicy>(
            systemContext(), vms::api::AccessRight::manageBookmarks, allowEmptySelection);
    }
    else if (validationPolicy == vms::rules::kLayoutAccessValidationPolicy)
    {
        m_policy = std::make_unique<QnLayoutAccessValidationPolicy>(
            systemContext(), allowEmptySelection);
    }
    else
    {
        m_policy = std::make_unique<QnDefaultSubjectValidationPolicy>(
            systemContext(), allowEmptySelection);
    }
}

void TargetUserPicker::onSelectButtonClicked()
{
    ui::SubjectSelectionDialog dialog(this);

    bool isValidationPolicyRequired = true;
    if (auto acknowledgeField =
        getActionField<vms::rules::ActionFlagField>(vms::rules::utils::kAcknowledgeFieldName))
    {
        if (!acknowledgeField->value())
            isValidationPolicyRequired = false;
    }

    if (isValidationPolicyRequired)
    {
        dialog.setValidationPolicy(m_policy.get());

        const auto validationPolicy = m_field->properties().validationPolicy;
        if (validationPolicy == vms::rules::kLayoutAccessValidationPolicy)
        {
            const auto layoutField =
                getActionField<vms::rules::LayoutField>(vms::rules::utils::kLayoutIdFieldName);

            if (!NX_ASSERT(
                layoutField, "LayoutField must be provided for the given validation policy"))
            {
                return;
            }

            const auto layoutResource =
                resourcePool()->getResourceById<QnLayoutResource>(layoutField->value());
            static_cast<QnLayoutAccessValidationPolicy*>(m_policy.get())->setLayout(layoutResource);
        }
        else if (validationPolicy == vms::rules::kBookmarkManagementValidationPolicy)
        {
            auto cameraField =
                getEventField<vms::rules::SourceCameraField>(vms::rules::utils::kCameraIdFieldName);

            if (cameraField && !cameraField->acceptAll())
            {
                const auto cameras =
                    resourcePool()->getResourcesByIds<QnVirtualCameraResource>(cameraField->ids());
                static_cast<QnRequiredAccessRightPolicy*>(m_policy.get())->setCameras(cameras);
            }
        }
    }

    dialog.setCheckedSubjects(m_field->ids());
    dialog.setAllUsers(m_field->acceptAll());

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_field->setIds(dialog.checkedSubjects());
    m_field->setAcceptAll(dialog.allUsers());
}

} // namespace nx::vms::client::desktop::rules
