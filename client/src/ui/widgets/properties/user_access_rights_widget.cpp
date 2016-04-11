#include "user_access_rights_widget.h"
#include "ui_user_access_rights_widget.h"

#include <ui/style/custom_style.h>

QnUserAccessRightsWidget::QnUserAccessRightsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::UserAccessRightsWidget())
{
    ui->setupUi(this);
    setTabShape(ui->tabWidget->tabBar(), style::TabShape::Compact);
    connect(ui->camerasTab, &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

QnUserAccessRightsWidget::~QnUserAccessRightsWidget()
{

}

QSet<QnUuid> QnUserAccessRightsWidget::accessibleResources() const
{
    return m_data;
}

void QnUserAccessRightsWidget::setAccessibleResources(const QSet<QnUuid>& value)
{
    m_data = value;
}

bool QnUserAccessRightsWidget::hasChanges() const
{
    return ui->camerasTab->hasChanges();
}

void QnUserAccessRightsWidget::loadDataToUi()
{
    ui->camerasTab->setCheckedResources(m_data); //TODO: #GDM filter by cameras, otherwise we will have problems on saving
    ui->camerasTab->loadDataToUi();
}

void QnUserAccessRightsWidget::applyChanges()
{
    ui->camerasTab->applyChanges();
    m_data = ui->camerasTab->checkedResources(); //TODO: #GDM filter by cameras, otherwise we will have problems on saving
}
