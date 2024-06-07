// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_picker_widget.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <ui/widgets/select_resources_button.h>

namespace nx::vms::client::desktop::rules {

SourceUserPicker::SourceUserPicker(
    vms::rules::SourceUserField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    ResourcePickerWidgetBase<vms::rules::SourceUserField>(field, context, parent)
{
    if (field->properties().validationPolicy == vms::rules::kUserInputValidationPolicy)
    {
        m_validationPolicy = std::make_unique<QnRequiredAccessRightPolicy>(
            systemContext(),
            nx::vms::api::AccessRight::userInput);
    }
    else
    {
        m_validationPolicy = std::make_unique<QnDefaultSubjectValidationPolicy>(systemContext());
    }
}

void SourceUserPicker::onSelectButtonClicked()
{
    ui::SubjectSelectionDialog dialog(this);
    dialog.setCheckedSubjects(m_field->ids());
    dialog.setAllUsers(m_field->acceptAll());
    dialog.setValidationPolicy(m_validationPolicy.get());

    if (dialog.exec() == QDialog::Rejected)
        return;

    m_field->setAcceptAll(dialog.allUsers());
    m_field->setIds(dialog.checkedSubjects());
}

} // namespace nx::vms::client::desktop::rules
