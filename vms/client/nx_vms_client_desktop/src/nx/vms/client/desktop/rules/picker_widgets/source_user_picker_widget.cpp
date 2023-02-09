// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_picker_widget.h"

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/qset.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <ui/widgets/select_resources_button.h>

#include "../utils/user_picker_helper.h"

namespace nx::vms::client::desktop::rules {

SourceUserPicker::SourceUserPicker(SystemContext* context, CommonParamsWidget* parent):
    ResourcePickerWidgetBase<vms::rules::SourceUserField>(context, parent),
    m_validationPolicy{new QnRequiredPermissionSubjectPolicy(
        context,
        Qn::SoftTriggerPermission,
        tr("User Input"))}
{
}

void SourceUserPicker::onSelectButtonClicked()
{
    auto selectedUsers = m_field->ids();

    const auto roleValidator =
        [this](const QnUuid& roleId) { return m_validationPolicy->roleValidity(roleId); };

    const auto userValidator =
        [this](const QnUserResourcePtr& user) { return m_validationPolicy->userValidity(user); };

    ui::SubjectSelectionDialog dialog(this);
    dialog.setAllUsers(m_field->acceptAll());
    dialog.setCheckedSubjects(selectedUsers);
    dialog.setRoleValidator(roleValidator);
    dialog.setUserValidator(userValidator);

    const auto updateAlert =
        [this, &dialog]()
        {
            dialog.showAlert(m_validationPolicy->calculateAlert(
                dialog.allUsers(), dialog.checkedSubjects()));
        };

    connect(&dialog, &ui::SubjectSelectionDialog::changed, this, updateAlert);
    updateAlert();

    if (dialog.exec() == QDialog::Rejected)
        return;

    m_field->setAcceptAll(dialog.allUsers());
    m_field->setIds(dialog.totalCheckedUsers());

    updateUi();
}

void SourceUserPicker::updateUi()
{
    UserPickerHelper helper{
        systemContext(),
        m_field->acceptAll(),
        m_field->ids(),
        m_validationPolicy,
        /*isIntermediateStateValid*/ false};

    m_selectButton->setText(helper.text());
    m_selectButton->setIcon(helper.icon());
}

} // namespace nx::vms::client::desktop::rules
