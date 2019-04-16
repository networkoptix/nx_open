#include "open_layout_action_widget.h"
#include "ui_open_layout_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>

#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/models/resource/resource_list_model.h>

#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>

#include "nx/vms/client/desktop/ui/event_rules/layout_selection_dialog.h"
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::vms::client::desktop {

OpenLayoutActionWidget::OpenLayoutActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::OpenLayoutActionWidget)
{
    ui->setupUi(this);

    connect(ui->selectLayoutButton, &QPushButton::clicked,
        this, &OpenLayoutActionWidget::openLayoutSelectionDialog);

    setWarningStyle(ui->warningForLayouts);
    setWarningStyle(ui->warningForUsers);

    ui->warningForLayouts->setHidden(true);
    ui->warningForUsers->setHidden(true);

    setSubjectsButton(ui->selectUsersButton);
}

OpenLayoutActionWidget::~OpenLayoutActionWidget()
{
}

void OpenLayoutActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->selectLayoutButton);
    setTabOrder(ui->selectLayoutButton, after);
}

void OpenLayoutActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        auto params = model()->actionParams();
        m_selectedLayout = resourcePool()->getResourceById<QnLayoutResource>(params.actionResourceId);
        checkWarnings();
        updateLayoutsButton();
    }
}

void OpenLayoutActionWidget::displayWarning(LayoutWarning warning)
{
    if (warning == m_layoutWarning)
        return;

    switch (warning)
    {
        case LayoutWarning::MissingAccess:
            setWarningStyle(ui->warningForUsers);
            ui->warningForLayouts->setText(tr("Some users don't have access to the selected layout. Action will not work for them."));
            break;
        case LayoutWarning::NobodyHasAccess:
            setWarningStyle(ui->warningForUsers);
            ui->warningForLayouts->setText(tr("None of selected users have access to the selected layout. Action will not work."));
            break;
        case LayoutWarning::NoWarning:
            break;
    }
    m_layoutWarning = warning;
    ui->warningForLayouts->setHidden(warning == LayoutWarning::NoWarning);
}

void OpenLayoutActionWidget::displayWarning(UserWarning warning)
{
    if (warning == m_userWarning)
        return;

    switch (warning)
    {
        case UserWarning::LocalResource:
            setWarningStyle(ui->warningForUsers);
            ui->warningForUsers->setText(tr("Local layouts can only be shown to their owners."));
            break;
        case UserWarning::EmptyRoles:
            setWarningStyle(ui->warningForUsers);
            ui->warningForUsers->setText(tr("None of selected user roles contain users. Action will not work."));
        case UserWarning::NoWarning:
            break;
    }
    m_userWarning = warning;
    ui->warningForUsers->setHidden(warning == UserWarning::NoWarning);
}

void OpenLayoutActionWidget::checkWarnings()
{
    QString warnings;

    bool rolesSelected;
    QnResourcePtr layout = getSelectedLayout();
    QnUserResourceList users = getSelectedUsers(rolesSelected);

    bool foundUserWarning = false;
    bool foundLayoutWarning = false;

    auto accessManager = commonModule()->resourceAccessManager();

    int noAccess = 0;
    bool othersLocal = false;

    if (layout)
    {
        auto parentResource = layout->getParentResource();

        for (auto user: users)
        {
            if (!accessManager->hasPermission(user, layout, Qn::ReadPermission))
                ++noAccess;

            if (parentResource && parentResource->getParentId() != user->getId())
                othersLocal = true;
        }
    }

    if (noAccess > 0)
    {
        displayWarning(noAccess == users.size() ? LayoutWarning::NobodyHasAccess : LayoutWarning::MissingAccess);
        foundLayoutWarning = true;
    }

    if (rolesSelected && !users.size())
    {
        displayWarning(UserWarning::EmptyRoles);
        foundUserWarning = true;
    }

    if (othersLocal)
    {
        displayWarning(UserWarning::LocalResource);
        foundUserWarning = true;
    }

    if (!foundUserWarning)
        displayWarning(UserWarning::NoWarning);
    if (!foundLayoutWarning)
        displayWarning(LayoutWarning::NoWarning);
}

QnLayoutResourcePtr OpenLayoutActionWidget::getSelectedLayout()
{
    return m_selectedLayout;
}

QnUserResourceList OpenLayoutActionWidget::getSelectedUsers(bool& rolesSelected)
{
    QnUserResourceList users;
    if (model())
    {
        const auto params = model()->actionParams();
        QList<QnUuid> roles;
        userRolesManager()->usersAndRoles(params.additionalResources, users, roles);
        rolesSelected = roles.size();
        for (const auto& roleId: roles)
            for (const auto& subject: resourceAccessSubjectsCache()->usersInRole(roleId))
                users.append(subject.user()); //< TODO: skip duplicates?
        users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
    }
    return users;
}

