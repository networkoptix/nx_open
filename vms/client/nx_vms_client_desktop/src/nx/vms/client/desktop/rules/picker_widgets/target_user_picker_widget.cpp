// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_picker_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/event_devices_field.h>
#include <nx/vms/rules/action_builder_fields/flag_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/actions/open_layout_action.h>
#include <nx/vms/rules/actions/push_notification_action.h>
#include <nx/vms/rules/actions/send_email_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <utils/email/email.h>

#include "../utils/icons.h"
#include "../utils/strings.h"

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
        dialog.setValidationPolicy(m_policy.get());

    dialog.setCheckedSubjects(m_field->ids());
    dialog.setAllUsers(m_field->acceptAll());

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_field->setIds(dialog.checkedSubjects());
    m_field->setAcceptAll(dialog.allUsers());
}

void TargetUserPicker::updateUi()
{
    ResourcePickerWidgetBase<vms::rules::TargetUserField>::updateUi();

    int additionalCount{0};
    if (const auto additionalRecipients =
        getActionField<vms::rules::ActionTextField>(vms::rules::utils::kEmailsFieldName))
    {
        if (!additionalRecipients->value().isEmpty())
        {
            const auto emails = additionalRecipients->value().split(';', Qt::SkipEmptyParts);
            additionalCount = static_cast<int>(emails.size());
        }
    }

    m_selectButton->setText(Strings::selectButtonText(systemContext(), m_field, additionalCount));
    m_selectButton->setIcon(selectButtonIcon(systemContext(), m_field, additionalCount));
}

} // namespace nx::vms::client::desktop::rules
