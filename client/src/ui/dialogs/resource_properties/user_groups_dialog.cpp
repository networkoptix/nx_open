#include "user_groups_dialog.h"
#include "ui_user_groups_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_access_manager.h>

#include <ui/models/resource_properties/user_grous_settings_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/properties/user_group_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

QnUserGroupsDialog::QnUserGroupsDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::UserGroupsDialog()),
    m_model(new QnUserGroupSettingsModel(this)),
    m_settingsPage(new QnUserGroupSettingsWidget(m_model, this)),
    m_permissionsPage(new QnPermissionsWidget(m_model, this)),
    m_camerasPage(new QnAccessibleResourcesWidget(m_model, QnAbstractPermissionsModel::CamerasFilter, this)),
    m_layoutsPage(new QnAccessibleResourcesWidget(m_model, QnAbstractPermissionsModel::LayoutsFilter, this)),
    m_serversPage(new QnAccessibleResourcesWidget(m_model, QnAbstractPermissionsModel::ServersFilter, this))
{
    ui->setupUi(this);

    addPage(SettingsPage, m_settingsPage, tr("Group Information"));
    addPage(PermissionsPage, m_permissionsPage, tr("Permissions"));
    addPage(CamerasPage, m_camerasPage, tr("Cameras"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));
    addPage(ServersPage, m_serversPage, tr("Servers"));

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);

    QStandardItemModel* groupsModel = new QStandardItemModel(this);
    for (const auto& group : qnResourceAccessManager->userGroups())
    {
        QString name = group.name;
        if (name.isEmpty())
            name = tr("<Unnamed Group>");

        QStandardItem* item = new QStandardItem(qnResIconCache->icon(QnResourceIconCache::Users), name);
        item->setData(qVariantFromValue(group.id), Qt::UserRole + 1);
        qDebug() << "added group id" << group.id.toString();
        groupsModel->appendRow(item);
    }
    ui->groupsTreeView->setModel(groupsModel);

    connect(ui->groupsTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex& previous)
    {
        if (!current.isValid())
            return;
        QnUuid groupId = current.data(Qt::UserRole + 1).value<QnUuid>();
        qDebug() << "selected group id" << groupId.toString();
    });

    connect(ui->newGroupButton, &QPushButton::clicked, this, [this, groupsModel]
    {
        ec2::ApiUserGroupData group;
        group.id = QnUuid::createUuid();
        group.name = group.id.toSimpleString();
        group.permissions = Qn::GlobalViewLivePermission;
        qnResourcesChangesManager->saveUserGroup(group);

        QStandardItem* item = new QStandardItem(qnResIconCache->icon(QnResourceIconCache::Users), group.name);
        item->setData(qVariantFromValue(group.id), Qt::UserRole + 1);
        qDebug() << "added group id" << group.id.toString();
        groupsModel->appendRow(item);
    });
}

QnUserGroupsDialog::~QnUserGroupsDialog()
{}


QnUuid QnUserGroupsDialog::userGroupId() const
{
    return m_model->userGroupId();
}

void QnUserGroupsDialog::setUserGroupId(const QnUuid& value)
{
    if (m_model->userGroupId() == value)
        return;

    if (!tryClose(false))
        return;

    m_model->setUserGroupId(value);
    loadDataToUi();
}

void QnUserGroupsDialog::retranslateUi()
{
    base_type::retranslateUi();
   /* if (!m_user)
        return;

    if (m_model->mode() == QnUserSettingsModel::NewUser)
    {
        setWindowTitle(tr("New User..."));
        setHelpTopic(this, Qn::NewUser_Help);
    }
    else
    {
        bool readOnly = !accessController()->hasPermissions(m_user, Qn::WritePermission | Qn::SavePermission);
        setWindowTitle(readOnly
            ? tr("User Settings - %1 (readonly)").arg(m_user->getName())
            : tr("User Settings - %1").arg(m_user->getName())
            );
        setHelpTopic(this, Qn::UserSettings_Help);
    }*/
}

bool QnUserGroupsDialog::hasChanges() const
{
    return base_type::hasChanges();

    //if (base_type::hasChanges())
    //    return true;

    ///* Permissions are checked separately */
    //if (isPageVisible(SettingsPage))
    //{
    //    if (!m_settingsPage->selectedUserGroup().isNull())
    //        return false;

    //    Qn::GlobalPermissions basePermissions = m_settingsPage->selectedPermissions();
    //    if (basePermissions != Qn::NoGlobalPermissions)
    //        /* Predefined group selected */
    //        return basePermissions != qnResourceAccessManager->globalPermissions(m_user))


    //}



}

void QnUserGroupsDialog::applyChanges()
{

/*    qnResourcesChangesManager->saveUser(m_user, [this, mode](const QnUserResourcePtr &user)
    {
        for (const auto& page : allPages())
        {
            if (page.enabled && page.visible)
                page.widget->applyChanges();
        }
    });

*/
    updateButtonBox();
}

