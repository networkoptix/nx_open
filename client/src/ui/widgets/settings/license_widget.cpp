#include "license_widget.h"
#include "ui_license_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtWidgets/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <utils/common/product_features.h>

#include <ui/common/palette.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/style/warning_style.h>

#include <licensing/license.h>

#include <utils/common/app_info.h>

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

    ui->onlineKeyEdit->setInputMask(QLatin1String(">NNNN-NNNN-NNNN-NNNN"));
    ui->manualKeyEdit->setInputMask(QLatin1String(">NNNN-NNNN-NNNN-NNNN"));
    ui->onlineKeyEdit->setFocus();
    ui->activateFreeLicenseButton->setText(qnProductFeatures().freeLicenseIsTrial ? tr("Activate Trial License") : tr("Activate Free License"));
    setWarningStyle(ui->fileLineEdit);

    ui->manualActivationTextWidget->resize(this->width(), ui->manualActivationTextWidget->height());
    ui->manualActivationTextWidget->setText(tr(
         "Please send email with the Serial Key and the Hardware ID provided to <a href=\"mailto:%1\">%1</a>. "
         "Then we'll send you an Activation Key file."
     ).arg(QLatin1String(QN_LICENSING_MAIL_ADDRESS))); // TODO: #Elric move to product features?

    connect(ui->onlineKeyEdit,              SIGNAL(textChanged(QString)),       this,   SLOT(updateControls()));
    connect(ui->manualKeyEdit,              SIGNAL(textChanged(QString)),       this,   SLOT(updateControls()));

    connect(ui->activationTypeComboBox,     SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_activationTypeComboBox_currentIndexChanged()));
    connect(ui->browseLicenseFileButton,    SIGNAL(clicked()),                  this,   SLOT(at_browseLicenseFileButton_clicked()));
    connect(ui->activateLicenseButton,      SIGNAL(clicked()),                  this,   SLOT(at_activateLicenseButton_clicked()));
    connect(ui->activateFreeLicenseButton,  SIGNAL(clicked()),                  this,   SLOT(at_activateFreeLicenseButton_clicked()));
    connect(ui->copyHwidButton, &QPushButton::clicked,  this, [this] {
        qApp->clipboard()->setText(ui->hardwareIdEdit->text());
        QMessageBox::information(this, tr("Success"), tr("Hardware ID copied to clipboard."));
    });

    updateControls();
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
    ui->copyHwidButton->setEnabled(!hardwareId.isEmpty());
}

QString QnLicenseWidget::serialKey() const {
    return isOnline() 
        ? ui->onlineKeyEdit->text()
        : ui->manualKeyEdit->text();
}

void QnLicenseWidget::setSerialKey(const QString &serialKey) {
    ui->onlineKeyEdit->setText(serialKey);
    ui->manualKeyEdit->setText(serialKey);
}

QByteArray QnLicenseWidget::activationKey() const {
    return m_activationKey;
}

void QnLicenseWidget::updateControls() {
    if(m_state == Normal) {
        ui->activateLicenseButton->setText(tr("Activate License"));

        bool canActivate = isValidSerialKey(serialKey());
        if (!isOnline())
            canActivate &= !m_activationKey.isEmpty();
        ui->activateLicenseButton->setEnabled(canActivate);
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

    if (isOnline)
        ui->onlineKeyEdit->setText(ui->manualKeyEdit->text());
    else
        ui->manualKeyEdit->setText(ui->onlineKeyEdit->text());

    ui->stackedWidget->setCurrentWidget(isOnline ? ui->onlinePage : ui->manualPage);

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
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not open the file %1").arg(fileName));
        return;
    }

    ui->fileLineEdit->setText(file.fileName());
    ui->fileLineEdit->setPalette(this->palette());

    QString source = QString::fromLatin1(file.readAll());
    file.close();

    QStringList lines = source.split(L'\n');
    QStringList filtered_lines;
    foreach(QString line, lines) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            filtered_lines.append(line);
        }
    }
    m_activationKey = filtered_lines.join(L'\n').toLatin1();
    if (m_activationKey.isEmpty()) {
        setWarningStyle(ui->fileLineEdit);
    }
}

void QnLicenseWidget::at_activateFreeLicenseButton_clicked() {
    setSerialKey(qnProductFeatures().freeLicenseKey);
    if (!isOnline() && m_activationKey.isEmpty()) {
        at_browseLicenseFileButton_clicked();
        if (m_activationKey.isEmpty())
            return;
    }
    at_activateLicenseButton_clicked();
}

void QnLicenseWidget::at_activateLicenseButton_clicked() {
    setState(Waiting);
}
