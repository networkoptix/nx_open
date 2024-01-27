// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_dialog.h"
#include "ui_webpage_dialog.h"

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/text/time_strings.h>

using namespace nx::vms::api;
using namespace std::chrono;

namespace {

bool isValidUrl(const QUrl& url)
{
    if (!url.isValid())
        return false;

    return nx::network::http::isUrlScheme(url.scheme());
}

static constexpr auto kAnyDomain = "*";
static constexpr int kInvalidIndex = -1;

enum DurationUnitItemRole
{
    UnitSuffixRole = Qt::UserRole,
    UnitDurationRole,
};

} // namespace

namespace nx::vms::client::desktop {

QnWebpageDialog::QnWebpageDialog(QWidget* parent, EditMode editMode):
    base_type(parent),
    m_serversWatcher(parent),
    m_editMode(editMode),
    ui(new Ui::WebpageDialog)
{
    using namespace nx::vms::client::desktop;

    ui->setupUi(this);

    // This field is only visible in developers mode.
    ui->allowListField->setTitle("Domains");
    ui->allowListField->setPlaceholderText("*.example.org, cdn.domain.net");

    ui->nameInputField->setTitle(tr("Name"));

    ui->urlInputField->setTitle(tr("URL"));
    ui->urlInputField->setValidator(
        [](const QString& url)
        {
            if (url.trimmed().isEmpty())
                return ValidationResult(tr("URL cannot be empty."));

            return isValidUrl(QUrl::fromUserInput(url))
                ? ValidationResult::kValid
                : ValidationResult(tr("Wrong URL format."));
        });

    connect(ui->urlInputField, &InputField::textChanged, this,
        [this](const QString& text)
        {
            ui->nameInputField->setPlaceholderText(text.isEmpty()
                ? tr("Web Page")
                : QnWebPageResource::nameForUrl(url()));
        });

    ui->urlInputField->setPlaceholderText(lit("example.org"));

    ui->proxyAlertLabel->setText(tr(
        "Proxying all contents exposes any service or device on the server's network to the users"
        " of this webpage"));

    ui->clientApiAlertLabel->setText(tr("The web page can interact with the Desktop Client and"
        " request access to the user session"));

    ui->webPageBanner->init({
        .text = tr("To allow the use of a client API, create a web integration instead. %1")
            .arg(nx::vms::common::html::localLink(tr("Learn more..."))),
        .level = BarDescription::BarLevel::Info,
        .isOpenExternalLinks = false,
        .isEnabledProperty = &messageBarSettings()->webPageInformation
    });
    ui->webPageBanner->setDisplayed(false);

    connect(ui->webPageBanner, &CommonMessageBar::linkActivated, this,
        []
        {
            HelpHandler::openHelpTopic(HelpTopic::Id::MainWindow_Tree_WebPage);
        });

    ui->integrationBanner->init({
        .text = tr("An integration may interact with the Desktop Client and request access to the"
            " user session"),
        .level = BarDescription::BarLevel::Warning,
        .isOpenExternalLinks = false,
        .isEnabledProperty = &messageBarSettings()->integrationWarning
    });
    ui->integrationBanner->setDisplayed(false);

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->nameInputField,
        ui->urlInputField,
        ui->clientApiCheckBoxSpacerWidget
    });

    const auto updateSelectServerMenuButtonVisibility =
        [this]
        {
            // If no server is selected - select the current server.
            if (const auto server = ui->selectServerMenuButton->currentServer(); !server)
            {
                ui->selectServerMenuButton->setCurrentServer(currentServer());
            }

            // Show server selection button only if there are servers to choose from.
            const bool visible = ui->proxyViaServerCheckBox->isChecked()
                && m_serversWatcher.serversCount() > 1;

            ui->selectServerMenuButton->setVisible(visible);
            ui->proxyAllContentsCheckBox->setEnabled(ui->proxyViaServerCheckBox->isChecked());

            ui->allowListField->setVisible(
                ini().developerMode
                && ui->proxyViaServerCheckBox->isChecked());

            updateWarningLabel();
        };

    // Setup proxy server selection button.
    connect(&m_serversWatcher, &CurrentSystemServers::serverAdded,
        ui->selectServerMenuButton, &QnChooseServerButton::addServer);
    connect(&m_serversWatcher, &CurrentSystemServers::serverRemoved,
        ui->selectServerMenuButton, &QnChooseServerButton::removeServer);
    for (const auto& server: m_serversWatcher.servers())
        ui->selectServerMenuButton->addServer(server);

    connect(&m_serversWatcher, &CurrentSystemServers::serversCountChanged,
        this, updateSelectServerMenuButtonVisibility);

    connect(ui->proxyViaServerCheckBox, &QCheckBox::stateChanged,
        this, updateSelectServerMenuButtonVisibility);

    connect(ui->proxyAllContentsCheckBox, &QCheckBox::stateChanged,
        this, &QnWebpageDialog::updateWarningLabel);

    ui->clientApiCheckBox->setVisible(!ini().webPagesAndIntegrations);

    connect(ui->clientApiCheckBox, &QCheckBox::clicked, this,
        [this](bool checked)
        {
            setSubtype(checked ? WebPageSubtype::clientApi : WebPageSubtype::none);
        });

    const auto updateSuffix =
        [this]()
        {
            for (int i = 0; i < ui->comboBoxRefreshPeriodUnit->count(); ++i)
            {
                const auto unitSuffix =
                    static_cast<QnTimeStrings::Suffix>(
                        ui->comboBoxRefreshPeriodUnit->itemData(i, UnitSuffixRole).toInt());

                ui->comboBoxRefreshPeriodUnit->setItemText(
                    i,
                    QnTimeStrings::fullSuffixCapitalized(
                        unitSuffix,
                        ui->spinBoxRefreshPeriod->value()));
            }
        };

    connect(ui->spinBoxRefreshPeriod, &QSpinBox::valueChanged, updateSuffix);
    connect(ui->refreshCheckBox, &QCheckBox::stateChanged, this,
        [this, updateSuffix]()
        {
            updateSuffix();
            ui->refreshParametersWidget->setVisible(ui->refreshCheckBox->isChecked());
        });

    static const auto addDurationUnitComboBoxItem =
        [](
            QComboBox* unitComboBox,
            QnTimeStrings::Suffix unitSuffux,
            seconds unitDuration)
        {
            const auto itemText = QnTimeStrings::fullSuffixCapitalized(unitSuffux);

            unitComboBox->addItem(itemText);
            const auto itemIndex = unitComboBox->count() - 1;

            unitComboBox->setItemData(
                itemIndex,
                QVariant(static_cast<int>(unitSuffux)), UnitSuffixRole);
            unitComboBox->setItemData(
                itemIndex,
                QVariant::fromValue(unitDuration),
                UnitDurationRole);
        };

    addDurationUnitComboBoxItem(
        ui->comboBoxRefreshPeriodUnit, QnTimeStrings::Suffix::Seconds, 1s);
    addDurationUnitComboBoxItem(
        ui->comboBoxRefreshPeriodUnit, QnTimeStrings::Suffix::Minutes, 1min);
    ui->comboBoxRefreshPeriodUnit->setCurrentIndex(kInvalidIndex);

    updateText();
    updateSelectServerMenuButtonVisibility();
}

