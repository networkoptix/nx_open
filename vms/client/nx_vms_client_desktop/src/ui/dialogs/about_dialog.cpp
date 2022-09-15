// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "about_dialog.h"
#include "ui_about_dialog.h"

#include <QtCore/QEvent>
#include <QtGui/QClipboard>
#include <QtGui/QTextDocumentFragment>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_type.h>
#include <helpers/cloud_url_helper.h>
#include <licensing/license.h>
#include <nx/audio/audiodevice.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/licensing/customer_support.h>
#include <nx/vms/client/desktop/resource_views/functional_delegate_utilities.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

QnAboutDialog::QnAboutDialog(QWidget *parent):
    base_type(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::AboutDialog())
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::About_Help);

    m_copyButton = new ClipboardButton(ClipboardButton::StandardType::copyLong, this);
    ui->buttonBox->addButton(m_copyButton, QDialogButtonBox::HelpRole);

    ui->servers->setItemDelegateForColumn(
        QnResourceListModel::NameColumn, new QnResourceItemDelegate(this));
    ui->servers->setItemDelegateForColumn(
        QnResourceListModel::CheckColumn, makeVersionStatusDelegate(context(), this));

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
    connect(systemSettings(), &SystemSettings::emailSettingsChanged, this,
        &QnAboutDialog::retranslateUi);
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(),
        &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged,
        this,
        &QnAboutDialog::retranslateUi);

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

    const bool isAdmin = accessController()->hasGlobalPermission(GlobalPermission::admin);
    QnMediaServerResourceList servers;
    QStringList report;

    if (isAdmin)
    {
        const auto watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
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

    QnCloudUrlHelper* cloudUrlHelper(new QnCloudUrlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::AboutDialog,
        this));

    const auto cloudLinkHtml =
        nx::format("<a href=\"%1\">%2</a>").args(
            cloudUrlHelper->cloudLinkUrl(/*withReferral*/ true),
            cloudUrlHelper->cloudLinkUrl(/*withReferral*/ false));

    ui->openSourceLibrariesLabel->setText(cloudLinkHtml);

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);

    QStringList gpu;
    auto gl = QnGlFunctions::openGLInfo();
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL version")).arg(gl.version);
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL renderer")).arg(gl.renderer);
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL vendor")).arg(gl.vendor);
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL max texture size")).arg(maxTextureSize);
    ui->gpuLabel->setText(gpu.join(html::kLineBreak));

    CustomerSupport customerSupport(systemContext());

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
