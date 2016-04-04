#include "user_access_rights_widget.h"
#include "ui_user_access_rights_widget.h"


QnUserAccessRightsWidget::QnUserAccessRightsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::UserAccessRightsWidget())
{
    ui->setupUi(this);
}

QnUserAccessRightsWidget::~QnUserAccessRightsWidget()
{

}
