#include "user_roles_dialog.h"
#include "ui_user_roles_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/common/indents.h>
#include <ui/models/resource_properties/user_roles_settings_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/properties/user_role_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/utils/raii_guard.h>
#include <nx/utils/string.h>

using namespace nx::client::desktop::ui;

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
        new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::MediaFilter, this),
        tr("Cameras && Resources"));
    addPage(
        LayoutsPage,
        new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::LayoutsFilter, this),
        tr("Layouts"));

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->userRolesPanel);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->userRolesTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
    ui->userRolesTreeView->setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(margin, margin)));

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    // Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code.
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);

    m_model->setUserRoles(userRolesManager()->userRoles());
    ui->userRolesTreeView->setModel(m_model);

    connect(ui->userRolesTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
        &QnUserRolesDialog::at_model_currentChanged);

    connect(ui->newUserRoleButton, &QPushButton::clicked, this,
        [this]
        {
            QStringList usedNames;
            for (const auto& userRole: m_model->userRoles())
                usedNames << userRole.name;

            ec2::ApiUserRoleData userRole;
            userRole.id = QnUuid::createUuid();
            userRole.name = nx::utils::generateUniqueString(
                usedNames, tr("New Role"), tr("New Role %1"));
            userRole.permissions = Qn::NoGlobalPermissions;

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
    ec2::ApiUserRoleDataList userRoles = m_model->userRoles();
    ec2::ApiUserRoleDataList::const_iterator userRole = std::find_if(
        userRoles.cbegin(), userRoles.cend(),
        [userRoleId](const ec2::ApiUserRoleData& elem)
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

        if (sharedResourcesManager()->sharedResources(userRole) !=
            m_model->accessibleResources(userRole))
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

        if (existing != userRole)
            qnResourcesChangesManager->saveUserRole(userRole);

        auto resources = m_model->accessibleResources(userRole);
        QnLayoutResourceList layoutsToShare = resourcePool()->getResources(resources)
            .filtered<QnLayoutResource>(
                [](const QnLayoutResourcePtr& layout)
                {
                    return !layout->isFile() && !layout->isShared();
                });

        for (const auto& layout: layoutsToShare)
        {
            menu()->trigger(action::ShareLayoutAction,
                action::Parameters(layout).withArgument(Qn::UuidRole, userRole.id));
        }

        qnResourcesChangesManager->saveAccessibleResources(userRole, resources);
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
            qnResourcesChangesManager->deleteResources(usersWithUserRole,
                [roleId = userRole.id](bool success)
                {
                    if (success)
                        qnResourcesChangesManager->removeUserRole(roleId);
                });
        }
        else
        {
            const auto deleteRoleGuard = QnRaiiGuard::createDestructible(
                [roleId = userRole.id]()
                {
                    qnResourcesChangesManager->removeUserRole(roleId);
                });

            const auto handleUserSaved =
                [deleteRoleGuard](bool success, const QnUserResourcePtr& /*user*/)
                {
                    if (!success)
                        deleteRoleGuard->disableDestructionHandler();
                };

            const auto applyChanges =
                [replacement](const QnUserResourcePtr& user)
                {
                    user->setUserRoleId(replacement.userRoleId);
                    user->setRawPermissions(replacement.permissions);
                };

            for (const auto& user: usersWithUserRole)
            {
                qnResourcesChangesManager->saveUser(user, applyChanges, handleUserSaved);

                if (replacement.permissions == Qn::GlobalCustomUserPermission)
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

