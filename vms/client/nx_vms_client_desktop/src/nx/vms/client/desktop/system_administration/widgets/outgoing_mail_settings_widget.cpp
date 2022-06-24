// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "outgoing_mail_settings_widget.h"
#include "ui_outgoing_mail_settings_widget.h"

#include <chrono>
#include <utility>

#include <QtCore/QHash>
#include <QtWidgets/QMenu>

#include <api/model/test_email_settings_reply.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/input_field.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/email/email.h>

namespace {

using namespace std::chrono;

static constexpr auto kStatusLabelPixelSize = 18;
static constexpr auto kStatueLabelFontWeight = QFont::Medium;

static constexpr int kStatusHintLabelPixelSize = 12;
static constexpr int kStatusHintLabelFontWeight = QFont::Normal;

static constexpr auto kSmtpTestingTimeout = 5s;

const bool isValidPort(int port) { return port >=1 && port <= 65535; };

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// OutgoingMailSettingsWidget::Private declaration.

class OutgoingMailSettingsWidget::Private
{
    Q_DECLARE_TR_FUNCTIONS(OutgoingMailSettingsWidget::Private)

public:
    Private(OutgoingMailSettingsWidget* owner);

    void setupDialogControls();
    void setShowFullSmtpParams(bool show);
    void setReadOnly(bool readOnly);

    enum ConfigurationStatus
    {
        Active,
        NotConfigured,
        Error,
    };
    void setConfigurationStatus(ConfigurationStatus status);
    void setConfigurationStatusHint(const QString& statusHint);

    QMenu* effectiveServiceTypeDropdownMenu() const;
    static QString serviceTypeDropdownText(bool useCloudService);

    void updateConfigurationStatus();
    void updateSmtpConfigurationStatus();
    void testSmtpConfiguration();
    void updateCloudServiceStatus();
    static QString smtpErrorCodeToString(nx::email::SmtpError errorCode);

    void setSmtpSettingsToDialog(const QnEmailSettings& smtpSettings);
    void setUseCloudServiceToDialog(bool useCloudService);

    QnEmailSettings getSmtpSettingsFromDialog() const;
    bool getUseCloudServiceFromDialog() const;

    bool ignoreInputNotifications() const;

private:
    OutgoingMailSettingsWidget* const q;

    std::unique_ptr<Ui::OutgoingMailSettingsWidget> ui;
    std::unique_ptr<QMenu> m_serviceTypeDropdownMenu;
    QPushButton* m_testSmtpConfigurationButton = nullptr;

    bool m_ignoreInputNotifications = false;
    bool m_useCloudServiceToSendEmail = false;
};

//-------------------------------------------------------------------------------------------------
// OutgoingMailSettingsWidget::Private definition.

OutgoingMailSettingsWidget::Private::Private(OutgoingMailSettingsWidget* owner):
    q(owner),
    ui(new Ui::OutgoingMailSettingsWidget()),
    m_serviceTypeDropdownMenu(new QMenu())
{
    ui->setupUi(q);
    m_testSmtpConfigurationButton = new QPushButton(ui->connectionSettingsGroupBox);
    setupDialogControls();
}

void OutgoingMailSettingsWidget::Private::setupDialogControls()
{
    // Setup layouts, margins and spacings.

    ui->formLayout->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);
    ui->formLayout->setSpacing(nx::style::Metrics::kDefaultTopLevelMargin);

    static constexpr auto kHeaderLayoutTopMargin = 10;
    static constexpr auto kHeaderLayoutSpacing = 4;
    auto headerLayoutMargins = nx::style::Metrics::kDefaultTopLevelMargins;
    headerLayoutMargins.setTop(kHeaderLayoutTopMargin);
    ui->headerLayout->setContentsMargins(headerLayoutMargins);
    ui->headerLayout->setSpacing(kHeaderLayoutSpacing);

    static constexpr auto kStatusLayoutSpacing = 4;
    ui->statusPageLayout->setSpacing(kStatusLayoutSpacing);

    // Setup header appearance.

    static const auto kHeaderBackgroundColor = colorTheme()->color("dark8");
    setPaletteColor(ui->headerWidget, QPalette::Window, kHeaderBackgroundColor);
    ui->headerWidget->setAutoFillBackground(true);

    QFont statusLabelFont;
    statusLabelFont.setPixelSize(kStatusLabelPixelSize);
    statusLabelFont.setWeight(kStatueLabelFontWeight);
    ui->configurationStatusLabel->setFont(statusLabelFont);

