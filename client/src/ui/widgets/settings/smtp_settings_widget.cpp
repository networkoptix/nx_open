#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

//TODO: #GDM use documentation from http://support.google.com/mail/bin/answer.py?hl=en&answer=1074635

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnSmtpSettingsWidget)
{
    ui->setupUi(this);

    connect(ui->portComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_portComboBox_currentIndexChanged(int)));
    at_portComboBox_currentIndexChanged(ui->portComboBox->currentIndex());
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
    delete ui;
}

void QnSmtpSettingsWidget::at_portComboBox_currentIndexChanged(int index) {

    /*
     * index = 0: port 587, default for TLS
     * index = 1: port 465, default for SSL
     * index = 2: port 25,  default for unsecured, often supports TLS
     */

    ui->unsecuredRadioButton->setEnabled(index == 2);
    ui->tlsRecommendedLabel->setVisible(index != 1);
    ui->sslRecommendedLabel->setVisible(index == 1);

    if (index == 1)
        ui->sslRadioButton->setChecked(true);
    else
        ui->tlsRadioButton->setChecked(true);
}