QnWebpageDialog::~QnWebpageDialog()
{
}

void QnWebpageDialog::updateTitle()
{
    using Option = QnWebPageResource::Option;

    QnWebPageResource::Options titleOptions;
    titleOptions.setFlag(Option::Proxied, ui->proxyViaServerCheckBox->isChecked());

    if (ini().webPagesAndIntegrations)
    {
        titleOptions.setFlag(
            Option::Integration, subtype() == nx::vms::api::WebPageSubtype::clientApi);
    }

    setWindowTitle(getTitle(titleOptions));
}

void QnWebpageDialog::updateText()
{
    const bool isIntegration =
        subtype() == nx::vms::api::WebPageSubtype::clientApi && ini().webPagesAndIntegrations;

     ui->proxyViaServerCheckBox->setText(isIntegration
        ? tr("Proxy this integration through the server")
        : tr("Proxy this web page through the server"));

    ui->disableCertificateCheckBox->setText(isIntegration
        ? tr("Allow opening this integration without SSL certificate checking")
        : tr("Allow opening this web page without SSL certificate checking"));

    updateTitle();
}

QString QnWebpageDialog::getTitle(QnWebPageResource::Options options)
{
    const bool isNew = m_editMode == AddPage;

    switch (options)
    {
        case QnWebPageResource::WebPage:
            return isNew ? tr("New Web Page") : tr("Edit Web Page");
        case QnWebPageResource::ProxiedWebPage:
            return isNew ? tr("New Proxied Web Page") : tr("Edit Proxied Web Page");
        case QnWebPageResource::Integration:
            return isNew ? tr("New Integration") : tr("Edit Integration");
        case QnWebPageResource::ProxiedIntegration:
            return isNew ? tr("New Proxied Integration") : tr("Edit Proxied Integration");
    }

    NX_ASSERT(false, "Unexpected value (%1)", options);
    return {};
}

void QnWebpageDialog::updateWarningLabel()
{
    const bool visible = ui->proxyAllContentsCheckBox->isEnabled()
        && ui->proxyAllContentsCheckBox->isChecked();

    ui->proxyAlertLabel->setVisible(visible);

    // Rich text displayed in a tool tip is implicitly word-wrapped unless specified differently
    // with <p style='white-space:pre'>.
    const QString tooltipTextWhenDisabled = nx::format("<p style='white-space:pre'>%1</p>",
        tr("Turn on webpage proxy on <b>General</b> tab."));

    ui->proxyAllContentsCheckBox->setToolTip(ui->proxyAllContentsCheckBox->isEnabled()
        ? QString()
        : tooltipTextWhenDisabled);

    if (ini().developerMode)
        setProxyDomainAllowList(proxyDomainAllowList());
}