    QFont statusHintFont;
    statusHintFont.setPixelSize(kStatusHintLabelPixelSize);
    statusHintFont.setWeight(kStatusHintLabelFontWeight);
    ui->configurationStatusHintLabel->setFont(statusHintFont);

    static const auto kStatusHintColor = colorTheme()->color("light10");
    setPaletteColor(ui->configurationStatusHintLabel, QPalette::WindowText, kStatusHintColor);

    // Fill connection type combo box options.

    ui->protocolComboBox->addItem(tr("TLS"), QVariant::fromValue(QnEmail::ConnectionType::tls));
    ui->protocolComboBox->addItem(tr("SSL"), QVariant::fromValue(QnEmail::ConnectionType::ssl));
    ui->protocolComboBox->addItem(tr("Unsecure"),
        QVariant::fromValue(QnEmail::ConnectionType::unsecure));

    // Setup "Check" button.

    m_testSmtpConfigurationButton->setText(tr("Check"));
    m_testSmtpConfigurationButton->setFlat(true);
    m_testSmtpConfigurationButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    m_testSmtpConfigurationButton->resize(m_testSmtpConfigurationButton->minimumSizeHint());
    static constexpr QMargins kCheckButtonMargins = {0, -4, 0, 0};
    anchorWidgetToParent(m_testSmtpConfigurationButton, Qt::TopEdge | Qt::RightEdge, kCheckButtonMargins);

    // Setup input validators.

    ui->emailInput->setValidator(
        [](const QString& text)
        {
            return nx::email::isValidAddress(text)
                ? ValidationResult::kValid
                : ValidationResult(tr("Email is not valid."));
        });

    ui->passwordInput->setValidator(defaultPasswordValidator(/*allowEmpty*/ true));

    ui->serverAddressInput->setValidator(
        [](const QString& text)
        {
            const auto url = nx::utils::Url::fromUserInput(text);
            return url.isValid()
                ? ValidationResult::kValid
                : ValidationResult(tr("URL is not valid."));
        });

    // Setup placeholders.

    ui->systemSingnatureInput->setPlaceholderText(tr("Enter a short System description here."));
    ui->supportSignatureInput->setPlaceholderText(nx::branding::supportAddress());

    // Setup service type dropdown menu.

    m_serviceTypeDropdownMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    m_serviceTypeDropdownMenu->addAction(serviceTypeDropdownText(/*useCloudService*/ true))
        ->setData(/*useCloudService*/ true);
    m_serviceTypeDropdownMenu->addAction(serviceTypeDropdownText(/*useCloudService*/ false))
        ->setData(/*useCloudService*/ false);

    // Workaround to keep widget layout neat without using Aligner, which doesn't fit there
    // really well.

    auto labels = q->findChildren<QLabel*>();
    for (const auto label: labels)
    {
        if (label->buddy())
        {
            label->setFocusPolicy(Qt::ClickFocus);
            label->setFocusProxy(label->buddy());
            label->buddy()->setFocusPolicy(Qt::ClickFocus);
            label->setAlignment(Qt::AlignRight | Qt::AlignTop);
            const int labelTopMargin =
                std::round((label->buddy()->height() - label->minimumSizeHint().height()) / 2.0);
            label->setContentsMargins({0, labelTopMargin, 0, 0});
        }
    }

    // Setup interactions.

    const auto inputs = q->findChildren<InputField*>();
    for (const auto& input: inputs)
    {
        q->connect(input, &InputField::textChanged, q,
            [this]
            {
                if (!ignoreInputNotifications())
                    emit q->hasChangesChanged();
            });
    }

    q->connect(ui->protocolComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
        [this]
        {
            if (!ignoreInputNotifications())
                emit q->hasChangesChanged();
        });

    q->connect(m_serviceTypeDropdownMenu.get(), &QMenu::triggered, q,
        [this](QAction* action)
        {
            const auto useCloudServiceFromMenu = action->data().toBool();
            if (useCloudServiceFromMenu == getUseCloudServiceFromDialog())
                return;

            setUseCloudServiceToDialog(useCloudServiceFromMenu);
            updateConfigurationStatus();

            emit q->hasChangesChanged();
        });

