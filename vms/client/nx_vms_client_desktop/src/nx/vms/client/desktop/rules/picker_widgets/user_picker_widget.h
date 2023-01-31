// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <ui/widgets/select_resources_button.h>

#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class UserPickerWidget: public ResourcePickerWidgetBase<F, QnSelectUsersButton>
{
public:
    using ResourcePickerWidgetBase<F, QnSelectUsersButton>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F, QnSelectUsersButton>::m_selectButton;
    using ResourcePickerWidgetBase<F, QnSelectUsersButton>::m_alertLabel;

    void onSelectButtonClicked() override
    {
        auto selectedUsers = m_field->ids();

        ui::SubjectSelectionDialog dialog(this);
        dialog.setAllUsers(m_field->acceptAll());
        dialog.setCheckedSubjects(selectedUsers);
        if (dialog.exec() == QDialog::Rejected)
            return;

        m_field->setAcceptAll(dialog.allUsers());
        m_field->setIds(dialog.totalCheckedUsers());

        updateValue();
    }

    void updateValue() override
    {
        auto resourceList =
            resourcePool()->template getResourcesByIds<QnUserResource>(m_field->ids());

        if (m_field->acceptAll())
            m_selectButton->selectAll();
        else
            m_selectButton->selectUsers(resourceList);

        if (resourceList.isEmpty() && !m_field->acceptAll())
        {
            m_alertLabel->setText(ResourcePickerWidgetStrings::selectUser());
            m_alertLabel->setVisible(true);
        }
        else
        {
            m_alertLabel->setVisible(false);
        }
    }
};

using SourceUserPicker = UserPickerWidget<vms::rules::SourceUserField>;
using TargetUserPicker = UserPickerWidget<vms::rules::TargetUserField>;

} // namespace nx::vms::client::desktop::rules
