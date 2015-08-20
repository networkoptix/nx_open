#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <ui/widgets/settings/private/user_management_widget_p.h>

QnUserManagementWidget::QnUserManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::QnUserManagementWidget)
    , d_ptr(new QnUserManagementWidgetPrivate(this))
{
    Q_D(QnUserManagementWidget);

    ui->setupUi(this);
    d->setupUi();
}

QnUserManagementWidget::~QnUserManagementWidget() {
}