    q->connect(ui->emailInput, &InputField::editingFinished, q,
        [this]
        {
            const auto emailAddress = QnEmailAddress(ui->emailInput->text());
            const auto preset = emailAddress.smtpServer();
            if (preset.isNull())
            {
                setShowFullSmtpParams(true);
                return;
            }

            if (QnEmailSettings::defaultPort(preset.connectionType) != preset.port
                && isValidPort(preset.port))
            {
                auto serverUrl = nx::utils::Url::fromUserInput(preset.server);
                serverUrl.setPort(preset.port);
                ui->serverAddressInput->setText(serverUrl.displayAddress());
            }
            else
            {
                ui->serverAddressInput->setText(preset.server);
            }

            ui->userInput->setText(emailAddress.user());

            ui->protocolComboBox->setCurrentIndex(
            ui->protocolComboBox->findData(QVariant::fromValue(preset.connectionType)));

            setShowFullSmtpParams(false);
        });

    q->connect(m_testSmtpConfigurationButton, &QPushButton::clicked, q,
        [this] { testSmtpConfiguration(); });

    q->connect(q->systemSettings(), &nx::vms::common::SystemSettings::cloudSettingsChanged, q,
        [this] { ui->serviceTypeDropdown->setMenu(effectiveServiceTypeDropdownMenu()); });
}

void OutgoingMailSettingsWidget::Private::setShowFullSmtpParams(bool show)
{
    ui->userLabel->setHidden(!show);
    ui->userInput->setHidden(!show);

    ui->serverAddressLabel->setHidden(!show);
    ui->serverAddressInput->setHidden(!show);

    ui->protocolLabel->setHidden(!show);
    ui->protocolComboBox->setHidden(!show);
}

void OutgoingMailSettingsWidget::Private::setReadOnly(bool readOnly)
{
    const auto inputs = q->findChildren<InputField*>();
    for (const auto input: inputs)
        input->setReadOnly(readOnly);

    ui->protocolComboBox->setEnabled(!readOnly);
    m_testSmtpConfigurationButton->setEnabled(!readOnly);

    ui->serviceTypeDropdown->setMenu(!readOnly
        ? effectiveServiceTypeDropdownMenu()
        : nullptr);
}

void OutgoingMailSettingsWidget::Private::setConfigurationStatus(ConfigurationStatus status)
{
    using TextAndColorCode = std::pair<QString, QString>;

    static QHash<ConfigurationStatus, TextAndColorCode> kStatusRepresentation = {
        {ConfigurationStatus::Active, {tr("Active"), "light4"}},
        {ConfigurationStatus::NotConfigured, {tr("Not configured"), "yellow_d1"}},
        {ConfigurationStatus::Error, {tr("Error"), "red_l2"}}};

    const auto [text, code] = kStatusRepresentation.value(status);
    ui->configurationStatusLabel->setText(text);
    setPaletteColor(ui->configurationStatusLabel, QPalette::WindowText, colorTheme()->color(code));
}

void OutgoingMailSettingsWidget::Private::setConfigurationStatusHint(const QString& statusHint)
{
    ui->configurationStatusHintLabel->setText(statusHint);
}

void OutgoingMailSettingsWidget::Private::updateConfigurationStatus()
{
    if (getUseCloudServiceFromDialog())
        updateCloudServiceStatus();
    else
        updateSmtpConfigurationStatus();
}

void OutgoingMailSettingsWidget::Private::updateSmtpConfigurationStatus()
{
    if (!NX_ASSERT(!getUseCloudServiceFromDialog()))
        return;

    if (!getSmtpSettingsFromDialog().isValid())
    {
        setConfigurationStatus(NotConfigured);
        setConfigurationStatusHint(tr("Set your email address or SMTP server"));
        return;
    }

    testSmtpConfiguration();
}

