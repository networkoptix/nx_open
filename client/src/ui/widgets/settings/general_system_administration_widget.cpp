#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(QWidget *parent /*= NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralSystemAdministrationWidget)
{
    ui->setupUi(this);
}

void QnGeneralSystemAdministrationWidget::updateFromSettings() {
    ui->cameraWidget->updateFromSettings();
    ui->backupWidget->updateFromSettings();
}

void QnGeneralSystemAdministrationWidget::submitToSettings() {
    ui->cameraWidget->submitToSettings();
    ui->backupWidget->submitToSettings();
}

