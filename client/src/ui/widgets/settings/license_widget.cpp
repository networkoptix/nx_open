#include "license_widget.h"
#include "ui_license_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <utils/common/product_features.h>

#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>

#include <licensing/license.h>

namespace {
    bool isValidSerialKey(const QString &key) {
        return key.length() == qnProductFeatures().freeLicenseKey.length() && !key.contains(QLatin1Char(' '));
    }

} // anonymous namespace


QnLicenseWidget::QnLicenseWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::LicenseWidget),
    m_state(Normal)
{
    ui->setupUi(this);

    ui->serialKeyEdit->setInputMask(QLatin1String(">NNNN-NNNN-NNNN-NNNN"));
    ui->serialKeyEdit->setFocus();
    ui->activateFreeLicenseButton->setText(qnProductFeatures().freeLicenseIsTrial ? tr("Activate Trial License") : tr("Activate Free License"));

    ui->manualActivationInfoLabel->setText(tr(
        "Please send E-Mail with the Serial Key and the Hardware ID provided to <a href=\"mailto:%1\">%1</a>. "
        "Then we'll send you an Activation Key which should be filled in the field below."
    ).arg(QLatin1String(QN_LICENSING_MAIL_ADDRESS))); // TODO: #Elric move to product features?

    connect(ui->serialKeyEdit,              SIGNAL(textChanged(QString)),       this,   SLOT(updateControls()));
    connect(ui->activationTypeComboBox,     SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_activationTypeComboBox_currentIndexChanged()));
    connect(ui->browseLicenseFileButton,    SIGNAL(clicked()),                  this,   SLOT(at_browseLicenseFileButton_clicked()));
    connect(ui->activateLicenseButton,      SIGNAL(clicked()),                  this,   SLOT(at_activateLicenseButton_clicked()));
    connect(ui->activateFreeLicenseButton,  SIGNAL(clicked()),                  this,   SLOT(at_activateFreeLicenseButton_clicked()));

    at_activationTypeComboBox_currentIndexChanged();
}

QnLicenseWidget::~QnLicenseWidget() {
    return;
}

QnLicenseWidget::State QnLicenseWidget::state() const {
    return m_state;
}

void QnLicenseWidget::setState(State state) {
    if(m_state == state)
        return;

    m_state = state;

    updateControls();

    emit stateChanged();
}

bool QnLicenseWidget::isOnline() const {
    return ui->activationTypeComboBox->currentIndex() == 0;
}

void QnLicenseWidget::setOnline(bool online) {
    ui->activationTypeComboBox->setCurrentIndex(online ? 0 : 1);
}

bool QnLicenseWidget::isFreeLicenseAvailable() const {
    return ui->activateFreeLicenseButton->isVisible();
}

void QnLicenseWidget::setFreeLicenseAvailable(bool available) {
    ui->activateFreeLicenseButton->setVisible(available);
}

void QnLicenseWidget::setHardwareId(const QByteArray& hardwareId) {
    ui->hardwareIdEdit->setText(QLatin1String(hardwareId));
}

QString QnLicenseWidget::serialKey() const {
    return ui->serialKeyEdit->text();
}

void QnLicenseWidget::setSerialKey(const QString &serialKey) {
    ui->serialKeyEdit->setText(serialKey);
}

QByteArray QnLicenseWidget::activationKey() const {
    return ui->activationKeyTextEdit->toPlainText().toLatin1();
}

void QnLicenseWidget::updateControls() {
    ui->activateLicenseButton->setEnabled(isValidSerialKey(ui->serialKeyEdit->text()));

    if(m_state == Normal) {
        ui->activateLicenseButton->setText(tr("Activate License"));
        ui->activateLicenseButton->setEnabled(true);
    } else {
        ui->activateLicenseButton->setText(tr("Activating..."));
        ui->activateLicenseButton->setEnabled(false);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);

    if(event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
}

void QnLicenseWidget::at_activationTypeComboBox_currentIndexChanged() {
    bool isOnline = this->isOnline();

    ui->manualActivationInfoLabel->setVisible(!isOnline);
    ui->hardwareIdLabel->setVisible(!isOnline);
    ui->hardwareIdEdit->setVisible(!isOnline);
    ui->activationKeyLabel->setVisible(!isOnline);
    ui->activationKeyTextEdit->setVisible(!isOnline);
    ui->activationKeyFileLabel->setVisible(!isOnline);
    ui->licenseFileLabel->setVisible(!isOnline);
    ui->browseLicenseFileButton->setVisible(!isOnline);

    updateControls();
}

void QnLicenseWidget::at_browseLicenseFileButton_clicked() {
    QString fileName = QnFileDialog::getOpenFileName(this,
                                                    tr("Open License File"),
                                                    QString(),
                                                    tr("All files (*.*)"),
                                                    0,
                                                    QnCustomFileDialog::fileDialogOptions());
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->activationKeyTextEdit->setPlainText(QLatin1String(file.readAll()));
        ui->licenseFileLabel->setText(file.fileName());
    }
}

void QnLicenseWidget::at_activateFreeLicenseButton_clicked() {
    ui->serialKeyEdit->setText(qnProductFeatures().freeLicenseKey);
    at_activateLicenseButton_clicked();
}

void QnLicenseWidget::at_activateLicenseButton_clicked() {
    setState(Waiting);
}