void OutgoingMailSettingsWidget::Private::testSmtpConfiguration()
{
    std::optional<QnUuid> proxyServerId;
    if (!q->currentServer()->hasInternetAccess())
    {
        if (auto serverWithInternetAccess = q->resourcePool()->serverWithInternetAccess())
        {
            proxyServerId = serverWithInternetAccess->getId();
        }
        else
        {
            setConfigurationStatus(Error);
            setConfigurationStatusHint(tr("Unable to test email settings due to no internet"
                "connection on any of the active servers"));
            return;
        }
    }

    const bool testRemoteSettings = !q->hasChanges();

    using EmailSettingsTestResult = rest::RestResultWithData<QnTestEmailSettingsReply>;
    auto callback = nx::utils::guarded(q,
        [this, testRemoteSettings]
        (bool success, int handle, const EmailSettingsTestResult& result)
        {
            ui->stackedWidget->setCurrentWidget(ui->statusPage);
            m_testSmtpConfigurationButton->setEnabled(true);

            if (success)
            {
                if (result.data.errorCode == email::SmtpError::success)
                {
                    setConfigurationStatus(Active);
                    setConfigurationStatusHint(testRemoteSettings
                        ? "Users are receiving emails"
                        : "Users will start receiving emails right after you apply settings");
                }
                else
                {
                    setConfigurationStatus(Error);
                    setConfigurationStatusHint(smtpErrorCodeToString(result.data.errorCode));
                }
            }
            else
            {
                setConfigurationStatus(Error);
                setConfigurationStatusHint("Unable to test SMTP server");
            }
        });

    if (testRemoteSettings)
    {
        q->connectedServerApi()->testEmailSettings(callback, q->thread(), proxyServerId);
    }
    else
    {
        auto settingsForTesting = getSmtpSettingsFromDialog();
        settingsForTesting.timeout = kSmtpTestingTimeout.count();

        if (!settingsForTesting.isValid())
        {
            setConfigurationStatus(NotConfigured);
            setConfigurationStatusHint("Additional info required");
            return;
        }

        ui->stackedWidget->setCurrentWidget(ui->progressIndicatorPage);
        m_testSmtpConfigurationButton->setEnabled(false);
        q->connectedServerApi()->testEmailSettings(
            settingsForTesting, callback, q->thread(), proxyServerId);
    }
}

void OutgoingMailSettingsWidget::Private::updateCloudServiceStatus()
{
    if (!NX_ASSERT(getUseCloudServiceFromDialog()))
        return;

    const auto connectedToCloud = !q->systemSettings()->cloudSystemId().isNull();
    if (connectedToCloud)
    {
        setConfigurationStatus(Active);

        //: %1 will be substituted with short, non-branded cloud service name e.g. "Cloud".
        const auto activeCloudOptionText = tr("%1 users are receiving emails")
            .arg(nx::branding::shortCloudName());

        //: %1 will be substituted with short, non-branded cloud service name e.g. "Cloud".
        const auto pendingCloudOptionText = tr("%1 users will start receiving emails right after "
            "you apply settings").arg(nx::branding::shortCloudName());

        setConfigurationStatusHint(q->systemSettings()->useCloudServiceToSendEmail()
            ? activeCloudOptionText
            : pendingCloudOptionText);
    }
    else
    {
        setConfigurationStatus(Error);

        //: %1 will be substituted with branded cloud service name e.g. "Nx Cloud".
        setConfigurationStatusHint(tr("%1 is not available").arg(nx::branding::cloudName()));
    }
}

void OutgoingMailSettingsWidget::Private::setSmtpSettingsToDialog(
    const QnEmailSettings& smtpSettings)
{
    QnScopedValueRollback<bool> inputNotificationsGuard(&m_ignoreInputNotifications, true);

    QnEmailAddress emailAddress(smtpSettings.email);
    const auto preset = emailAddress.smtpServer();

    ui->emailInput->setText(smtpSettings.email);
    ui->userInput->setText(smtpSettings.user);
    ui->passwordInput->setText(smtpSettings.password);
    ui->protocolComboBox->setCurrentIndex(
        ui->protocolComboBox->findData(QVariant::fromValue(smtpSettings.connectionType)));
    ui->serverAddressInput->setText(smtpSettings.server);

    if (isValidPort(smtpSettings.port)
        && smtpSettings.port != QnEmailSettings::defaultPort((smtpSettings.connectionType)))
    {
        auto url = nx::utils::Url::fromUserInput(smtpSettings.server);
        url.setPort(smtpSettings.port);
        ui->serverAddressInput->setText(url.displayAddress());
    }

    ui->systemSingnatureInput->setText(smtpSettings.signature);
    ui->supportSignatureInput->setText(smtpSettings.supportEmail);

    bool showAdvancedControls = preset.isNull()
        || preset.server != smtpSettings.server
        || preset.port != smtpSettings.port
        || preset.connectionType != smtpSettings.connectionType;

    setShowFullSmtpParams(showAdvancedControls);
}

void OutgoingMailSettingsWidget::Private::setUseCloudServiceToDialog(bool useCloudService)
{
    m_useCloudServiceToSendEmail = useCloudService;

    ui->connectionSettingsGroupBox->setHidden(m_useCloudServiceToSendEmail);
    ui->serviceTypeDropdown->setText(serviceTypeDropdownText(m_useCloudServiceToSendEmail));
    ui->serviceTypeDropdown->setMenu(!q->isReadOnly() ?
        effectiveServiceTypeDropdownMenu():
        nullptr);
}

