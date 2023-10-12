// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "single_target_layout_picker_widget.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/ui/event_rules/layout_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop::rules {

SingleTargetLayoutPicker::SingleTargetLayoutPicker(
    QnWorkbenchContext* context, CommonParamsWidget* parent)
    :
    ResourcePickerWidgetBase<vms::rules::LayoutField>(context, parent)
{
}

void SingleTargetLayoutPicker::updateUi()
{
    if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(theField()->value()))
    {
        m_selectButton->setText(layout->getName());
        m_selectButton->setForegroundRole(QPalette::BrightText);

        QnResourceIconCache::Key iconKey = QnResourceIconCache::Layout;
        if (layout->isShared())
            iconKey = QnResourceIconCache::SharedLayout;
        else if (layout->locked())
            iconKey = QnResourceIconCache::Layout | QnResourceIconCache::Locked;

        m_selectButton->setIcon(
            core::Skin::maximumSizePixmap(qnResIconCache->icon(iconKey), QnIcon::Selected));
    }
    else
    {
        m_selectButton->setText(tr("Select layout..."));
        m_selectButton->setIcon(core::Skin::maximumSizePixmap(
            qnResIconCache->icon(QnResourceIconCache::Layouts), QnIcon::Selected));
        m_selectButton->setForegroundRole(QPalette::ButtonText);
        resetStyle(m_selectButton);
    }
}

void SingleTargetLayoutPicker::onSelectButtonClicked()
{
    LayoutSelectionDialog dialog(/*singlePick*/ true, this);

    LayoutSelectionDialog::LocalLayoutSelection selectionMode = LayoutSelectionDialog::ModeFull;

    QSet<QnUuid> selection;
    if (!theField()->value().isNull())
        selection.insert(theField()->value());

    QnResourceList localLayouts;
    QnUserResourceSet users;
    if (const auto usersField =
        getActionField<vms::rules::TargetUserField>(vms::rules::utils::kUsersFieldName))
    {
        users = usersField->users();
    }

    // Gather local layouts. Specification is this tricky.
    if (users.size() > 1)
    {
        // Should add only resources, that are selected. The rest resources are omitted
        selectionMode = LayoutSelectionDialog::ModeLimited;
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [selection, users](const QnLayoutResourcePtr& layout)
            {
                if (!layout->hasFlags(Qn::remote))
                    return false;

                // Check it belongs to any user and this layout is picked already
                for(const auto& user: users)
                {
                    if (layout->getParentId() == user->getId())
                    {
                        // Only of someone has already selected it
                        return selection.contains(layout->getId());
                    }
                }
                return false;
            });

        if (!localLayouts.empty())
            dialog.showAlert(tr("Local layouts can only be shown to their owners"));
    }
    else if (users.size() == 1)
    {
        // Should show all resources belonging to current user.
        selectionMode = LayoutSelectionDialog::ModeFull;
        auto user = *users.begin();
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [selection, user](const QnLayoutResourcePtr& layout)
            {
                return layout->hasFlags(Qn::remote) && layout->getParentId() == user->getId();
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

    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto selectedLayouts = dialog.checkedLayouts();
    theField()->setValue(selectedLayouts.empty() ? QnUuid{} : *selectedLayouts.begin());
}

} // namespace nx::vms::client::desktop::rules
