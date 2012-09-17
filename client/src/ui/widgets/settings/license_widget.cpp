#include "license_widget.h"
#include "ui_license_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "licensing/license.h"

namespace {
    bool isValidSerialKey(const QString &key) {
        return key.length() == QnLicense::FREE_LICENSE_KEY.length() && !key.contains(QLatin1Char(' '));
    }

} // anonymous namespace


LicenseWidget::LicenseWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::LicenseWidget),
    m_state(Normal)
{
    ui->setupUi(this);

    ui->serialKeyEdit->setInputMask(QLatin1String(">NNNN-NNNN-NNNN-NNNN"));
    ui->serialKeyEdit->setFocus();

    connect(ui->serialKeyEdit,              SIGNAL(textChanged(QString)),       this,   SLOT(updateControls()));
    connect(ui->activationTypeComboBox,     SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_activationTypeComboBox_currentIndexChanged()));
    connect(ui->browseLicenseFileButton,    SIGNAL(clicked()),                  this,   SLOT(at_browseLicenseFileButton_clicked()));
    connect(ui->activateLicenseButton,      SIGNAL(clicked()),                  this,   SLOT(at_activateLicenseButton_clicked()));
    connect(ui->activateFreeLicenseButton,  SIGNAL(clicked()),                  this,   SLOT(at_activateFreeLicenseButton_clicked()));

    at_activationTypeComboBox_currentIndexChanged();
}

LicenseWidget::~LicenseWidget() {
    return;
}

LicenseWidget::State LicenseWidget::state() const {
    return m_state;
}

void LicenseWidget::setState(State state) {
    if(m_state == state)
        return;

    m_state = state;

    updateControls();

    emit stateChanged();
}

bool LicenseWidget::isOnline() const {
    return ui->activationTypeComboBox->currentIndex() == 0;
}

void LicenseWidget::setOnline(bool online) {
    ui->activationTypeComboBox->setCurrentIndex(online ? 0 : 1);
}

bool LicenseWidget::isFreeLicenseAvailable() const {
    return ui->activateFreeLicenseButton->isVisible();
}

void LicenseWidget::setFreeLicenseAvailable(bool available) {
    ui->activateFreeLicenseButton->setVisible(available);
}

void LicenseWidget::setHardwareId(const QByteArray& hardwareId) {
    ui->hardwareIdEdit->setText(QLatin1String(hardwareId));
}

QString LicenseWidget::serialKey() const {
    return ui->serialKeyEdit->text();
}

void LicenseWidget::setSerialKey(const QString &serialKey) {
    ui->serialKeyEdit->setText(serialKey);
}

QByteArray LicenseWidget::activationKey() const {
    return ui->activationKeyTextEdit->toPlainText().toLatin1();
}

void LicenseWidget::updateControls() {
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
void LicenseWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);

    if(event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
}

void LicenseWidget::at_activationTypeComboBox_currentIndexChanged() {
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

void LicenseWidget::at_browseLicenseFileButton_clicked() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(this, tr("Open License file")));
    dialog->setNameFilters(QStringList() << tr("All files (*.*)"));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::ExistingFile);
    if (dialog->exec() && !dialog->selectedFiles().isEmpty()) {
        QFile file(dialog->selectedFiles().first());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ui->activationKeyTextEdit->setPlainText(QString::fromLatin1(file.readAll()));
            ui->licenseFileLabel->setText(file.fileName());
        }
    }
}

void LicenseWidget::at_activateFreeLicenseButton_clicked() {
    ui->serialKeyEdit->setText(QLatin1String(QnLicense::FREE_LICENSE_KEY));
    at_activateLicenseButton_clicked();
}

void LicenseWidget::at_activateLicenseButton_clicked() {
    setState(Waiting);
}