QnEmailSettings OutgoingMailSettingsWidget::Private::getSmtpSettingsFromDialog() const
{
    QnEmailSettings result;

    const QnEmailAddress emailAddress(ui->emailInput->text());
    const auto serverPreset = emailAddress.smtpServer();

    result.email = emailAddress.value();
    if (!serverPreset.isNull())
    {
        result.server = serverPreset.server;
        result.port = serverPreset.port;
        result.connectionType = serverPreset.connectionType;
    }
    else
    {
        result.connectionType = ui->protocolComboBox->currentData().value<QnEmail::ConnectionType>();
        const auto url = nx::utils::Url::fromUserInput(ui->serverAddressInput->text().trimmed());
        result.port = url.port(QnEmailSettings::defaultPort(result.connectionType));
        result.server = url.port() > 0 ? url.host() : ui->serverAddressInput->text().trimmed();
    }

    result.user = ui->userInput->text();
    result.password = ui->passwordInput->text();
    result.signature = ui->systemSingnatureInput->text();
    result.supportEmail = ui->supportSignatureInput->text();
    result.timeout = QnEmailSettings::defaultTimeoutSec();

    return result;
}

bool OutgoingMailSettingsWidget::Private::getUseCloudServiceFromDialog() const
{
    return m_useCloudServiceToSendEmail;
}

bool OutgoingMailSettingsWidget::Private::ignoreInputNotifications() const
{
    return m_ignoreInputNotifications;
}

QMenu* OutgoingMailSettingsWidget::Private::effectiveServiceTypeDropdownMenu() const
{
    const auto connectedToCloud = !q->systemSettings()->cloudSystemId().isNull();

    // Menu will be shown anyway if "Cloud" option was set somehow.
    const auto showMenu =
        (connectedToCloud && ini().allowConfigureCloudServiceToSendEmail)
        || getUseCloudServiceFromDialog();

    return showMenu
        ? m_serviceTypeDropdownMenu.get()
        : nullptr;
}

QString OutgoingMailSettingsWidget::Private::serviceTypeDropdownText(bool useCloudService)
{
    //: %1 will be substituted with branded cloud service name e.g. "Nx Cloud".
    const auto cloudOptionText = tr("Route via %1").arg(nx::branding::cloudName());
    const auto smtpOptionText = tr("Route via SMTP server");

    return useCloudService ? cloudOptionText : smtpOptionText;
}

QString OutgoingMailSettingsWidget::Private::smtpErrorCodeToString(nx::email::SmtpError errorCode)
{
    using namespace nx::email;
    switch (errorCode)
    {
        case SmtpError::success:
            return tr("Success");

        case SmtpError::responseTimeout:
        case SmtpError::sendDataTimeout:
        case SmtpError::connectionTimeout:
            return tr("Connection timed out");

        case SmtpError::authenticationFailed:
            return tr("Authentication failed");

        default:
            return tr("Unknown error");
    }
    return QString();
}

//-------------------------------------------------------------------------------------------------
// OutgoingMailSettingsWidget definition.

OutgoingMailSettingsWidget::OutgoingMailSettingsWidget(
    nx::vms::common::SystemContext* context,
    QWidget* parent)
    :
    base_type(parent),
    SystemContextAware(context),
    d(new Private(this))
{
}

OutgoingMailSettingsWidget::~OutgoingMailSettingsWidget()
{
}

void OutgoingMailSettingsWidget::loadDataToUi()
{
    d->setSmtpSettingsToDialog(systemSettings()->emailSettings());
    d->setUseCloudServiceToDialog(systemSettings()->useCloudServiceToSendEmail());
    d->updateConfigurationStatus();
}

void OutgoingMailSettingsWidget::applyChanges()
{
    if (!NX_ASSERT(hasChanges()))
        return;

    if (isReadOnly())
        return;

    systemSettings()->setEmailSettings(d->getSmtpSettingsFromDialog());
    systemSettings()->setUseCloudServiceToSendEmail(d->getUseCloudServiceFromDialog());
    systemSettings()->synchronizeNowSync();

    loadDataToUi();
}

bool OutgoingMailSettingsWidget::hasChanges() const
{
    return systemSettings()->useCloudServiceToSendEmail() != d->getUseCloudServiceFromDialog()
        || !systemSettings()->emailSettings().equals(d->getSmtpSettingsFromDialog());
}

void OutgoingMailSettingsWidget::setReadOnlyInternal(bool readOnly)
{
    d->setReadOnly(readOnly);
}

} // namespace nx::vms::client::desktop
