#include "user_group_settings_widget.h"
#include "ui_user_group_settings_widget.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

#include <ui/common/indents.h>
#include <ui/models/resource_properties/user_groups_settings_model.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>


QnUserGroupSettingsWidget::QnUserGroupSettingsWidget(QnUserGroupSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserGroupSettingsWidget()),
    m_model(model),
    m_usersModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->usersListTreeView->setModel(m_usersModel);

    ui->usersListTreeView->setProperty(style::Properties::kSuppressHoverPropery, true);
    ui->usersListTreeView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents()));

    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &QnUserGroupSettingsWidget::applyChanges);

    connect(ui->deleteGroupButton, &QPushButton::clicked, this, [this]()
    {
        m_model->removeGroup(m_model->selectedGroup());
        emit hasChangesChanged();
    });
}

QnUserGroupSettingsWidget::~QnUserGroupSettingsWidget()
{
}

bool QnUserGroupSettingsWidget::hasChanges() const
{
    return false;
}

void QnUserGroupSettingsWidget::loadDataToUi()
{
    QSignalBlocker blocker(ui->nameLineEdit);
    ui->nameLineEdit->setText(m_model->groupName());

    QnUserResourceList users = m_model->selectedGroup().isNull()
        ? QnUserResourceList()
        : qnResPool->getResources<QnUserResource>().filtered([this](const QnUserResourcePtr& user)
    {
        return user->userGroup() == m_model->selectedGroup();
    });

    m_usersModel->clear();
    for (const auto& user : users)
        m_usersModel->appendRow(new QStandardItem(qnResIconCache->icon(QnResourceIconCache::User), user->getName()));

    if (users.isEmpty())
        m_usersModel->appendRow(new QStandardItem(tr("No users have this role")));

}

void QnUserGroupSettingsWidget::applyChanges()
{
    m_model->setGroupName(ui->nameLineEdit->text());
    emit hasChangesChanged();
}
