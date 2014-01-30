#include "server_settings_widget.h"
#include "ui_server_settings_widget.h"

QnServerSettingsWidget::QnServerSettingsWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::ServerSettingsWidget)
{
    ui->setupUi(this);
}

QnServerSettingsWidget::~QnServerSettingsWidget() {
    return;
}

void QnServerSettingsWidget::submitToSettings() {
    ui->smtpSettingsWidget->submitToSettings();
    ui->cameraSettingsWidget->submitToSettings();
}

void QnServerSettingsWidget::updateFromSettings() {
    ui->smtpSettingsWidget->updateFromSettings();
    ui->cameraSettingsWidget->updateFromSettings();
}
