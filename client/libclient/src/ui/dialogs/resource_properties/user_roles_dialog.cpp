#include "user_roles_dialog.h"
#include "ui_user_roles_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/common/indents.h>
#include <ui/models/resource_properties/user_roles_settings_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/properties/user_role_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/utils/string.h>


QnUserRolesDialog::QnUserRolesDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::UserRolesDialog()),
    m_model(new QnUserRolesSettingsModel(this))
{
    ui->setupUi(this);
    setTabWidget(ui->tabWidget);

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

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(ui->groupsPanel);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->groupsTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    int margin = style()->pixelMetric(QStyle::PM_DefaultTopLevelMargin);
    ui->groupsTreeView->setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(margin, margin)));

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);

    m_model->setRoles(qnUserRolesManager->userRoles());
    ui->groupsTreeView->setModel(m_model);

    connect(ui->groupsTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
        &QnUserRolesDialog::at_model_currentChanged);

    connect(ui->newGroupButton, &QPushButton::clicked, this,
        [this]
        {
            QStringList usedNames;
            for (const auto& group: m_model->roles())
                usedNames << group.name;

            ec2::ApiUserGroupData group;
            group.id = QnUuid::createUuid();
            group.name = nx::utils::generateUniqueString(usedNames, tr("New Role"), tr("New Role %1"));
            group.permissions = Qn::NoGlobalPermissions;

            int row = m_model->addRole(group);
            ui->groupsTreeView->selectionModel()->setCurrentIndex(m_model->index(row),
                QItemSelectionModel::ClearAndSelect);
            updateButtonBox();
        });

    auto modelChanged = [this]()
        {
            bool hasGroups = m_model->rowCount() > 0;
            ui->groupsTreeView->setVisible(hasGroups);
            ui->groupsListUnderline->setVisible(hasGroups);

            if (hasGroups && !ui->groupsTreeView->selectionModel()->currentIndex().isValid())
            {
                ui->groupsTreeView->selectionModel()->setCurrentIndex(
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

bool QnUserRolesDialog::selectGroup(const QnUuid& groupId)
{
    ec2::ApiUserGroupDataList groups = m_model->roles();
    ec2::ApiUserGroupDataList::const_iterator group = std::find_if(groups.cbegin(), groups.cend(),
        [groupId](const ec2::ApiUserGroupData& elem)
        {
            return elem.id == groupId;
        });

    if (group == groups.end())
        return false;

    QModelIndex index = m_model->index(group - groups.begin());
    ui->groupsTreeView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectCurrent);

    return true;
}

bool QnUserRolesDialog::canApplyChanges() const
{
    return m_model->isValid() && base_type::canApplyChanges();
}

bool QnUserRolesDialog::hasChanges() const
{
    /* Quick check */
    if (qnUserRolesManager->userRoles().size() != m_model->roles().size())
        return true;

    for (const auto& group : m_model->roles())
    {
        auto existing = qnUserRolesManager->userRole(group.id);

        if (existing != group)
            return true;

        if (qnSharedResourcesManager->sharedResources(group) != m_model->accessibleResources(group))
            return true;
    }

    return base_type::hasChanges();
}

void QnUserRolesDialog::applyChanges()
{
    /* Here all changes are submitted to model. */
    base_type::applyChanges();

    if (!canApplyChanges())
        return;

    QSet<QnUuid> groupsLeft;
    for (const auto& group : m_model->roles())
    {
        groupsLeft << group.id;
        auto existing = qnUserRolesManager->userRole(group.id);

        if (existing != group)
            qnResourcesChangesManager->saveUserRole(group);

        auto resources = m_model->accessibleResources(group);
        QnLayoutResourceList layoutsToShare = qnResPool->getResources(resources)
            .filtered<QnLayoutResource>(
                [](const QnLayoutResourcePtr& layout)
                {
                    return !layout->isFile() && !layout->isShared();
                });

        for (const auto& layout : layoutsToShare)
        {
            menu()->trigger(QnActions::ShareLayoutAction,
                QnActionParameters(layout).withArgument(Qn::UuidRole, group.id));
        }

        qnResourcesChangesManager->saveAccessibleResources(group, resources);
    }

    for (const auto& group : qnUserRolesManager->userRoles())
    {
        if (groupsLeft.contains(group.id))
            continue;

        const auto& groupUsers = m_model->users(group.id, false);
        auto replacement = m_model->replacement(group.id);

        if (replacement.isEmpty())
        {
            qnResourcesChangesManager->deleteResources(groupUsers);
        }
        else
        {
            for (const auto& user : groupUsers)
            {
                qnResourcesChangesManager->saveUser(user,
                    [replacement](const QnUserResourcePtr& user)
                    {
                        user->setUserGroup(replacement.role);
                        user->setRawPermissions(replacement.permissions);
                    });

                if (replacement.permissions == Qn::GlobalCustomUserPermission)
                    qnResourcesChangesManager->saveAccessibleResources(user, QSet<QnUuid>());
            }
        }

        qnResourcesChangesManager->removeUserRole(group.id);
    }

    updateButtonBox();
    loadDataToUi();
}

void QnUserRolesDialog::at_model_currentChanged(const QModelIndex& current)
{
    bool valid = current.isValid();
    ui->groupInfoStackedWidget->setCurrentWidget(valid ? ui->groupInfoPage : ui->noGroupsPage);
    if (!valid)
        return;

    QnUuid groupId = current.data(Qn::UuidRole).value<QnUuid>();
    if (m_model->selectedRole() == groupId)
        return;

    /* Apply page changes to current group in the model. */
    base_type::applyChanges();
    m_model->selectRole(groupId);
    loadDataToUi();
}