QString QnWebpageDialog::name() const
{
    const auto name = ui->nameInputField->text().trimmed();
    return name.isEmpty()
        ? QnWebPageResource::nameForUrl(url())
        : name;
}

QUrl QnWebpageDialog::url() const
{
    return QUrl::fromUserInput(ui->urlInputField->text().trimmed());
}

QnUuid QnWebpageDialog::proxyId() const
{
    if (!ui->proxyViaServerCheckBox->isChecked())
        return {};

    const auto server = ui->selectServerMenuButton->currentServer();
    return server ? server->getId() : QnUuid();
}

void QnWebpageDialog::setName(const QString& name)
{
    ui->nameInputField->setText(name);
}

void QnWebpageDialog::setUrl(const QUrl& url)
{
    ui->urlInputField->setText(url.toString());
}

void QnWebpageDialog::setProxyId(QnUuid id)
{
    const auto server =
        m_serversWatcher.commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(id);

    ui->selectServerMenuButton->setCurrentServer(server);
    ui->proxyViaServerCheckBox->setChecked(!id.isNull());

    m_initialProxyId = id;

    updateText();
}

WebPageSubtype QnWebpageDialog::subtype() const
{
    return m_subtype;
}

void QnWebpageDialog::setSubtype(WebPageSubtype value)
{
    const bool isIntegration = value == nx::vms::api::WebPageSubtype::clientApi;

    if (ini().webPagesAndIntegrations)
    {
        ui->webPageBanner->setDisplayed(
            !isIntegration && messageBarSettings()->webPageInformation());
        ui->integrationBanner->setDisplayed(
            isIntegration && messageBarSettings()->integrationWarning());
    }
    else
    {
        ui->clientApiCheckBox->setChecked(isIntegration);
        ui->clientApiAlertLabel->setVisible(isIntegration);
    }

    m_subtype = value;
    updateText();
}

QStringList QnWebpageDialog::proxyDomainAllowList() const
{
    // Do not clear the whole list, just add or remove `*` domain wildcard.

    auto currentList = ui->allowListField->text().trimmed().split(',', Qt::SkipEmptyParts);

    if (ui->proxyAllContentsCheckBox->isChecked())
    {
        if (!currentList.contains(kAnyDomain))
            currentList.append(kAnyDomain);
    }
    else
    {
        currentList.removeAll(kAnyDomain);
    }

    return currentList;
}

void QnWebpageDialog::setProxyDomainAllowList(const QStringList& allowList)
{
    m_initialProxyAll = allowList.contains(kAnyDomain);
    ui->proxyAllContentsCheckBox->setChecked(m_initialProxyAll);
    ui->allowListField->setText(allowList.join(','));
}

seconds QnWebpageDialog::refreshInterval() const
{
    if (!ui->refreshCheckBox->isChecked())
        return 0s;

    return ui->comboBoxRefreshPeriodUnit->itemData(
        ui->comboBoxRefreshPeriodUnit->currentIndex(),
        UnitDurationRole
    ).value<seconds>() * ui->spinBoxRefreshPeriod->value();
}

void QnWebpageDialog::setRefreshInterval(seconds interval)
{
    const int value = interval.count();
    ui->refreshCheckBox->setChecked(value > 0);

    int periodIndex = 0;
    int spinBoxValue = 5; //< Default value is 5 seconds.

    for (int i = 0; i < ui->comboBoxRefreshPeriodUnit->count(); ++i)
    {
        const auto newValue = value
            / ui->comboBoxRefreshPeriodUnit->itemData(i, UnitDurationRole).value<seconds>().count();

        if (newValue <= 0)
            break;

        periodIndex = i;
        spinBoxValue = newValue;
    }

    ui->comboBoxRefreshPeriodUnit->setCurrentIndex(periodIndex);
    ui->spinBoxRefreshPeriod->setValue(spinBoxValue);
}

void QnWebpageDialog::setResourceId(QnUuid id)
{
    m_resourceId = id;
}

bool QnWebpageDialog::isCertificateCheckEnabled() const
{
    return !ui->disableCertificateCheckBox->isChecked();
}

void QnWebpageDialog::setCertificateCheckEnabled(bool value)
{
    ui->disableCertificateCheckBox->setChecked(!value);
}

void QnWebpageDialog::accept()
{
    if (!ui->urlInputField->validate())
        return;

    const bool keepProxyAll = m_initialProxyAll && proxyDomainAllowList().contains(kAnyDomain);
    const bool changeProxyServer = !proxyId().isNull() && proxyId() != m_initialProxyId;

    if (keepProxyAll && changeProxyServer)
    {
        const auto resourcePool = m_serversWatcher.commonModule()->resourcePool();
        const auto server = resourcePool->getResourceById<QnMediaServerResource>(proxyId());
        const auto page = resourcePool->getResourceById<QnWebPageResource>(m_resourceId);

        if (!ui::messages::Resources::moveProxiedWebPages(this, {page}, server))
            return;
    }

    base_type::accept();
}

} // namespace nx::vms::client::desktop
