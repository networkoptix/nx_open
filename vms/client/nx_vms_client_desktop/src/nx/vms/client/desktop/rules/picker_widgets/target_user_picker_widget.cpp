// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_picker_widget.h"

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/event_devices_field.h>
#include <nx/vms/rules/action_builder_fields/flag_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/actions/open_layout_action.h>
#include <nx/vms/rules/actions/send_email_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <utils/email/email.h>

#include "../utils/user_picker_helper.h"

namespace nx::vms::client::desktop::rules {

void TargetUserPicker::onSelectButtonClicked()
{
    ui::SubjectSelectionDialog dialog(this);
    dialog.setCheckedSubjects(m_field->ids());
    dialog.setAllUsers(m_field->acceptAll());

    if (vms::rules::utils::type<vms::rules::SendEmailAction>()
        == parentParamsWidget()->descriptor().id)
    {
        QSharedPointer<QnSendEmailActionDelegate> dialogDelegate(
            new QnSendEmailActionDelegate(this));

        dialog.setAllUsersSelectorEnabled(false);
        dialog.setUserValidator(
            [dialogDelegate](const QnUserResourcePtr& user)
            {
                return dialogDelegate->isValid(user->getId());
            });

        const auto updateAlert =
            [&dialog, dialogDelegate]
            {
                dialog.showAlert(dialogDelegate->validationMessage(
                    dialog.checkedSubjects()));
            };

        connect(&dialog, &ui::SubjectSelectionDialog::changed, this, updateAlert);
        updateAlert();
    }

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_field->setIds(dialog.checkedSubjects());
    m_field->setAcceptAll(dialog.allUsers());
}

void TargetUserPicker::updateUi()
{
    static const QnDefaultSubjectValidationPolicy defaultPolicy;

    if (vms::rules::utils::type<vms::rules::SendEmailAction>()
        == parentParamsWidget()->descriptor().id)
    {
        QnUserWithEmailValidationPolicy userWithEmailValidationPolicy{systemContext()};
        UserPickerHelper helper{
            systemContext(),
            m_field->acceptAll(),
            m_field->ids(),
            &userWithEmailValidationPolicy,
            /*isIntermediateStateValid*/ false};

        const auto additionalEmailsField =
            parentParamsWidget()->actionBuilder()->fieldByName<vms::rules::ActionTextField>(
                vms::rules::utils::kEmailsFieldName);
        m_selectButton->setText(QnSendEmailActionDelegate::getText(
            m_field->ids(),
            /*detailed*/ true,
            NX_ASSERT(additionalEmailsField) ? additionalEmailsField->value() : QString{}));
        m_selectButton->setIcon(helper.icon());
    }
    else if (vms::rules::utils::type<vms::rules::NotificationAction>()
        == parentParamsWidget()->descriptor().id)
    {
        const auto acknowledgeField =
            parentParamsWidget()->actionBuilder()->fieldByName<vms::rules::FlagField>(
                vms::rules::utils::kAcknowledgeFieldName);

        QnRequiredPermissionSubjectPolicy acknowledgePolicy(
            systemContext(), Qn::ManageBookmarksPermission, QString());
        // TODO: use event cameras for the policy.
        // acknowledgePolicy.setCameras(
        //     resourcePool()->getResourcesByIds<QnVirtualCameraResource>(eventDevicesField));

        const QnSubjectValidationPolicy* policy = acknowledgeField->value() == true
            ? dynamic_cast<const QnSubjectValidationPolicy*>(&acknowledgePolicy)
            : dynamic_cast<const QnDefaultSubjectValidationPolicy*>(&defaultPolicy);

        UserPickerHelper helper{
            systemContext(),
            m_field->acceptAll(),
            m_field->ids(),
            policy};

        m_selectButton->setText(helper.text());
        m_selectButton->setIcon(helper.icon());
    }
    // else if (vms::rules::utils::type<vms::rules::PushNotificationAction>()
    //     == parentParamsWidget()->descriptor().id)
    // {
    //     static const QnCloudUsersValidationPolicy cloudUsersPolicy;
    //     UserPickerHelper helper{
    //         systemContext(),
    //         m_field->acceptAll(),
    //         m_field->ids(),
    //         &cloudUsersPolicy};

    //     m_selectButton->setText(helper.text());
    //     m_selectButton->setIcon(helper.icon());
    // }
    else if (vms::rules::utils::type<vms::rules::OpenLayoutAction>()
        == parentParamsWidget()->descriptor().id)
    {
        // TODO: Uncomment when OpenLayoutAction will be implemented.
        // const auto targetLayoutField = parentParamsWidget()->actionBuilder()->fieldByName<...>(...);
        // const auto layout = resourcePool()->getResourceById<QnLayoutResource>(
        //     targetLayoutField->id());

        // QnLayoutAccessValidationPolicy layoutAccessPolicy(systemContext());
        // layoutAccessPolicy.setLayout(layout);

        // UserPickerHelper helper{
        //     systemContext(),
        //     m_field->acceptAll(),
        //     m_field->ids(),
        //     &layoutAccessPolicy};

        // m_selectButton->setText(helper.text());
        // m_selectButton->setIcon(helper.icon());
    }
    else
    {
        UserPickerHelper helper{
            systemContext(),
            m_field->acceptAll(),
            m_field->ids(),
            &defaultPolicy};

        m_selectButton->setText(helper.text());
        m_selectButton->setIcon(helper.icon());
    }
}

} // namespace nx::vms::client::desktop::rules
