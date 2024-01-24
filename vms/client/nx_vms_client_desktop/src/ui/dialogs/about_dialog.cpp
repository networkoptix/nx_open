// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "about_dialog.h"
#include "ui_about_dialog.h"

#include <QtCore/QEvent>
#include <QtGui/QClipboard>
#include <QtGui/QTextDocumentFragment>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
#endif

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_type.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/audio/audiodevice.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/licensing/customer_support.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource_views/functional_delegate_utilities.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

namespace {

QString graphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api)
    {
        case QSGRendererInterface::Unknown: return "Unknown";
        case QSGRendererInterface::Software: return "Software";
        case QSGRendererInterface::OpenVG: return "OpenVG";
        case QSGRendererInterface::OpenGL: return "OpenGL";
        case QSGRendererInterface::Direct3D11: return "Direct3D11";
        case QSGRendererInterface::Vulkan: return "Vulkan";
        case QSGRendererInterface::Metal: return "Metal";
        case QSGRendererInterface::Null: return "Null";
        default: return QString("%1").arg(api);
    }
}

} // namespace

QnAboutDialog::QnAboutDialog(QWidget *parent):
    base_type(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::AboutDialog())
{
    ui->setupUi(this);

    setHelpTopic(this, HelpTopic::Id::About);

    m_copyButton = new ClipboardButton(ClipboardButton::StandardType::copyLong, this);
    ui->buttonBox->addButton(m_copyButton, QDialogButtonBox::HelpRole);

    ui->servers->setItemDelegateForColumn(
        QnResourceListModel::NameColumn, new QnResourceItemDelegate(this));
    ui->servers->setItemDelegateForColumn(
        QnResourceListModel::CheckColumn, makeVersionStatusDelegate(workbenchContext(), this));

    m_serverListModel = new QnResourceListModel(this);
    m_serverListModel->setHasStatus(true);

    // Custom accessor to get a string with server version.
    m_serverListModel->setCustomColumnAccessor(
        QnResourceListModel::CheckColumn, resourceVersionAccessor);
    ui->servers->setModel(m_serverListModel);

    auto* header = ui->servers->horizontalHeader();
    header->setSectionsClickable(false);
    header->setSectionResizeMode(
        QnResourceListModel::NameColumn, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(
        QnResourceListModel::CheckColumn, QHeaderView::ResizeMode::ResizeToContents);

    // Fill in server list and generate report.
    generateServersReport();

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QnAboutDialog::reject);
    connect(m_copyButton, &QPushButton::clicked, this, &QnAboutDialog::at_copyButton_clicked);
    connect(system()->globalSettings(), &SystemSettings::emailSettingsChanged, this,
        &QnAboutDialog::retranslateUi);
    connect(workbenchContext()->instance<QnWorkbenchVersionMismatchWatcher>(),
        &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged,
        this,
        &QnAboutDialog::retranslateUi);

    connect(windowContext(), &WindowContext::systemChanged, this, &QnAboutDialog::retranslateUi);

    if (saas::saasInitialized(systemContext()))
        initSaasSupportInfo();

    retranslateUi();
}

QnAboutDialog::~QnAboutDialog()
{
}

void QnAboutDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    if(event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void QnAboutDialog::generateServersReport()
{
    using namespace nx::vms::common;

    const bool isAdmin = system()->accessController()->hasPowerUserPermissions();
    QnMediaServerResourceList servers;
    QStringList report;

    if (isAdmin)
    {
        const auto watcher = workbenchContext()->instance<QnWorkbenchVersionMismatchWatcher>();
        for (const auto& data: watcher->components())
        {
            if (data.component != QnWorkbenchVersionMismatchWatcher::Component::server)
                continue;

            NX_ASSERT(data.server);
            if (!data.server)
                continue;

            QString version = 'v' + data.version.toString();
            QString serverText = lit("%1: %2")
                .arg(QnResourceDisplayInfo(data.server).toString(Qn::RI_WithUrl), version);
            report << serverText;
            servers << data.server;
        }
    }

    this->m_serversReport = report.join(html::kLineBreak);

    m_serverListModel->setResources(servers);
    ui->serversGroupBox->setVisible(!servers.empty());
}

void QnAboutDialog::retranslateUi()
{
    using namespace nx::vms::common;

    ui->retranslateUi(this);

    QStringList version;
    version <<
        tr("%1 version %2 (%3).")
        .arg(html::bold(qApp->applicationDisplayName()))
        .arg(qApp->applicationVersion())
        .arg(nx::build_info::revision());
    version <<
        tr("Built for %1-%2 with %3.")
        .arg(nx::build_info::applicationPlatform())
        .arg(nx::build_info::applicationArch())
        .arg(nx::build_info::applicationCompiler());
    ui->versionLabel->setText(version.join(html::kLineBreak));

    const QString brandName = html::bold(nx::branding::company());
    ui->developerNameLabel->setText(brandName);

    core::CloudUrlHelper cloudUrlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::AboutDialog);

    const auto cloudLinkHtml =
        nx::format("<a href=\"%1\">%2</a>").args(
            cloudUrlHelper.cloudLinkUrl(/*withReferral*/ true),
            cloudUrlHelper.cloudLinkUrl(/*withReferral*/ false));

    ui->openSourceLibrariesLabel->setText(cloudLinkHtml);

    QStringList gpu;

    const auto quickWindow =
        nx::vms::client::desktop::appContext()->mainWindowContext()->quickWindow();
    const auto graphicsApi = (quickWindow && quickWindow->rendererInterface())
        ? quickWindow->rendererInterface()->graphicsApi()
        : QSGRendererInterface::OpenGL; //< Assume defaut.

    if (graphicsApi == QSGRendererInterface::OpenGL)
    {
        int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);

        auto gl = QnGlFunctions::openGLInfo();
        gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL version")).arg(gl.version);
        gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL renderer")).arg(gl.renderer);
        gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL vendor")).arg(gl.vendor);
        gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL max texture size")).arg(maxTextureSize);
    }
    else if (const auto rhi = reinterpret_cast<QRhi*>(
            quickWindow->rendererInterface()->getResource(
                quickWindow,
                QSGRendererInterface::RhiResource)))
    {
        const auto driverInfo = rhi->driverInfo();
        gpu << QString("<b>%1</b>: %2.").arg(tr("RHI backend")).arg(rhi->backendName());
        gpu << QString("<b>%1</b>: %2.").arg(tr("RHI device")).arg(driverInfo.deviceName);
        gpu << QString("<b>%1</b>: %2.").arg(tr("RHI device ID")).arg(driverInfo.deviceId, 0, 16);
        gpu << QString("<b>%1</b>: %2.").arg(tr("RHI vendor ID")).arg(driverInfo.vendorId, 0, 16);
        gpu << QString("<b>%1</b>: %2.").arg(tr("RHI max texture size")).arg(rhi->resourceLimit(QRhi::TextureSizeMax));
    }
    else if (graphicsApi == QSGRendererInterface::Software)
    {
        gpu << QString("<b>%1</b>: %2.").arg(tr("Graphics API")).arg(graphicsApiName(graphicsApi));
    }
    else
    {
        gpu << tr("Unable to get GPU information for %1").arg(graphicsApiName(graphicsApi));
    }

    ui->gpuLabel->setText(gpu.join(html::kLineBreak));

    CustomerSupport customerSupport(system());

    ui->customerSupportTitleLabel->setText(customerSupport.systemContact.company);
    const bool isValidLink = QUrl(customerSupport.systemContact.address.rawData).isValid();
    ui->customerSupportLabel->setText(isValidLink
        ? customerSupport.systemContact.address.href
        : customerSupport.systemContact.address.rawData);
    ui->customerSupportLabel->setOpenExternalLinks(isValidLink);

    int row = 1;
    for (const CustomerSupport::Contact& contact: customerSupport.regionalContacts)
    {
        const QString text = contact.company + html::kLineBreak + contact.address.href;
        ui->supportLayout->addWidget(new QLabel(tr("Regional / License support")), row, /*column*/ 0);
        ui->supportLayout->addWidget(new QLabel(text), row, /*column*/ 1);
        ++row;
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnAboutDialog::at_copyButton_clicked()
{
    const QLatin1String nextLine("\n");
    QString output;
    output += QTextDocumentFragment::fromHtml(ui->versionLabel->text()).toPlainText() + nextLine;
    output += QTextDocumentFragment::fromHtml(ui->gpuLabel->text()).toPlainText() + nextLine;
    output += QTextDocumentFragment::fromHtml(m_serversReport).toPlainText();

    QApplication::clipboard()->setText(output);
}

void QnAboutDialog::initSaasSupportInfo()
{
    static constexpr auto kContentsColumn = 1;
    static constexpr auto kSpacerHeight = 2;
    static constexpr auto kCaptionFontWeight = QFont::Medium;

    const auto addLabelAtPosition =
        [this](const QString& text, int row, int column) -> QLabel*
        {
            auto layout = ui->supportLayout;
            if (auto item = layout->itemAtPosition(row, column))
            {
                NX_ASSERT(false, "Label replaced existing layout item");
                layout->removeItem(item);
                delete item;
            }

            auto label = new QLabel(text);
            layout->addWidget(label, row, column);
            return label;
        };

    const auto appendLabelRow =
        [this, &addLabelAtPosition](const QString& text, QFont::Weight fontWeight = QFont::Normal)
        {
            auto layout = ui->supportLayout;
            auto label = addLabelAtPosition(text, layout->rowCount(), kContentsColumn);
            auto font = label->font();
            font.setWeight(fontWeight);
            label->setFont(font);
            return label;
        };

    const auto addSpacer =
        [this]
        {
            auto layout = ui->supportLayout;
            auto spacerItem = new QSpacerItem(/*width*/ 0, kSpacerHeight,
                QSizePolicy::Minimum, QSizePolicy::Fixed);
            layout->addItem(spacerItem, layout->rowCount(), kContentsColumn);
        };

    const auto saasData = systemContext()->saasServiceManager()->data();
    const auto channelPartnerSupportData = saasData.channelPartner.supportInformation;
    const auto channelPartnerName = saasData.channelPartner.name;

    // It's expected that only valid data structures are returned by the partners API.

    addSpacer();
    appendLabelRow(channelPartnerName, kCaptionFontWeight);
    addLabelAtPosition(tr("Partner information"), ui->supportLayout->rowCount() - 1, /*column*/ 0);

    // Web links.
    for (const auto& siteInfo: channelPartnerSupportData.sites)
    {
        auto webAddressLabel = appendLabelRow(html::link(QUrl(siteInfo.value)));
        webAddressLabel->setOpenExternalLinks(true);
    }

    // Phone numbers.
    if (!channelPartnerSupportData.phones.empty())
    {
        addSpacer();
        appendLabelRow(tr("Phones"), kCaptionFontWeight);
    }
    for (const auto& phoneInfo: channelPartnerSupportData.phones)
    {
        auto phoneText = html::phoneNumberLink(phoneInfo.value);
        if (!phoneInfo.description.isEmpty())
            phoneText = nx::format("%1 - %2").args(phoneText, phoneInfo.description);

        auto phoneLabel = appendLabelRow(phoneText);
        phoneLabel->setOpenExternalLinks(true);
    }

    // Email addresses.
    if (!channelPartnerSupportData.emails.empty())
    {
        addSpacer();
        appendLabelRow(tr("Emails"), kCaptionFontWeight);
    }
    for (const auto& emailInfo: channelPartnerSupportData.emails)
    {
        auto emailLabel = appendLabelRow(html::mailtoLink(emailInfo.value));
        emailLabel->setOpenExternalLinks(true);
    }

    // Custom data.
    for (const auto& customInfo: channelPartnerSupportData.custom)
    {
        addSpacer();
        appendLabelRow(customInfo.label, kCaptionFontWeight);
        appendLabelRow(customInfo.value)->setWordWrap(true);
    }
}
