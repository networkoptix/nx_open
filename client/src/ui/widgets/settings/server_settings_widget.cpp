#include "server_settings_widget.h"
#include "ui_server_settings_widget.h"

QnServerSettingsWidget::QnServerSettingsWidget(QWidget *parent, Qt::WindowFlags windowFlags):
    QWidget(parent, windowFlags),
    ui(new Ui::ServerSettingsWidget)
{
    ui->setupUi(this);
}

QnServerSettingsWidget::~QnServerSettingsWidget() {
    return;
}

void QnServerSettingsWidget::submitToSettings() {
    ui->smtpSettingsWidget->submitToSettings();
}

void QnServerSettingsWidget::updateFromSettings() {
    ui->smtpSettingsWidget->updateFromSettings();
}

void QnServerSettingsWidget::updateFocusedElement() {
    ui->smtpSettingsWidget->updateFocusedElement();
}
