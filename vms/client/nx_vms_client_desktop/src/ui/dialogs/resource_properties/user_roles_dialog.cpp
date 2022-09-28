// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_dialog.h"
#include "ui_user_roles_dialog.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <ui/common/indents.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_properties/user_roles_settings_model.h>
#include <ui/widgets/properties/accessible_layouts_widget.h>
#include <ui/widgets/properties/accessible_media_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/widgets/properties/user_role_settings_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

using nx::vms::api::UserRoleData;
using nx::vms::api::UserRoleDataList;

namespace {

QnLayoutResourceList layoutsToShare(const QnResourcePool* resourcePool,
    const QnUuidSet& accessibleResources)
{
    if (!NX_ASSERT(resourcePool))
        return {};

    return resourcePool->getResourcesByIds(accessibleResources).filtered<QnLayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return !layout->isFile() && !layout->isShared();
        });
}

} // namespace


QnUserRolesDialog::QnUserRolesDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::UserRolesDialog()),
    m_model(new QnUserRolesSettingsModel(this))
{
    ui->setupUi(this);
    setTabWidget(ui->tabWidget);

    setHelpTopic(this, Qn::UserRoles_Help);

    addPage(
        SettingsPage,
        new QnUserRoleSettingsWidget(m_model, this),
        tr("Role Info"));
    addPage(
        PermissionsPage,
        new QnPermissionsWidget(m_model, this),
        tr("Permissions"));
    addPage(
        CamerasPage,
        new QnAccessibleMediaWidget(m_model, this),
        tr("Cameras && Resources"));
    addPage(
        LayoutsPage,
        new QnAccessibleLayoutsWidget(m_model, this),
        tr("Layouts"));

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(ui->userRolesPanel);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->userRolesTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
    ui->userRolesTreeView->setProperty(nx::style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(margin, margin)));

    m_model->setUserRoles(userRolesManager()->userRoles());
    ui->userRolesTreeView->setModel(m_model);

    ui->userRolesTreeView->setCustomSizeHint(true);

    connect(ui->userRolesTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
        &QnUserRolesDialog::at_model_currentChanged);

    connect(ui->newUserRoleButton, &QPushButton::clicked, this,
        [this]
        {
            QStringList usedNames;
            for (const auto& userRole: m_model->userRoles())
                usedNames << userRole.name;

            UserRoleData userRole;
            userRole.id = QnUuid::createUuid();
            userRole.name = nx::utils::generateUniqueString(
                usedNames, tr("New Role"), tr("New Role %1"));
            userRole.permissions = {};

            int row = m_model->addUserRole(userRole);
            ui->userRolesTreeView->selectionModel()->setCurrentIndex(m_model->index(row),
                QItemSelectionModel::ClearAndSelect);
            updateButtonBox();
        });

    auto modelChanged = [this]()
        {
            bool hasUserRoles = m_model->rowCount() > 0;
            ui->userRolesTreeView->setVisible(hasUserRoles);
            ui->userRolesListUnderline->setVisible(hasUserRoles);

            if (hasUserRoles && !ui->userRolesTreeView->selectionModel()->currentIndex().isValid())
            {
                ui->userRolesTreeView->selectionModel()->setCurrentIndex(
                    m_model->index(0, 0),
                    QItemSelectionModel::SelectCurrent);
            }
        };

    connect(m_model, &QAbstractItemModel::modelReset,   this, modelChanged);
    connect(m_model, &QAbstractItemModel::rowsRemoved,  this, modelChanged);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, modelChanged);

    modelChanged();
}

QnUserRolesDialog::~QnUserRolesDialog()
{
}

bool QnUserRolesDialog::selectUserRole(const QnUuid& userRoleId)
{
    UserRoleDataList userRoles = m_model->userRoles();
    UserRoleDataList::const_iterator userRole = std::find_if(
        userRoles.cbegin(), userRoles.cend(),
        [userRoleId](const UserRoleData& elem)
        {
            return elem.id == userRoleId;
        });

    if (userRole == userRoles.end())
        return false;

    QModelIndex index = m_model->index(userRole - userRoles.begin());
    ui->userRolesTreeView->selectionModel()->setCurrentIndex(
        index, QItemSelectionModel::SelectCurrent);

    return true;
}

bool QnUserRolesDialog::canApplyChanges() const
{
    return m_model->isValid() && base_type::canApplyChanges();
}

