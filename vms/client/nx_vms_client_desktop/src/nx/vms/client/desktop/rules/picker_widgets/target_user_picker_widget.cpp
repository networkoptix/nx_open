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

#include "../utils/user_picker_helper.h"

namespace nx::vms::client::desktop::rules {

void TargetUserPicker::onSelectButtonClicked()
{
    auto field = theField();

    ui::SubjectSelectionDialog dialog(this);
    dialog.setValidationPolicy(m_policy.get());
    dialog.setCheckedSubjects(field->ids());
    dialog.setAllUsers(field->acceptAll());

    if (dialog.exec() != QDialog::Accepted)
        return;

    field->setIds(dialog.checkedSubjects());
    field->setAcceptAll(dialog.allUsers());
}

void TargetUserPicker::updateUi()
{
    auto field = theField();
    createPolicy();

    UserPickerHelperParameters helperParameters;

    if (m_fieldDescriptor->linkedFields.contains(vms::rules::utils::kEmailsFieldName))
    {
        const auto additionalRecipients =
            getActionField<vms::rules::ActionTextField>(vms::rules::utils::kEmailsFieldName);
        if (NX_ASSERT(additionalRecipients) && !additionalRecipients->value().isEmpty())
        {
            const auto emails = additionalRecipients->value().split(';', Qt::SkipEmptyParts);
            helperParameters.additionalUsers = emails.size();
            helperParameters.additionalValidUsers = std::count_if(
                emails.cbegin(),
                emails.cend(),
                [](const QString& s) { return email::isValidAddress(s); });
        }
    }

    UserPickerHelper helper{
        systemContext(),
        field->acceptAll(),
        field->ids(),
        m_policy.get(),
        /*isIntermediateStateValid*/ false,
        helperParameters};

    m_selectButton->setText(helper.text());
    m_selectButton->setIcon(helper.icon());
}

void TargetUserPicker::createPolicy()
{
    const auto actionId = parentParamsWidget()->descriptor()->id;
    if (actionId == vms::rules::utils::type<vms::rules::SendEmailAction>())
    {
        m_policy = std::make_unique<QnUserWithEmailValidationPolicy>();
    }
    else if (actionId == vms::rules::utils::type<vms::rules::NotificationAction>())
    {
        auto acknowledgeField =
            getActionField<vms::rules::ActionFlagField>(vms::rules::utils::kAcknowledgeFieldName);

        if (acknowledgeField->value() == true)
        {
            m_policy = std::make_unique<QnRequiredAccessRightPolicy>(
                nx::vms::api::AccessRight::manageBookmarks);
        }
        else
        {
            m_policy = std::make_unique<QnDefaultSubjectValidationPolicy>();
        }
    }
    else if (actionId == vms::rules::utils::type<vms::rules::PushNotificationAction>())
    {
        m_policy = std::make_unique<QnCloudUsersValidationPolicy>();
    }
    else
    {
        m_policy = std::make_unique<QnDefaultSubjectValidationPolicy>();
    }
}

} // namespace nx::vms::client::desktop::rules
