// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_management_dialogs.h"

#include <QtGui/QClipboard>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <licensing/license.h>
#include <nx/branding.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/licensing/customer_support.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/license/validator.h>
#include <nx_ec/ec_api_common.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

static const QString kEmptyLine = QString(html::kLineBreak) + html::kLineBreak;

QnLicensePtr findLicenseInList(const QByteArray& key, const QnLicenseList& licenses)
{
    for (const auto& license: licenses)
    {
        if (license->key() == key)
            return license;
    }
    return QnLicensePtr();
}

enum class CopyToClipboardButtonVisibility
{
    hidden,
    visible
};

class LicenseManagementMessagesPrivate
{
    Q_DECLARE_TR_FUNCTIONS(LicenseManagementMessagesPrivate)

public:
    static void addClipboardButton(
        QnMessageBox* messageBox,
        const QString& text,
        const QString& extras)
    {
        auto copyButton = new ClipboardButton(
            ClipboardButton::StandardType::copyLong,
            messageBox);
        messageBox->addButton(copyButton, QDialogButtonBox::HelpRole);
        QObject::connect(copyButton, &QPushButton::clicked, messageBox,
            [text, extras]
            {
                qApp->clipboard()->setText(text + "\n" + html::toPlainText(extras));
            });
    }

    static void showMessage(
        QWidget* parent,
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras = QString(),
        CopyToClipboardButtonVisibility button = CopyToClipboardButtonVisibility::hidden)
    {
        QnSessionAwareMessageBox messageBox(parent);
        messageBox.setIcon(icon);
        messageBox.setText(text);
        messageBox.setInformativeText(extras, /*split*/ false);
        messageBox.setInformativeTextFormat(Qt::RichText);

        if (button == CopyToClipboardButtonVisibility::visible)
            addClipboardButton(&messageBox, text, extras);

        messageBox.setStandardButtons(QDialogButtonBox::Ok);
        messageBox.setEscapeButton(QDialogButtonBox::Ok);
        messageBox.setDefaultButton(QDialogButtonBox::Ok);
        messageBox.exec();
    }

    /**
     * Create a support message, referring to regional support, mentioned in the provided licenses.
     * If no licenses are provided, all system licenses are used.
     */
    static QString customerSupportMessage(
        QWidget* parent,
        const QString& genericString,
        const QString& regionalSupportString,
        std::optional<QnLicenseList> limitToLicenses = std::nullopt)
    {
        NX_ASSERT(genericString.contains("%1"));
        auto context = dynamic_cast<QnWorkbenchContextAware*>(parent);
        if (!NX_ASSERT(context))
            return QString();

        QnLicenseList licenses = limitToLicenses
            ? *limitToLicenses
            : context->systemContext()->licensePool()->getLicenses();

        CustomerSupport customerSupport(context->systemContext());
        const auto regionalContacts = customerSupport.regionalContactsForLicenses(licenses);

        if (regionalContacts.empty())
        {
            const ContactAddress licensingAddress = customerSupport.licensingContact.address;
            if (NX_ASSERT(licensingAddress.channel != ContactAddress::Channel::empty,
                "Customization is invalid!"))
            {
                QString supportLink = licensingAddress.href;

                // Http links must be on a separate string to avoid line break on "http://".
                if (licensingAddress.channel == ContactAddress::Channel::link)
                   supportLink = html::kLineBreak + supportLink;

                return genericString.arg(supportLink);
            }

            // Backup string, this must never happen.
            return genericString.arg("your Regional / License support");
        }

        QStringList regionalSupport;
        regionalSupport.push_back(regionalSupportString);
        for (const CustomerSupport::Contact& contact: regionalContacts)
            regionalSupport.push_back(contact.company + html::kLineBreak + contact.address.href);
        return regionalSupport.join(kEmptyLine);

    }

    static QString getValidLicenseKeyMessage(QWidget* parent)
    {
        return customerSupportMessage(
            parent,
            tr("To get a valid License Key please contact %1."),
            tr("To get a valid License Key please contact your Regional / License support:"));
    }

    static QString whatIfProblemPersistsMessage(
        QWidget* parent,
        std::optional<QnLicenseList> licenses = std::nullopt)
    {
        return customerSupportMessage(
            parent,
            tr("If the problem persists, please contact %1."),
            tr("If the problem persists, please contact your Regional / License support:"),
            licenses);
    }

    static QStringList licenseHtmlDescription(const QnLicensePtr& license)
    {
        static const auto kLightTextColor = core::colorTheme()->color("light10");

        QStringList result;

        const QString licenseKey = html::monospace(
            html::colored(QString::fromLatin1(license->key()), kLightTextColor));

        const QString channelsCount = tr("%n channels.", "", license->cameraCount());

        result.push_back(licenseKey);
        result.push_back(license->displayName() + ", " + channelsCount);
        return result;
    }

