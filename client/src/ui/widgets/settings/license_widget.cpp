#include "license_widget.h"
#include "ui_license_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <common/common_module.h>

#include <licensing/license.h>

#include <ui/common/palette.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/style/custom_style.h>

#include <utils/common/app_info.h>
#include <utils/common/product_features.h>

namespace {
    bool isValidSerialKey(const QString &key) {
        return key.length() == qnProductFeatures().freeLicenseKey.length() && !key.contains(QLatin1Char(' '));
    }

} // anonymous namespace


QnLicenseWidget::QnLicenseWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::LicenseWidget),
    m_state(Normal),
    m_freeLicenseAvailable(true)
{
    ui->setupUi(this);

    ui->onlineKeyEdit->setFocus();
    ui->activateFreeLicenseButton->setText(qnProductFeatures().freeLicenseIsTrial ? tr("Activate Trial License") : tr("Activate Free License"));
    setWarningStyle(ui->fileLineEdit);

    const QString emailAddress = lit("<a href=\"mailto:%1\">%1</a>").arg(QnAppInfo::licensingEmailAddress());

    ui->manualActivationTextWidget->resize(this->width(), ui->manualActivationTextWidget->height());
    ui->manualActivationTextWidget->setText(
        tr("Please send email with the Serial Key and the Hardware ID provided to %1 to obtain an Activation Key file.").arg(emailAddress)
        ); // TODO: #Elric move to product features?

    setWarningStyle(ui->licenseKeyWarningLabel);
    ui->licenseKeyWarningLabel->setVisible(false);

    connect(ui->onlineKeyEdit,              &QLineEdit::textChanged,        this,   &QnLicenseWidget::updateControls);
    connect(ui->tabWidget,                  &QTabWidget::currentChanged,    this,   &QnLicenseWidget::at_tabWidget_currentChanged);
    connect(ui->browseLicenseFileButton,    &QPushButton::clicked,          this,   &QnLicenseWidget::at_browseLicenseFileButton_clicked);

    connect(ui->activateLicenseButton,      &QPushButton::clicked,              this,   [this] {
        setState(Waiting);
    });

    connect(ui->activateFreeLicenseButton,  &QPushButton::clicked,              this,   [this] {
        setSerialKey(qnProductFeatures().freeLicenseKey);
        setState(Waiting);
    });

    connect(ui->copyHwidButton, &QPushButton::clicked,  this, [this] {
        qApp->clipboard()->setText(ui->hardwareIdEdit->text());
        QnMessageBox::information(this, tr("Success"), tr("Hardware ID copied to clipboard."));
    });

    connect(ui->pasteKeyButton, &QPushButton::clicked,  this, [this] {
        ui->onlineKeyEdit->setText(qApp->clipboard()->text());
    });

    connect(ui->onlineKeyEdit, &QLineEdit::textChanged, this, [this] (const QString &text) {
        const int minLength = 3;    // for the fixed input mask ">NNNN-NNNN-NNNN-NNNN" method ::text() returns at least "---"
        ui->licenseKeyWarningLabel->setVisible(text.length() > minLength 
            && text.length() != ui->onlineKeyEdit->maxLength());
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
    return ui->tabWidget->currentWidget() == ui->onlinePage;
}

void QnLicenseWidget::setOnline(bool online) {
    ui->tabWidget->setCurrentWidget(online ? ui->onlinePage : ui->manualPage);
}

bool QnLicenseWidget::isFreeLicenseAvailable() const {
    return m_freeLicenseAvailable;
}

void QnLicenseWidget::setFreeLicenseAvailable(bool available) {
    m_freeLicenseAvailable = available;
    ui->activateFreeLicenseButton->setVisible(m_freeLicenseAvailable && isOnline());
}

void QnLicenseWidget::setHardwareId(const QString& hardwareId) {
    ui->hardwareIdEdit->setText(hardwareId);
    ui->copyHwidButton->setEnabled(!hardwareId.isEmpty());
}

QString QnLicenseWidget::serialKey() const {
    return ui->onlineKeyEdit->text();
}

void QnLicenseWidget::setSerialKey(const QString &serialKey) {
    ui->onlineKeyEdit->setText(serialKey);
}

QByteArray QnLicenseWidget::activationKey() const {
    return m_activationKey;
}

void QnLicenseWidget::updateControls() {
    if(m_state == Normal) {
        ui->activateLicenseButton->setText(tr("Activate License"));
        bool canActivate = isOnline()
            ? isValidSerialKey(serialKey())
            : !m_activationKey.isEmpty();
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

void QnLicenseWidget::at_tabWidget_currentChanged() {
    bool isOnline = this->isOnline();

    ui->tabWidget->setCurrentWidget(isOnline ? ui->onlinePage : ui->manualPage);
    ui->activateFreeLicenseButton->setVisible(m_freeLicenseAvailable && isOnline);

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
        QnMessageBox::warning(this, tr("Error"), tr("Could not open the file %1").arg(fileName));
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

    updateControls();
}
