// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_layout_action_widget.h"
#include "ui_open_layout_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <business/business_resource_validation.h>
#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/layout_selection_dialog.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/models/resource/resource_list_model.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

OpenLayoutActionWidget::OpenLayoutActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
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

    m_validator = new QnLayoutAccessValidationPolicy{};
    setValidationPolicy(m_validator); //< Takes ownership, so we use raw pointer here.

    connect(ui->rewindForWidget, &RewindForWidget::valueEdited, this,
        [this]()
        {
            const bool isValidState = model() && !m_updating;
            if (!NX_ASSERT(isValidState))
                return;

            QScopedValueRollback<bool> guard(m_updating, true);
            auto params = model()->actionParams();
            params.recordBeforeMs = milliseconds(ui->rewindForWidget->value()).count();
            model()->setActionParams(params);
        });
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

    if (fields.testFlag(Field::actionParams))
    {
        // Validator should be updated before the call to base_type.
        auto params = model()->actionParams();
        m_selectedLayout = resourcePool()->getResourceById<QnLayoutResource>(params.actionResourceId);
        m_validator->setLayout(m_selectedLayout);
    }

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        checkWarnings();
        updateLayoutsButton();
        ui->rewindForWidget->setValue(duration_cast<seconds>(
            milliseconds(model()->actionParams().recordBeforeMs)));
    }
}

void OpenLayoutActionWidget::displayWarning(LayoutWarning warning)
{
    if (warning == m_layoutWarning)
        return;

    switch (warning)
    {
        case LayoutWarning::LocalResource:
            setWarningStyle(ui->warningForLayouts);
            ui->warningForLayouts->setText(tr("Local layouts can only be shown to their owners"));
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
        case UserWarning::EmptyRoles:
            setWarningStyle(ui->warningForUsers);
            ui->warningForUsers->setText(
                tr("None of selected user roles contain users. Action will not work."));
            break;
        case UserWarning::MissingAccess:
            setWarningStyle(ui->warningForUsers);
            ui->warningForUsers->setText(
                tr("Some users do not have access to the selected layout. "
                    "Action will not work for them."));
            break;
        case UserWarning::NobodyHasAccess:
            setWarningStyle(ui->warningForUsers);
            ui->warningForUsers->setText(
                tr("None of selected users have access to the selected layout. "
                    "Action will not work."));
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
    QnUserResourceList users;
    QList<QnUuid> roles;
    std::tie(users, roles) = getSelectedUsersAndRoles();

    bool foundUserWarning = false;
    bool foundLayoutWarning = false;

    auto accessManager = resourceAccessManager();

    int noAccess = 0;
    bool othersLocal = false;

    if (layout)
    {
        auto parentResource = layout->getParentResource();

        for (const auto& user: users)
        {
            if (!accessManager->hasPermission(user, layout, Qn::ReadPermission))
                ++noAccess;

            if (parentResource && parentResource->getId() != user->getId())
                othersLocal = true;
        }

        for (const auto& role: roles)
        {
            if (m_validator->roleValidity(role) != QValidator::Acceptable)
                ++noAccess;
        }
    }

    if (othersLocal)
    {
        displayWarning(LayoutWarning::LocalResource);
        foundLayoutWarning = true;
    }

    // Right now we show layout warning only if a local layout is selected.
    // According to the spec, we shouldn't show additional user warnings in this case.
    if (!foundLayoutWarning)
    {
        if (noAccess > 0)
        {
            displayWarning(noAccess == users.size() + roles.size() ? UserWarning::NobodyHasAccess : UserWarning::MissingAccess);
            foundUserWarning = true;
        }

        if (roles.size() && !users.size())
        {
            displayWarning(UserWarning::EmptyRoles);
            foundUserWarning = true;
        }
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

std::pair<QnUserResourceList, QList<QnUuid>> OpenLayoutActionWidget::getSelectedUsersAndRoles()
{
    QnUserResourceList users;
    QList<QnUuid> groupIds;
    if (model())
    {
        const auto params = model()->actionParams();
        if (params.allUsers)
            return {{}, {}};

        nx::vms::common::getUsersAndGroups(systemContext(),
            params.additionalResources, users, groupIds);
        users.append(systemContext()->accessSubjectHierarchy()->usersInGroups(
            nx::utils::toQSet(groupIds)));
        users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
    }

    return std::pair(users, groupIds);
}

void OpenLayoutActionWidget::updateLayoutsButton()
{
    if (!model())
        return;

    const auto icon = [](QnResourceIconCache::Key key, QnIcon::Mode mode)
        {
            return core::Skin::maximumSizePixmap(qnResIconCache->icon(key), mode);
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

    QnUserResourceList users;
    QList<QnUuid> roles;
    std::tie(users, roles) = getSelectedUsersAndRoles();

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
        auto user = users.front();
        localLayouts = resourcePool()->getResources<QnLayoutResource>(
            [selection, user](const QnLayoutResourcePtr& layout)
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
        dialog.showInfo(tr("Looking for a local layout? Select only one user from the \"Show to\" "
            "list to display their local layouts as an option here."));
        selectionMode = LayoutSelectionDialog::ModeHideLocal;
    }
    dialog.setLocalLayouts(localLayouts, selection, selectionMode);

    // Gather shared layouts. No trickery is here.
    QnResourceList sharedLayouts = resourcePool()->getResources<QnLayoutResource>(
        [](const QnLayoutResourcePtr& layout)
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