    static QString deactivationErrorMessage(
        QWidget* parent,
        const QnLicenseList& licenses,
        const license::DeactivationErrors& errors)
    {
        static const QString kMessageDelimiter = kEmptyLine + "&mdash;&mdash;" + kEmptyLine;

        QStringList result;
        QnLicenseList licensesWithErrors;
        for (auto it = errors.begin(); it != errors.end(); ++it)
        {
            const QnLicensePtr license = findLicenseInList(it.key(), licenses);
            if (!NX_ASSERT(license))
                continue;

            QStringList licenseBlock = licenseHtmlDescription(license);
            const QString error = setWarningStyleHtml(
                license::Deactivator::errorDescription(it.value()));
            licenseBlock.push_back(QString()); //< Additional spacer.
            licenseBlock.push_back(error);

            result.push_back(licenseBlock.join(html::kLineBreak));
            licensesWithErrors.push_back(license);
        }

        const auto contactCustomerSupportText = customerSupportMessage(
            parent,
            tr("Please contact %1."),
            tr("Please contact your Regional / License support:"),
            licensesWithErrors);

        const QString licensesBlock = result.join(kMessageDelimiter);
        const QString contactsDelimiter = (result.size() == 1) ? kEmptyLine : kMessageDelimiter;
        return licensesBlock + contactsDelimiter + contactCustomerSupportText;
    }
};

} // namespace

using Private = LicenseManagementMessagesPrivate;

// -------------------------------------------------------------------------------------------------
// License activation messages

void LicenseActivationDialogs::licenseIsIncompatible(QWidget* parent)
{
    const QString supportContactString = Private::getValidLicenseKeyMessage(parent);

    Private::showMessage(
        parent,
        QnMessageBoxIcon::Warning,
        tr("Incompatible license"),
        tr("License you are trying to activate is incompatible with your software.")
            + kEmptyLine
            + supportContactString
    );
}

void LicenseActivationDialogs::invalidDataReceived(QWidget* parent)
{
    const QString supportContactString = Private::customerSupportMessage(
        parent,
        tr("To report the issue please contact %1."),
        tr("To report the issue please contact your Regional / License support:"));

    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Failed to activate license"),
        tr("Invalid data received.")
            + kEmptyLine
            + supportContactString
    );
}

void LicenseActivationDialogs::databaseErrorOccurred(QWidget* parent)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Failed to activate license"),
        tr("Database error occurred.")
    );
}

void LicenseActivationDialogs::invalidLicenseKey(QWidget* parent)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Warning,
        tr("Invalid License Key"),
        tr("Please make sure it is entered correctly.")
            + kEmptyLine
            + Private::whatIfProblemPersistsMessage(parent)
    );
}

void LicenseActivationDialogs::invalidKeyFile(QWidget* parent)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Warning,
        tr("Invalid activation key file"),
        tr("Select a valid activation key file to continue.")
            + kEmptyLine
            + Private::whatIfProblemPersistsMessage(parent)
    );
}


void LicenseActivationDialogs::licenseAlreadyActivated(
    QWidget* parent,
    const QString& hardwareId,
    const QString& time)
{
    const QString supportContactString = Private::getValidLicenseKeyMessage(parent);

    const QString text = time.isEmpty()
        ? tr("This license is already activated and linked to Hardware ID %1")
            .arg(hardwareId)
        : tr("This license is already activated and linked to Hardware ID %1 on %2")
            .arg(hardwareId, time);

    Private::showMessage(
        parent,
        QnMessageBoxIcon::Warning,
        tr("License already activated on another server"),
        text
            + kEmptyLine
            + supportContactString,
        CopyToClipboardButtonVisibility::visible
    );
}

void LicenseActivationDialogs::licenseAlreadyActivatedHere(QWidget* parent)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Information,
        tr("License has already been activated")
    );
}

void LicenseActivationDialogs::activationError(
    QWidget* parent,
    nx::vms::license::QnLicenseErrorCode errorCode,
    Qn::LicenseType licenseType)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Failed to activate license"),
        nx::vms::license::Validator::errorMessage(errorCode, licenseType)
    );
}

void LicenseActivationDialogs::networkError(QWidget* parent)
{
    const QString supportContactString = Private::customerSupportMessage(
        parent,
        tr("To activate License Key manually please contact %1."),
        tr("To activate License Key manually please contact your Regional / License support:"));

    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Network error"),
        supportContactString
    );
}

