#include "user_group_settings_widget.h"
#include "ui_user_group_settings_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/models/resource_properties/user_grous_settings_model.h>
#include <ui/style/custom_style.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

namespace
{
    const int kUserGroupIdRole = Qt::UserRole + 1;
    const int kPermissionsRole = Qt::UserRole + 2;
}

QnUserGroupSettingsWidget::QnUserGroupSettingsWidget(QnUserGroupSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserGroupSettingsWidget()),
    m_model(model),
    m_usersModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    ui->usersLabel->setVisible(false);
    ui->usersListTreeView->setVisible(false);
    ui->usersListTreeView->setModel(m_usersModel);
}

QnUserGroupSettingsWidget::~QnUserGroupSettingsWidget()
{}

bool QnUserGroupSettingsWidget::hasChanges() const
{
    return false;
}

void QnUserGroupSettingsWidget::loadDataToUi()
{
    ui->nameLineEdit->setText(m_model->groupName());

    QnUserResourceList users = m_model->userGroupId().isNull()
        ? QnUserResourceList()
        : qnResPool->getResources<QnUserResource>().filtered([this](const QnUserResourcePtr& user)
    {
        return user->userGroup() == m_model->userGroupId();
    });

    ui->usersLabel->setVisible(!users.isEmpty());
    ui->usersListTreeView->setVisible(!users.isEmpty());

    m_usersModel->clear();
    for (const auto& user : users)
    {
        QStandardItem* item = new QStandardItem(qnResIconCache->icon(QnResourceIconCache::User), user->getName());
        m_usersModel->appendRow(item);
    }

}

void QnUserGroupSettingsWidget::applyChanges()
{

}
