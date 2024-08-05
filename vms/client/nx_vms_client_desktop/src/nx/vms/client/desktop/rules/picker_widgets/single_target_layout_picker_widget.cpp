// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "single_target_layout_picker_widget.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/ui/event_rules/layout_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/validity.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop::rules {

SingleTargetLayoutPicker::SingleTargetLayoutPicker(
    vms::rules::LayoutField* field,
    SystemContext* systemContext,
    ParamsWidget* parent)
    :
    ResourcePickerWidgetBase<vms::rules::LayoutField>(field, systemContext, parent)
{
}

void SingleTargetLayoutPicker::onSelectButtonClicked()
{
    LayoutSelectionDialog dialog(/*singlePick*/ true, this);

    LayoutSelectionDialog::LocalLayoutSelection selectionMode = LayoutSelectionDialog::ModeFull;

    QSet<nx::Uuid> selection;
    if (!m_field->value().isNull())
        selection.insert(m_field->value());

    QnResourceList localLayouts;
    QnUserResourceList users;
    if (const auto usersField =
        getActionField<vms::rules::TargetUserField>(vms::rules::utils::kUsersFieldName))
    {
        users = usersField->users().values();
    }

    connect(
        &dialog,
        &LayoutSelectionDialog::layoutsChanged,
        this,
        [&dialog, &users, this]
        {
            const auto selectedLayouts = dialog.checkedLayouts();
            if (selectedLayouts.empty())
            {
                dialog.showAlert({});
                return;
            }

            const auto layout =
                resourcePool()->getResourceById<QnLayoutResource>(*selectedLayouts.begin());
            const auto selectionValidity =
                vms::rules::utils::layoutValidity(systemContext(), layout, users);

            dialog.showAlert(selectionValidity.description);
        });

    // Gather local layouts. Specification is this tricky.
    if (users.size() > 1)
    {
        // Should add only resources, that are selected. The rest resources are omitted
        selectionMode = LayoutSelectionDialog::ModeLimited;
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [&selection, &users](const QnLayoutResourcePtr& layout)
            {
                if (!layout->hasFlags(Qn::remote))
                    return false;

                // Check it belongs to any user and this layout is picked already.
                for(const auto& user: users)
                {
                    if (layout->getParentId() == user->getId())
                    {
                        // Only if someone has already selected it.
                        return selection.contains(layout->getId());
                    }
                }
                return false;
            });
    }
    else if (users.size() == 1)
    {
        // Should show all resources belonging to current user.
        selectionMode = LayoutSelectionDialog::ModeFull;
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [userId = users.first()->getId()](const QnLayoutResourcePtr& layout)
            {
                return layout->hasFlags(Qn::remote) && layout->getParentId() == userId;
            });
    }
    else
    {
        dialog.showInfo(tr("Looking for a local layout? Select only one user from the \"Show to\" "
            "list to display their local layouts as an option here."));
        selectionMode = LayoutSelectionDialog::ModeHideLocal;
    }
    dialog.setLocalLayouts(localLayouts, selection, selectionMode);

    // Gather shared layouts. No trickery is here.
    QnResourceList sharedLayouts = resourcePool()->getResources<QnLayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return layout->hasFlags(Qn::remote) && layout->isShared();
        });

    dialog.setSharedLayouts(sharedLayouts, selection);

    if (dialog.exec() == QDialog::Accepted)
    {
        const auto selectedLayouts = dialog.checkedLayouts();
        m_field->setValue(selectedLayouts.empty() ? nx::Uuid{} : *selectedLayouts.begin());
    }

    ResourcePickerWidgetBase<vms::rules::LayoutField>::onSelectButtonClicked();
}

} // namespace nx::vms::client::desktop::rules