void LicenseActivationDialogs::freeLicenseNetworkError(QWidget* parent, const QString& hardwareId)
{
    const QString supportContactString = Private::customerSupportMessage(
        parent,
        tr("If your System does not have access to the Internet, please send the provided License "
           "Key and Hardware ID to %1 "
            "to receive the activation file."),
        tr("If your System does not have access to the Internet, please send the provided License "
           "Key and Hardware ID to your Regional / License support team "
            "to receive the activation file:")) +
        QString(html::kLineBreak);

    QString licenseInfoString;
    static const auto kLightTextColor = core::colorTheme()->color("light10");

    html::Tag tagTable("table", licenseInfoString);
    {
        {
            html::Tag tagRow("tr", licenseInfoString);
            {
                html::Tag tagCell("td", licenseInfoString);
                licenseInfoString.append(tr("Hardware ID"));
            }
            {
                html::Tag tagCell("td", licenseInfoString);
                licenseInfoString.append("&nbsp;");
            }
            {
                html::Tag tagCell("td", licenseInfoString);
                licenseInfoString.append(html::colored(hardwareId, kLightTextColor));
            }
        }

        {
            html::Tag tagRow("tr", licenseInfoString);
            {
                html::Tag tagCell("td", licenseInfoString);
                licenseInfoString.append(tr("License Key"));
            }
            {
                html::Tag tagCell("td", licenseInfoString);
                licenseInfoString.append("&nbsp;");
            }
            {
                html::Tag tagCell("td", licenseInfoString);
                licenseInfoString.append(
                    html::colored(nx::branding::freeLicenseKey(), kLightTextColor));
            }
        }
    }

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Critical);
    messageBox.setText(tr("Failed to activate free license"));
    messageBox.setInformativeText(supportContactString + licenseInfoString, /*split*/ false);
    messageBox.setInformativeTextFormat(Qt::RichText);

    auto copyButton = new ClipboardButton(tr("Copy Parameters"), tr("Copied"), &messageBox);
    messageBox.addButton(copyButton, QDialogButtonBox::HelpRole);
    QObject::connect(copyButton, &QPushButton::clicked,
        [hardwareId]
        {
            qApp->clipboard()->setText(tr("Hardware ID: %1\nLicense Key: %2")
                .arg(hardwareId, nx::branding::freeLicenseKey()));
        });

    messageBox.setStandardButtons(QDialogButtonBox::Ok);
    messageBox.setEscapeButton(QDialogButtonBox::Ok);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setSafeMinimumWidth(425);

    messageBox.exec();
}

void LicenseActivationDialogs::failure(QWidget* parent, const QString& description)
{
    const QString supportContactString = Private::getValidLicenseKeyMessage(parent);

    Private::showMessage(
        parent,
        QnMessageBoxIcon::Warning,
        tr("Failed to activate license"),
        description
    );
}

void LicenseActivationDialogs::success(QWidget* parent)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Success,
        tr("License activated")
    );
}

// -------------------------------------------------------------------------------------------------
// License deactivation messages

void LicenseDeactivationDialogs::deactivationError(
    QWidget* parent,
    const QnLicenseList& licenses,
    const license::DeactivationErrors& errors)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Failed to deactivate %n licenses", "", errors.size()),
        Private::deactivationErrorMessage(parent, licenses, errors),
        CopyToClipboardButtonVisibility::visible
    );
}

bool LicenseDeactivationDialogs::partialDeactivationError(
    QWidget* parent,
    const QnLicenseList& licenses,
    const license::DeactivationErrors& errors)
{
    const QString text = tr("%1 of %n licenses cannot be deactivated", "", licenses.size())
        .arg(errors.size());

    const QString extras = Private::deactivationErrorMessage(parent, licenses, errors);

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Information);
    messageBox.setText(text);
    messageBox.setInformativeText(extras, /*split*/ false);
    messageBox.setInformativeTextFormat(Qt::RichText);
    Private::addClipboardButton(&messageBox, text, extras);
    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.setEscapeButton(QDialogButtonBox::Cancel);

    QPushButton* deactivateButton = messageBox.addButton(
        tr("Deactivate %n Other", "", licenses.size() - errors.size()),
        QDialogButtonBox::YesRole,
        Qn::ButtonAccent::Warning);
    messageBox.setDefaultButton(deactivateButton);

    // "Deactivate Other" will return 1 as its index in the custom buttons. Not good enough to rely
    // on this.
    return messageBox.exec() != QDialogButtonBox::Cancel;
}

void LicenseDeactivationDialogs::unexpectedError(
    QWidget* parent,
    const QnLicenseList& licenses)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Cannot deactivate licenses", "", licenses.count()),
        tr("Please try again later.")
    );
}

void LicenseDeactivationDialogs::networkError(QWidget* parent)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Information,
        tr("Cannot connect to the License Server"),
        tr("Please make sure your server has active Internet connection or check firewall settings.")
    );
}

void LicenseDeactivationDialogs::licensesServerError(QWidget* parent, const QnLicenseList& licenses)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("License Server error"),
        Private::whatIfProblemPersistsMessage(parent, licenses)
    );
}

void LicenseDeactivationDialogs::failedToRemoveLicense(QWidget* parent, ec2::ErrorCode errorCode)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Critical,
        tr("Failed to remove license from Server"),
        ec2::toString(errorCode)
    );
}

void LicenseDeactivationDialogs::success(
    QWidget* parent,
    const QnLicenseList& licenses)
{
    Private::showMessage(
        parent,
        QnMessageBoxIcon::Success,
        tr("%n licenses deactivated", "", licenses.count())
    );
}

} // namespace nx::vms::client::desktop
