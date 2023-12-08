// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_picker_widget.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/rules/events/soft_trigger_event.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/widgets/select_resources_button.h>

#include "../utils/user_picker_helper.h"

namespace nx::vms::client::desktop::rules {

SourceUserPicker::SourceUserPicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
    ResourcePickerWidgetBase<vms::rules::SourceUserField>(context, parent)
{
}

void SourceUserPicker::onDescriptorSet()
{
    ResourcePickerWidgetBase<vms::rules::SourceUserField>::onDescriptorSet();

    if (nx::vms::rules::utils::type<nx::vms::rules::SoftTriggerEvent>()
        == parentParamsWidget()->descriptor().id)
    {
        m_validationPolicy = std::make_unique<QnRequiredAccessRightPolicy>(
            nx::vms::api::AccessRight::userInput);
    }
    else
    {
        m_validationPolicy = std::make_unique<QnDefaultSubjectValidationPolicy>();
    }
}

void SourceUserPicker::onSelectButtonClicked()
{
    auto field = theField();

    ui::SubjectSelectionDialog dialog(this);
    dialog.setCheckedSubjects(field->ids());
    dialog.setAllUsers(field->acceptAll());
    dialog.setValidationPolicy(m_validationPolicy.get());

    if (dialog.exec() == QDialog::Rejected)
        return;

    field->setAcceptAll(dialog.allUsers());
    field->setIds(dialog.checkedSubjects());
}

void SourceUserPicker::updateUi()
{
    auto field = theField();
    UserPickerHelper helper{
        systemContext(),
        field->acceptAll(),
        field->ids(),
        m_validationPolicy.get(),
        /*isIntermediateStateValid*/ false};

    m_selectButton->setText(helper.text());
    m_selectButton->setIcon(helper.icon());
}

} // namespace nx::vms::client::desktop::rules
