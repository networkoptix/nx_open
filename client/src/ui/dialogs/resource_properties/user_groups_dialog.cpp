#include "user_groups_dialog.h"
#include "ui_user_groups_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_access_manager.h>

#include <ui/models/resource_properties/user_grous_settings_model.h>
#include <ui/widgets/properties/user_group_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/string.h>

QnUserGroupsDialog::QnUserGroupsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::UserGroupsDialog()),
    m_model(new QnUserGroupSettingsModel(this)),
    m_settingsPage(new QnUserGroupSettingsWidget(m_model, this)),
    m_permissionsPage(new QnPermissionsWidget(m_model, this)),
    m_camerasPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::CamerasFilter, this)),
    m_layoutsPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::LayoutsFilter, this)),
    m_serversPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::ServersFilter, this))
{
    ui->setupUi(this);

    addPage(SettingsPage, m_settingsPage, tr("Group Info"));
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

    m_model->setGroups(qnResourceAccessManager->userGroups());
    ui->groupsTreeView->setModel(m_model);

    connect(ui->groupsTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex& previous)
    {
        bool valid = current.isValid();

        ui->groupInfoStackedWidget->setCurrentWidget(valid
            ? ui->groupInfoPage
            : ui->noGroupsPage
        );

        if (!valid)
            return;

        QnUuid groupId = current.data(Qn::UuidRole).value<QnUuid>();
        if (m_model->selectedGroup() == groupId)
            return;

        /* Changes are locally stored in model. */
        for (auto page : allPages())
            page.widget->applyChanges();
        m_model->selectGroup(groupId);
        loadDataToUi();
    });

    connect(ui->newGroupButton, &QPushButton::clicked, this, [this]
    {
        QStringList usedNames;
        for (const auto& group: m_model->groups())
            usedNames << group.name;

        ec2::ApiUserGroupData group;
        group.id = QnUuid::createUuid();
        group.name = generateUniqueString(usedNames, tr("New Group"), tr("New Group %1"));
        group.permissions = Qn::GlobalViewLivePermission;

        int row = m_model->addGroup(group);
        ui->groupsTreeView->selectionModel()->setCurrentIndex(m_model->index(row), QItemSelectionModel::ClearAndSelect);
        updateButtonBox();
    });
}

QnUserGroupsDialog::~QnUserGroupsDialog()
{}

bool QnUserGroupsDialog::hasChanges() const
{
    /* Quick check */
    if (qnResourceAccessManager->userGroups().size() != m_model->groups().size())
        return true;

    for (const auto& group : m_model->groups())
    {
        auto existing = qnResourceAccessManager->userGroup(group.id);

        if (existing == group)
            continue;

        return true;
    }

    return false;
}

void QnUserGroupsDialog::applyChanges()
{
    base_type::applyChanges();

    QSet<QnUuid> groupsLeft;
    for (const auto& group : m_model->groups())
    {
        groupsLeft << group.id;
        auto existing = qnResourceAccessManager->userGroup(group.id);

        if (existing == group)
            continue;

        qnResourcesChangesManager->saveUserGroup(group);
    }

    for (const auto& group : qnResourceAccessManager->userGroups())
    {
        if (groupsLeft.contains(group.id))
            continue;

        qnResourcesChangesManager->removeUserGroup(group.id);
    }

    updateButtonBox();
}

