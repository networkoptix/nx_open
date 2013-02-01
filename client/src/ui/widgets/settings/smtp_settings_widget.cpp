#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtGui/QMessageBox>

#include <api/app_server_connection.h>

//TODO: #GDM use documentation from http://support.google.com/mail/bin/answer.py?hl=en&answer=1074635

namespace {

    const QLatin1String nameFrom("EMAIL_FROM");
    const QLatin1String nameHost("EMAIL_HOST");
    const QLatin1String namePort("EMAIL_PORT");
    const QLatin1String nameUser("EMAIL_HOST_USER");
    const QLatin1String namePassword("EMAIL_HOST_PASSWORD");
    const QLatin1String nameTls("EMAIL_USE_TLS");

}

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnSmtpSettingsWidget),
    m_requestHandle(-1)
{
    ui->setupUi(this);

    connect(ui->portComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_portComboBox_currentIndexChanged(int)));
    at_portComboBox_currentIndexChanged(ui->portComboBox->currentIndex());
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
    delete ui;
}

void QnSmtpSettingsWidget::update() {
    m_requestHandle = QnAppServerConnectionFactory::createConnection()->getSettingsAsync(
                this, SLOT(at_settings_received(int,QByteArray,QnKvPairList,int)));
}

void QnSmtpSettingsWidget::submit() {
    bool useTls = ui->tlsRadioButton->isChecked();
    int port = ui->portComboBox->currentIndex() == 0 ? 587 : 25;

    QnKvPairList settings;
    settings
        << QnKvPair(nameHost, ui->serverLineEdit->text())
        << QnKvPair(namePort, QString::number(port))
        << QnKvPair(nameUser, ui->userLineEdit->text())
        << QnKvPair(nameFrom, ui->userLineEdit->text())
        << QnKvPair(namePassword, ui->passwordLineEdit->text())
        << QnKvPair(nameTls, useTls ? QLatin1String("True") : QLatin1String("False"));

    QnAppServerConnectionFactory::createConnection()->saveSettingsAsync(settings);
}

void QnSmtpSettingsWidget::at_portComboBox_currentIndexChanged(int index) {

    /*
     * index = 0: port 587, default for TLS
     * index = 1: port 25,  default for unsecured, often supports TLS
     */

    if (index == 0)
        ui->tlsRadioButton->setChecked(true);
}

void QnSmtpSettingsWidget::at_settings_received(int status, const QByteArray &errorString, const QnKvPairList &settings, int handle) {
    Q_UNUSED(errorString)
    if (handle != m_requestHandle)
        return;

    m_requestHandle = -1;

    bool success = (status == 0);
    if(!success) {
        //TODO: #GDM remove password from error message
        //QMessageBox::critical(this, tr("Error while receiving settings"), QString::fromLatin1(errorString));
        return;
    }

    foreach (const QnKvPair &setting, settings) {
        if (setting.name() == nameHost) {
            ui->serverLineEdit->setText(setting.value());
        } else if (setting.name() == namePort) {
            bool ok;
            int port = setting.value().toInt(&ok);
            int idx = (ok && port == 25) ? 1 : 0;
            ui->portComboBox->setCurrentIndex(idx);
        } else if (setting.name() == nameUser) {
            ui->userLineEdit->setText(setting.value());
        } else if (setting.name() == namePassword) {
            ui->passwordLineEdit->setText(setting.value());
        } else if (setting.name() == nameTls) {
            bool useTls = setting.value() == QLatin1String("True");
            if (useTls)
                ui->tlsRadioButton->setChecked(true);
            else
                ui->unsecuredRadioButton->setChecked(true);
        }
    }

}