void OpenLayoutActionWidget::updateLayoutsButton()
{
    if (!model())
        return;

    const auto icon = [](QnResourceIconCache::Key key, QnIcon::Mode mode)
        {
            return QnSkin::maximumSizePixmap(qnResIconCache->icon(key), mode);
        };

    auto button = ui->selectLayoutButton;

    if (QnLayoutResourcePtr layout = getSelectedLayout())
    {
        button->setText(layout->getName());
        button->setForegroundRole(QPalette::BrightText);

        const bool isWarning = m_layoutWarning != LayoutWarning::NoWarning;
        setWarningStyleOn(button, isWarning);
        const QnIcon::Mode iconMode = isWarning ? QnIcon::Error : QnIcon::Selected;
        QnResourceIconCache::Key iconKey = QnResourceIconCache::Layout;

        if (layout->isShared())
            iconKey = QnResourceIconCache::SharedLayout;
        else if (layout->locked())
            iconKey = QnResourceIconCache::Layout | QnResourceIconCache::Locked;
        button->setIcon(icon(iconKey, iconMode));
    }
    else
    {
        button->setText(tr("Select layout..."));
        button->setIcon(icon(QnResourceIconCache::Layouts, QnIcon::Selected));
        button->setForegroundRole(QPalette::ButtonText);
        resetStyle(button);
    }
}

void OpenLayoutActionWidget::openLayoutSelectionDialog()
{
    if (!model() || m_updating)
        return;
    /*
    Cases from the specification:
        No users selected:
            - hide local layouts
            - show notification
        Exactly one user selected:
            - show shared
            - show local
        Multiple users selected:
            - restrict selection
            - show local red
            - hide local if shared is selected
            - show warning label
    */

    const bool singlePick = true;
    LayoutSelectionDialog dialog(singlePick, this);

    bool rolesSelected; //< #TODO: check specification for the case when selection includes roles
    const auto& users = getSelectedUsers(rolesSelected);

    LayoutSelectionDialog::LocalLayoutSelection selectionMode =
        LayoutSelectionDialog::ModeFull;

    QSet<QnUuid> selection;
    if (auto layout = getSelectedLayout())
        selection.insert(layout->getId());

    QnResourceList localLayouts;

    // Gather local layouts. Specification is this tricky.
    if (users.size() > 1)
    {
        // Should add only resources, that are selected. The rest resources are omitted
        selectionMode = LayoutSelectionDialog::ModeLimited;
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [this, selection, users](const QnLayoutResourcePtr& layout)
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
            dialog.showAlert(tr("Local layouts can only be shown to their owners."));
    }
    else if (users.size() == 1)
    {
        auto accessManager = commonModule()->resourceAccessManager();
        // Should show all resources belonging to current user.
        selectionMode = LayoutSelectionDialog::ModeFull;
        auto user = users.front();
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [this, selection, user, accessManager](const QnLayoutResourcePtr& layout)
            {
                if (!layout->hasFlags(Qn::remote))
                    return false;
                if (layout->getParentId() == user->getId())
                {
                    return true;
                }
                return false;
            });
    }
    else if (users.empty())
    {
        dialog.showInfo(tr("Select some single user in \"Show to\" line to display his local layouts in this list"));
        selectionMode = LayoutSelectionDialog::ModeHideLocal;
    }
    dialog.setLocalLayouts(localLayouts, selection, selectionMode);

    // Gather shared layouts. No trickery is here.
    QnResourceList sharedLayouts = resourcePool()->getResources<QnLayoutResource>(
        [this](const QnLayoutResourcePtr& layout)
        {
            if (!layout->hasFlags(Qn::remote))
                return false;
            return layout->isShared();
        });

    dialog.setSharedLayouts(sharedLayouts, selection);

    if (dialog.exec() == QDialog::Accepted)
    {
        QScopedValueRollback<bool> updatingRollback(m_updating, true);

        auto params = model()->actionParams();
        auto selection = dialog.checkedLayouts();
        if (selection.empty())
        {
            params.actionResourceId = QnUuid();
        }
        else
        {
            auto uuid = *selection.begin();
            m_selectedLayout = resourcePool()->getResourceById<QnLayoutResource>(uuid);
            params.actionResourceId = uuid;
        }
        model()->setActionParams(params);
    }

    checkWarnings();
    updateLayoutsButton();
}

} // namespace nx::vms::client::desktop
