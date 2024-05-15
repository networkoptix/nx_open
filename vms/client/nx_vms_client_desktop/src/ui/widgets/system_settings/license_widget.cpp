// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_widget.h"
#include "ui_license_widget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <licensing/license.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/licensing/customer_support.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/email/email.h>

using namespace nx::vms::client::desktop;

namespace {

bool isValidSerialKey(const QString& key)
{
    return key.length() == nx::branding::freeLicenseKey().length()
        && !key.contains(QLatin1Char(' '));
}

} // namespace

QnLicenseWidget::QnLicenseWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LicenseWidget)
{
    ui->setupUi(this);

    setTabShape(ui->tabWidget->tabBar(), nx::style::TabShape::Compact);

    // Choose monospace font for license input line.
    setMonospaceFont(ui->onlineKeyEdit);

    // Upon taking focus by license input line set cursor to first empty position.
    installEventHandler(ui->onlineKeyEdit, QEvent::FocusIn, this,
        [this]()
        {
            auto adjustCursorPosition =
                [this]()
                {
                    auto pos = ui->onlineKeyEdit->displayText().indexOf('_');
                    if (pos != -1)
                        ui->onlineKeyEdit->setCursorPosition(pos);
                };

            executeLater(adjustCursorPosition, ui->onlineKeyEdit);
        });

    ui->onlineKeyEdit->setFocus();
    ui->activateFreeLicenseButton->setText(tr("Activate Trial License"));

    setPaletteColor(ui->manualActivationTextWidget, QPalette::WindowText,
        ui->manualActivationTextWidget->palette().color(QPalette::Light));

    ui->manualActivationTextWidget->setOpenExternalLinks(true);

    setWarningStyle(ui->licenseKeyWarningLabel);
    ui->licenseKeyWarningLabel->setVisible(false);

    connect(ui->onlineKeyEdit, &QLineEdit::textChanged, this, &QnLicenseWidget::updateControls);
    connect(ui->browseLicenseFileButton, &QPushButton::clicked,
        this, &QnLicenseWidget::browseForLicenseFile);

    setAccentStyle(ui->activateLicenseButton);
    connect(ui->activateLicenseButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->activateLicenseButton->showIndicator();
            emit licenseActivationRequested();
        });

    setAccentStyle(ui->activateLicenseManualButton);
    connect(ui->activateLicenseManualButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->activateLicenseManualButton->showIndicator();
            emit licenseActivationRequested();
        });

    connect(ui->activateFreeLicenseButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->activateFreeLicenseButton->showIndicator();
            setSerialKey(nx::branding::freeLicenseKey());
            emit licenseActivationRequested();
        });

    ClipboardButton::createInline(ui->hardwareIdEdit, ClipboardButton::StandardType::copy);
    ClipboardButton::createInline(ui->onlineKeyEdit, ClipboardButton::StandardType::paste);

    updateControls();

    // Regional support info is stored in the licenses pool.
    connect(systemContext()->licensePool(), &QnLicensePool::licensesChanged, this,
        &QnLicenseWidget::updateManualActivationLinkText);
}

QnLicenseWidget::~QnLicenseWidget()
{
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
}

void QnLicenseWidget::clearManualActivationUserInput()
{
    ui->fileLineEdit->clear();
    m_activationKey.clear();
    updateControls();
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

bool QnLicenseWidget::isFreeLicenseKey() const
{
    return serialKey() == nx::branding::freeLicenseKey();
}

void QnLicenseWidget::setHardwareId(const QString& hardwareId)
{
    ui->hardwareIdEdit->setText(hardwareId);
}

QString QnLicenseWidget::serialKey() const
{
    return ui->onlineKeyEdit->text();
}

void QnLicenseWidget::setSerialKey(const QString& serialKey)
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
        ui->activateLicenseManualButton->setEnabled(canActivate);

        ui->activateLicenseButton->hideIndicator();
        ui->activateFreeLicenseButton->hideIndicator();
        ui->activateLicenseManualButton->hideIndicator();
    }
    updateManualActivationLinkText();
}

QString QnLicenseWidget::calculateManualActivationLinkText() const
{
    CustomerSupport customerSupport(systemContext());

    if (customerSupport.regionalContacts.empty())
    {
        const ContactAddress licensingAddress = customerSupport.licensingContact.address;
        if (NX_ASSERT(licensingAddress.channel != ContactAddress::Channel::empty,
            "Customization is invalid!"))
        {
            // Http links must be displayed on a separate string to avoid line break on "http://".
            QString activationLink = licensingAddress.channel == ContactAddress::Channel::link
                ? nx::vms::common::html::kLineBreak + licensingAddress.href
                : licensingAddress.href;

            return tr("To obtain an Activation Key file please send the provided License Key "
                "and Hardware ID to %1.").arg(activationLink);
        }

        // Backup string, this must never happen.
        return tr("Please send the provided License Key and Hardware ID "
            "to your Regional / License support to obtain an Activation Key file.");
    }

    QStringList contactsList;
    for (const CustomerSupport::Contact& contact: customerSupport.regionalContacts)
        contactsList.push_back(contact.toString());

    return tr("Please send the provided License Key and Hardware ID "
        "to your Regional / License support (%1) to obtain an Activation Key file.",
        "%1 will be substituted by a list of contacts").arg(contactsList.join("; "));
}

void QnLicenseWidget::updateManualActivationLinkText()
{
    ui->manualActivationTextWidget->setText(calculateManualActivationLinkText());
}

void QnLicenseWidget::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
}

void QnLicenseWidget::keyPressEvent(QKeyEvent* event)
{
    const auto lineEdit = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    if (!lineEdit || !lineEdit->isEnabled() || lineEdit->isReadOnly() || !lineEdit->isVisible())
    {
        event->ignore();
        return;
    }

    switch (event->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        {
            event->accept(); //< Do not propagate Enter key up to the dialog.

            if (event->modifiers() && event->modifiers() != Qt::KeypadModifier)
                break;

            if (lineEdit == ui->onlineKeyEdit)
                ui->activateLicenseButton->click();

            break;
        }

        default:
            break;
    }
}

void QnLicenseWidget::browseForLicenseFile()
{
    std::vector<QnCustomFileDialog::FileFilter> filters;
    filters.push_back({tr("Text Files"), {"txt"}});
    filters.push_back(QnCustomFileDialog::kAllFilesFilter);

    const auto fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open License File"),
        QString(),
        QnCustomFileDialog::createFilter(filters),
        nullptr,
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

    const auto source = QString::fromLatin1(file.readAll());
    file.close();

    QStringList lines = source.split('\n');
    QStringList filteredLines;
    for (const auto& line: lines)
    {
        const auto trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty())
            filteredLines.append(line);
    }

    m_activationKey = filteredLines.join('\n').toLatin1();
    if (m_activationKey.isEmpty())
        setWarningStyle(ui->fileLineEdit);

    updateControls();
}
