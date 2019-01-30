#include "about_dialog.h"
#include "ui_about_dialog.h"

#include <QtCore/QEvent>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QApplication>

#include <QtGui/QClipboard>
#include <QtGui/QTextDocumentFragment>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource/resource_display_info.h>
#include "core/resource/resource_type.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource/media_server_resource.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/resource_views/functional_delegate_utilities.h>

#include <ui/delegates/resource_item_delegate.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>

#include <helpers/cloud_url_helper.h>
#include <utils/email/email.h>
#include <utils/common/html.h>
#include <nx/audio/audiodevice.h>
#include <utils/common/app_info.h>
#include <utils/common/html.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>

using namespace nx::vms::client::desktop;

namespace {
    QString versionString(const QString &version) {
        QString result = version;
        result.replace(lit("-SNAPSHOT"), QString());
        return result;
    }

} // anonymous namespace

QnAboutDialog::QnAboutDialog(QWidget *parent):
    base_type(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::AboutDialog())
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::About_Help);

    m_copyButton = new ClipboardButton(ClipboardButton::StandardType::copyLong, this);
    ui->buttonBox->addButton(m_copyButton, QDialogButtonBox::HelpRole);

    ui->servers->setItemDelegateForColumn(0, new QnResourceItemDelegate(this));
    ui->servers->setItemDelegateForColumn(1, makeVersionStatusDelegate(context(), this));

    m_serverListModel = new QnResourceListModel(this);
    m_serverListModel->setHasStatus(true);

    // Custom accessor to get a string with server version.
    m_serverListModel->setCustomColumnAccessor(1, resourceVersionAccessor);
    ui->servers->setModel(m_serverListModel);

    auto* header = ui->servers->horizontalHeader();
    header->setSectionsClickable(false);
    header->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);

    // Fill in server list and generate report.
    generateServersReport();

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QnAboutDialog::reject);
    connect(m_copyButton, &QPushButton::clicked, this, &QnAboutDialog::at_copyButton_clicked);
    connect(qnGlobalSettings, &QnGlobalSettings::emailSettingsChanged, this,
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
    const auto watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();

    QnResourceList servers;
    QStringList report;
    for (const auto& data: watcher->mismatchData())
    {
        if (data.component != Qn::ServerComponent)
            continue;

        QnMediaServerResourcePtr server = data.resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QString version = L'v' + data.version.toString();
        QString serverText = lit("%1: %2")
            .arg(QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl), version);
        report << serverText;
        servers << server;
    }

    this->m_serversReport = report.join(lit("<br/>"));

    m_serverListModel->setResources(servers);
    ui->serversGroupBox->setVisible(!servers.empty());
}

void QnAboutDialog::retranslateUi()
{
    ui->retranslateUi(this);

    QStringList version;
    version <<
        tr("%1 version %2 (%3).")
        .arg(htmlBold(qApp->applicationDisplayName()))
        .arg(qApp->applicationVersion())
        .arg(QnAppInfo::applicationRevision());
    version <<
        tr("Built for %1-%2 with %3.")
        .arg(QnAppInfo::applicationPlatform())
        .arg(QnAppInfo::applicationArch())
        .arg(QnAppInfo::applicationCompiler());

    QString appName = lit("<b>%1&trade; %2</b>")
        .arg(QnAppInfo::organizationName())
        .arg(qApp->applicationDisplayName());

    QStringList credits;
    credits << tr("%1 uses the following external libraries:").arg(appName);
    credits << lit("<b>Qt v.%1</b> - Copyright &copy; 2015 The Qt Company Ltd.")
        .arg(QLatin1String(QT_VERSION_STR));
    credits << QString();
    credits << lit("<b>FFMpeg %1</b> - Copyright &copy; 2000-2018 FFmpeg developers.")
    .arg(versionString(QnAppInfo::ffmpegVersion()));
    credits << lit("<b>OpenAL %1</b> - Copyright &copy; 2000-2006 %2.")
        .arg(nx::audio::AudioDevice::instance()->versionString())
        .arg(nx::audio::AudioDevice::instance()->company());
    credits << lit("<b>SIGAR %1</b> - Copyright &copy; 2004-2011 VMware Inc.")
        .arg(versionString(QnAppInfo::sigarVersion()));
    credits << lit("<b>Boost %1</b> - Copyright &copy; 2000-2012 Boost developers.")
        .arg(versionString(QnAppInfo::boostVersion()));

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);

    QStringList gpu;
     // TODO: #Elric same shit, OpenGL calls.
	auto gl = QnGlFunctions::openGLCachedInfo();
	gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL version")).arg(gl.version);
	gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL renderer")).arg(gl.renderer);
	gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL vendor")).arg(gl.vendor);
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL max texture size")).arg(maxTextureSize);

    const QString lineSeparator = lit("<br>\n");

    ui->versionLabel->setText(version.join(lineSeparator));
    ui->creditsLabel->setText(credits.join(lineSeparator));
    ui->gpuLabel->setText(gpu.join(lineSeparator));

    QString supportAddress = qnGlobalSettings->emailSettings().supportEmail;
    QString supportLink = supportAddress;
    QnEmailAddress supportEmail(supportAddress);

    // Check if email is provided
    if (supportEmail.isValid())
        supportLink = makeMailHref(supportAddress);
    // simple check if phone is provided
    else if (!supportAddress.isEmpty() && !supportAddress.startsWith(lit("+")))
        supportLink = makeHref(supportAddress);

    const auto makeTitleLabel =
        [](QLabel* label)
        {
            label->setText(QString("<b>%1</b>:").arg(label->text()));
        };

    makeTitleLabel(ui->customerSupportTitleLabel);
    makeTitleLabel(ui->armSupportTitleLabel);

    ui->customerSupportLabel->setText(supportLink);

    QnCloudUrlHelper cloudUrlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::SettingsDialog);

    ui->armSupportLabel->setText(makeHref(
        cloudUrlHelper.armPolicyUrl().toDisplayString(QUrl::RemoveQuery),
        cloudUrlHelper.armPolicyUrl()));
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
