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

#include <utils/common/scoped_value_rollback.h>

#include "nx/client/desktop/ui/event_rules/layout_selection_dialog.h"
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

OpenLayoutActionWidget::OpenLayoutActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::OpenLayoutActionWidget)
{
    ui->setupUi(this);

    m_defaultPalette = qApp->palette();
    m_warningPalette = qApp->palette();

    connect(ui->selectLayoutButton, &QPushButton::clicked,
        this, &OpenLayoutActionWidget::openLayoutSelectionDialog);

    QColor color = qnGlobals->errorTextColor();
    m_warningPalette.setColor(QPalette::Text, color);
    m_warningPalette.setColor(QPalette::BrightText, color);

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
        m_selectedLayout = resourcePool()->getResourceById<QnLayoutResource>(params.layoutResourceId);
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
            ui->warningForLayouts->setText(tr("Some users don't have access to the selected layout.\nAction will not work for them."));
            break;
        case LayoutWarning::NobodyHasAccess:
            setWarningStyle(ui->warningForUsers);
            ui->warningForLayouts->setText(tr("None of selected users have access to the selected layout.Action will not work."));
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
            ui->warningForUsers->setText(tr("Local layouts can only be shown to their owners. "));
            break;
        case UserWarning::NoWarning:
            break;
    }
    m_userWarning = warning;
    ui->warningForUsers->setHidden(warning == UserWarning::NoWarning);
}

void OpenLayoutActionWidget::checkWarnings()
{
    QString warnings;

    QnResourcePtr layout = getSelectedLayout();
    QnUserResourceList users = getSelectedUsers();

    bool foundUserWarning = false;
    bool foundLayoutWarning = false;

    auto accessManager = commonModule()->resourceAccessManager();
    
    int noAccess = 0;
    int othersLocal = 0;

    if(layout)
    {
        auto parentResource = layout->getParentResource();

        for (auto user : users)
        {
            if (!accessManager->hasPermission(user, layout, Qn::ReadPermission))
                noAccess++;

            if (parentResource && parentResource->getParentId() != user->getId())
                othersLocal++;
        }
    }

    if (othersLocal > 0)
        displayWarning(UserWarning::LocalResource);

    if (noAccess > 0)
    {
        displayWarning(noAccess == users.size() ? LayoutWarning::NobodyHasAccess : LayoutWarning::MissingAccess);
        foundLayoutWarning = true;
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

QnUserResourceList OpenLayoutActionWidget::getSelectedUsers()
{
    QnUserResourceList users;
    if (model())
    {
        const auto params = model()->actionParams();
        QList<QnUuid> roles;
        userRolesManager()->usersAndRoles(params.additionalResources, users, roles);
        users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
    }
    return users;
}

void OpenLayoutActionWidget::updateLayoutsButton()
{
    if (!model())
        return;

    const auto icon = [](const QString& path) -> QIcon
        {
            static const QnIcon::SuffixesList suffixes{ { QnIcon::Normal, lit("selected") } };
            return qnSkin->icon(path, QString(), &suffixes);
        };

    auto button = ui->selectLayoutButton;

    if (QnLayoutResourcePtr layout = getSelectedLayout())
    {
        button->setText(layout->getName());
        button->setForegroundRole(QPalette::BrightText);

        if (m_layoutWarning != LayoutWarning::NoWarning)
        {
            if (layout->isShared())
                button->setIcon(icon(lit("tree/layout_shared.png")));
            else if (layout->locked())
                button->setIcon(icon(lit("tree/layout_locked_error.png")));
            else
                button->setIcon(icon(lit("tree/layout_error.png")));
            button->setPalette(m_warningPalette);
        }
        else
        {
            if (layout->isShared())
                button->setIcon(icon(lit("tree/layout_shared.png")));
            else if (layout->locked())
                button->setIcon(icon(lit("tree/layout_locked.png")));
            else
                button->setIcon(icon(lit("tree/layout.png")));
            button->setPalette(m_defaultPalette);
        }
    }
    else
    {
        button->setText(tr("Select layout..."));
        button->setIcon(icon(lit("tree/layouts.png")));
        button->setForegroundRole(QPalette::ButtonText);
        button->setPalette(m_defaultPalette);
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
    ui::LayoutSelectionDialog dialog(singlePick, this);

    const auto& users = getSelectedUsers();

    ui::LayoutSelectionDialog::LocalLayoutSelection selectionMode =
        ui::LayoutSelectionDialog::ModeFull;

    QSet<QnUuid> selection;
    if (auto layout = getSelectedLayout())
        selection.insert(layout->getId());
    
    QnResourceList localLayouts;

    // Gather local layouts. Specification is this tricky.
    if (users.size() > 1)
    {
        // Should add only resources, that are selected. The rest resources are omitted
        selectionMode = ui::LayoutSelectionDialog::ModeLimited;
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
            dialog.showAlert(tr("Local layouts can only be shown to their owners. "));
    }
    else if (users.size() == 1)
    {
        auto accessManager = commonModule()->resourceAccessManager();
        // Should show all resources belonging to current user.
        selectionMode = ui::LayoutSelectionDialog::ModeFull;
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
        selectionMode = ui::LayoutSelectionDialog::ModeHideLocal;
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
            params.layoutResourceId = QnUuid();
        }
        else
        {
            auto uuid = *selection.begin();
            m_selectedLayout = resourcePool()->getResourceById<QnLayoutResource>(uuid);
            params.layoutResourceId = uuid;
        }
        model()->setActionParams(params);
    }

    checkWarnings();
    updateLayoutsButton();
}

} // namespace desktop
} // namespace client
} // namespace nx
