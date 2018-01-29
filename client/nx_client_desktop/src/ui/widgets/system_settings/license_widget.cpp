#include "license_widget.h"
#include "ui_license_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <common/common_module.h>

#include <licensing/license.h>

#include <ui/common/palette.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/style/custom_style.h>

#include <utils/email/email.h>

#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#include <nx/client/desktop/ui/common/clipboard_button.h>

using namespace nx::client::desktop::ui;

namespace {
bool isValidSerialKey(const QString &key)
{
    return key.length() == QnAppInfo::freeLicenseKey().length() && !key.contains(QLatin1Char(' '));
}

} // anonymous namespace


QnLicenseWidget::QnLicenseWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::LicenseWidget),
    m_state(Normal),
    m_freeLicenseAvailable(true)
{
    ui->setupUi(this);

    setTabShape(ui->tabWidget->tabBar(), style::TabShape::Compact);

    /* Workaround for initially hidden tab providing incorrect size hint: */
    autoResizePagesToContents(ui->tabWidget, { QSizePolicy::Expanding, QSizePolicy::Preferred }, false);

    /* Choose monospace font for license input line: */
    setMonospaceFont(ui->onlineKeyEdit);

    /* Upon taking focus by license input line set cursor to first empty position: */
    installEventHandler(ui->onlineKeyEdit, QEvent::FocusIn, this,
        [this]()
        {
            auto adjustCursorPosition =
                [this]()
                {
                    auto pos = ui->onlineKeyEdit->displayText().indexOf(L'_');
                    if (pos != -1)
                        ui->onlineKeyEdit->setCursorPosition(pos);
                };

            executeDelayedParented(adjustCursorPosition, 0, ui->onlineKeyEdit);
        });

    ui->onlineKeyEdit->setFocus();
    ui->activateFreeLicenseButton->setText(QnAppInfo::freeLicenseIsTrial()
        ? tr("Activate Trial License")
        : tr("Activate Free License"));

    setPaletteColor(ui->manualActivationTextWidget, QPalette::WindowText,
        ui->manualActivationTextWidget->palette().color(QPalette::Light));

    QnEmailAddress licensingEmail(QnAppInfo::licensingEmailAddress());
    if (licensingEmail.isValid())
    {
        const QString emailLink = lit("<a href=\"mailto:%1\">%1</a>").arg(licensingEmail.value());
        ui->manualActivationTextWidget->setText(
            tr("Please send email with License Key and Hardware Id provided to %1 to obtain an Activation Key file.")
            .arg(emailLink));
    }
    else
    {
        ui->manualActivationTextWidget->setText(
            tr("Please send License Key and Hardware Id provided to %1 to obtain an Activation Key file.")
            .arg(QnAppInfo::licensingEmailAddress()));
    }

    ui->manualActivationTextWidget->setOpenExternalLinks(true);

    setWarningStyle(ui->licenseKeyWarningLabel);
    ui->licenseKeyWarningLabel->setVisible(false);

    connect(ui->onlineKeyEdit, &QLineEdit::textChanged, this, &QnLicenseWidget::updateControls);
    connect(ui->browseLicenseFileButton, &QPushButton::clicked,
        this, &QnLicenseWidget::at_browseLicenseFileButton_clicked);

    setAccentStyle(ui->activateLicenseButton);
    connect(ui->activateLicenseButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->activateLicenseButton->showIndicator();
            setState(Waiting);
        });

    setAccentStyle(ui->activateLicenseButtonCopy);
    connect(ui->activateLicenseButtonCopy, &QPushButton::clicked, this,
        [this]()
        {
            ui->activateLicenseButtonCopy->showIndicator();
            setState(Waiting);
        });

    connect(ui->activateFreeLicenseButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->activateFreeLicenseButton->showIndicator();
            setSerialKey(QnAppInfo::freeLicenseKey());
            setState(Waiting);
        });

    ClipboardButton::createInline(ui->hardwareIdEdit, ClipboardButton::StandardType::copy);
    ClipboardButton::createInline(ui->onlineKeyEdit, ClipboardButton::StandardType::paste);

    updateControls();
}

QnLicenseWidget::~QnLicenseWidget()
{
    return;
}

QnLicenseWidget::State QnLicenseWidget::state() const
{
    return m_state;
}

void QnLicenseWidget::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;

    updateControls();

    emit stateChanged();
}

bool QnLicenseWidget::isOnline() const
{
    return ui->tabWidget->currentWidget() == ui->onlinePage;
}

void QnLicenseWidget::setOnline(bool online)
{
    ui->tabWidget->setCurrentWidget(online ? ui->onlinePage : ui->manualPage);
}

bool QnLicenseWidget::isFreeLicenseAvailable() const
{
    return m_freeLicenseAvailable;
}

void QnLicenseWidget::setFreeLicenseAvailable(bool available)
{
    if (m_freeLicenseAvailable == available)
        return;

    m_freeLicenseAvailable = available;
    ui->activateFreeLicenseButton->setVisible(m_freeLicenseAvailable);
}

void QnLicenseWidget::setHardwareId(const QString& hardwareId)
{
    ui->hardwareIdEdit->setText(hardwareId);
}

QString QnLicenseWidget::serialKey() const
{
    return ui->onlineKeyEdit->text();
}

void QnLicenseWidget::setSerialKey(const QString &serialKey)
{
    ui->onlineKeyEdit->setText(serialKey);
}

QByteArray QnLicenseWidget::activationKey() const
{
    return m_activationKey;
}

void QnLicenseWidget::updateControls()
{
    bool normalState = m_state == Normal;
    ui->tabWidget->setEnabled(normalState);

    if (normalState)
    {
        bool canActivate = isOnline()
            ? isValidSerialKey(serialKey())
            : !m_activationKey.isEmpty();

        ui->activateLicenseButton->setEnabled(canActivate);
        ui->activateLicenseButtonCopy->setEnabled(canActivate);

        ui->activateLicenseButton->hideIndicator();
        ui->activateFreeLicenseButton->hideIndicator();
        ui->activateLicenseButtonCopy->hideIndicator();
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLicenseWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
}

void QnLicenseWidget::at_browseLicenseFileButton_clicked()
{
    QString fileName = QnFileDialog::getOpenFileName(this,
                                                     tr("Open License File"),
                                                     QString(),
                                                     tr("All files (*.*)"),
                                                     0,
                                                     QnCustomFileDialog::fileDialogOptions());
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QnMessageBox::critical(this, tr("Failed to open file"), fileName);
        return;
    }

    ui->fileLineEdit->setText(file.fileName());
    ui->fileLineEdit->setPalette(this->palette());

    QString source = QString::fromLatin1(file.readAll());
    file.close();

    QStringList lines = source.split(L'\n');
    QStringList filtered_lines;
    foreach(QString line, lines)
    {
        line = line.trimmed();
        if (!line.isEmpty())
        {
            filtered_lines.append(line);
        }
    }
    m_activationKey = filtered_lines.join(L'\n').toLatin1();
    if (m_activationKey.isEmpty())
    {
        setWarningStyle(ui->fileLineEdit);
    }

    updateControls();
}