bool QnUserRolesDialog::hasChanges() const
{
    /* Quick check */
    if (userRolesManager()->userRoles().size() != m_model->userRoles().size())
        return true;

    for (const auto& userRole: m_model->userRoles())
    {
        auto existing = userRolesManager()->userRole(userRole.id);

        if (existing != userRole)
            return true;

        const auto modelResources = m_model->accessibleResources(userRole);
        // Checking whether a changes were applied.
        if (m_sharedResourcesQueuedToSave.contains(userRole.id))
        {
            if(m_sharedResourcesQueuedToSave.value(userRole.id) != modelResources)
                return true; //< User has changed something while changes are still applying.
        }
        else if (sharedResourcesManager()->sharedResources(userRole) != modelResources)
        {
            return true;
        }
    }

    return base_type::hasChanges();
}

void QnUserRolesDialog::applyChanges()
{
    // Here all changes are submitted to the model.
    base_type::applyChanges();

    if (!canApplyChanges())
        return;

    QSet<QnUuid> userRolesLeft;
    for (const auto& userRole: m_model->userRoles())
    {
        userRolesLeft << userRole.id;
        auto existing = userRolesManager()->userRole(userRole.id);

        const auto accessibleResources = m_model->accessibleResources(userRole);
        m_sharedResourcesQueuedToSave.insert(userRole.id, accessibleResources);

        auto setupAccessibleResources =
            [accessibleResources,
                dialog = QPointer(this),
                resourcePool = QPointer<QnResourcePool>(context()->resourcePool()),
                actionManager = QPointer<action::Manager>(menu())](
                    bool roleIsStored,
                    const UserRoleData& role)
            {
                if (dialog)
                    dialog->m_sharedResourcesQueuedToSave.remove(role.id);

                if (!roleIsStored || !resourcePool || !actionManager)
                    return;

                const auto layouts = layoutsToShare(resourcePool, accessibleResources);
                for (const auto& layout: layouts)
                {
                    actionManager->trigger(action::ShareLayoutAction,
                        action::Parameters(layout).withArgument(Qn::UuidRole, role.id));
                }
                qnResourcesChangesManager->saveAccessibleResources(role, accessibleResources);
            };

        if (existing != userRole)
            qnResourcesChangesManager->saveUserRole(userRole, setupAccessibleResources);
        else
            setupAccessibleResources(/*roleIsStored*/ true, userRole);
    }

    for (const auto& userRole: userRolesManager()->userRoles())
    {
        if (userRolesLeft.contains(userRole.id))
            continue;

        const auto& usersWithUserRole = m_model->users(userRole.id, false);
        if (usersWithUserRole.isEmpty())
        {
            qnResourcesChangesManager->removeUserRole(userRole.id);
            continue;
        }

        const auto replacement = m_model->replacement(userRole.id);
        if (replacement.isEmpty())
        {
            auto callback = nx::utils::guarded(this,
                [roleId = userRole.id](bool success)
                {
                    if (success)
                        qnResourcesChangesManager->removeUserRole(roleId);
                });

            qnResourcesChangesManager->deleteResources(usersWithUserRole, callback);
        }
        else
        {
            const auto deleteRoleGuard = nx::utils::makeSharedGuard(
                [roleId = userRole.id]()
                {
                    qnResourcesChangesManager->removeUserRole(roleId);
                });

            const auto handleUserSaved = nx::utils::guarded(this,
                [deleteRoleGuard](bool success, const QnUserResourcePtr& /*user*/)
                {
                    if (!success)
                        deleteRoleGuard->disarm();
                });

            const auto applyChanges = nx::utils::guarded(this,
                [replacement](const QnUserResourcePtr& user)
                {
                    user->setUserRoleId(replacement.userRoleId);
                    user->setRawPermissions(replacement.permissions);
                });

            for (const auto& user: usersWithUserRole)
            {
                qnResourcesChangesManager->saveUser(
                    user, QnUserResource::DigestSupport::keep, applyChanges, handleUserSaved);

                if (replacement.permissions == GlobalPermissions(GlobalPermission::customUser))
                    qnResourcesChangesManager->saveAccessibleResources(user, QSet<QnUuid>());
            }
        }
    }

    updateButtonBox();
    loadDataToUi();
}

void QnUserRolesDialog::at_model_currentChanged(const QModelIndex& current)
{
    bool valid = current.isValid();
    ui->userRoleInfoStackedWidget->setCurrentWidget(
        valid ? ui->userRoleInfoPage : ui->noUserRolesPage);
    if (!valid)
        return;

    QnUuid userRoleId = current.data(Qn::UuidRole).value<QnUuid>();
    if (m_model->selectedUserRoleId() == userRoleId)
        return;

    // Apply page changes to the current role in the model.
    base_type::applyChanges();
    m_model->selectUserRoleId(userRoleId);
    loadDataToUi();
}
