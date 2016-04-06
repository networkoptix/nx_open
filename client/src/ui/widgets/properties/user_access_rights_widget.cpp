#include "user_access_rights_widget.h"
#include "ui_user_access_rights_widget.h"


QnUserAccessRightsWidget::QnUserAccessRightsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::UserAccessRightsWidget())
{
    ui->setupUi(this);
    connect(ui->camerasTab, &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

QnUserAccessRightsWidget::~QnUserAccessRightsWidget()
{

}

ec2::ApiAccessRightsData QnUserAccessRightsWidget::accessRights() const
{
    return m_data;
}

void QnUserAccessRightsWidget::setAccessRights(const ec2::ApiAccessRightsData& value)
{
    m_data = value;
}

bool QnUserAccessRightsWidget::hasChanges() const
{
    return ui->camerasTab->hasChanges();
}

void QnUserAccessRightsWidget::loadDataToUi()
{
    QSet<QnUuid> accessible;
    for (const QnUuid& id : m_data.resourceIds)
        accessible << id;
    ui->camerasTab->setCheckedResources(accessible); //TODO: #GDM filter by cameras, otherwise we will have problems on saving
    ui->camerasTab->loadDataToUi();
}

void QnUserAccessRightsWidget::applyChanges()
{
    ui->camerasTab->applyChanges();

    m_data.resourceIds.clear();
    for (const QnUuid& id : ui->camerasTab->checkedResources()) //TODO: #GDM filter by cameras, otherwise we will have problems on saving
        m_data.resourceIds.push_back(id);
}
