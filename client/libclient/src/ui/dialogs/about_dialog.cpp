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

#include <core/resource/resource_display_info.h>
#include "core/resource/resource_type.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource/media_server_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/style/custom_style.h>

#include <utils/email/email.h>
#include <utils/common/html.h>

#include "openal/qtvaudiodevice.h"
#include <utils/common/app_info.h>

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

    if(menu()->canTrigger(QnActions::ShowcaseAction)) {
        QPushButton* showcaseButton = new QPushButton(this);
        showcaseButton->setText(action(QnActions::ShowcaseAction)->text());
        connect(showcaseButton, &QPushButton::clicked, action(QnActions::ShowcaseAction), &QAction::trigger);
        ui->buttonBox->addButton(showcaseButton, QDialogButtonBox::HelpRole);
    }

    m_copyButton = new QPushButton(this);
    ui->buttonBox->addButton(m_copyButton, QDialogButtonBox::HelpRole);

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QnAboutDialog::reject);
    connect(m_copyButton, &QPushButton::clicked, this, &QnAboutDialog::at_copyButton_clicked);
    connect(QnGlobalSettings::instance(), &QnGlobalSettings::emailSettingsChanged, this, &QnAboutDialog::retranslateUi);
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(), &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged, this, &QnAboutDialog::retranslateUi);

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

QString QnAboutDialog::connectedServers() const
{
    QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();

    QnSoftwareVersion latestVersion = watcher->latestVersion();
    QnSoftwareVersion latestMsVersion = watcher->latestVersion(Qn::ServerComponent);

    // if some component is newer than the newest mediaserver, focus on its version
    if (QnWorkbenchVersionMismatchWatcher::versionMismatches(latestVersion, latestMsVersion))
        latestMsVersion = latestVersion;

    QStringList servers;
    for (const QnAppInfoMismatchData &data: watcher->mismatchData())
    {
        if (data.component != Qn::ServerComponent)
            continue;

        QnMediaServerResourcePtr server = data.resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        /* Consistency with QnWorkbenchActionHandler::at_versionMismatchMessageAction_triggered */
        QString version = L'v' + data.version.toString();
        bool updateRequested = QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestMsVersion, true);
        if (updateRequested)
            version = setWarningStyleHtml(version);

        QString serverText = lit("%1: %2").arg(QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl), version);
        servers << serverText;
    }

    return servers.join(lit("<br/>"));
}

void QnAboutDialog::retranslateUi()
{
    ui->retranslateUi(this);

    m_copyButton->setText(tr("Copy to Clipboard"));

    QStringList version;
    version <<
        tr("%1 version %2 (%3).")
        .arg(htmlBold(qApp->applicationDisplayName()))
        .arg(QApplication::applicationVersion())
        .arg(QnAppInfo::applicationRevision());
    version <<
        tr("Built for %1-%2 with %3.")
        .arg(QnAppInfo::applicationPlatform())
        .arg(QnAppInfo::applicationArch())
        .arg(QnAppInfo::applicationCompiler());

//    QnSoftwareVersion ecsVersion = QnAppServerConnectionFactory::currentVersion();
    QString servers = connectedServers();
    if (servers.isEmpty())
        servers = tr("Client is not connected to any server");

    QString appName = lit("<b>%1%2 %3</b>")
        .arg(QnAppInfo::organizationName())
        .arg(lit("(tm)"))
        .arg(qApp->applicationDisplayName());

    QStringList credits;
    credits << tr("%1 uses the following external libraries:").arg(appName);
    credits << QString();
    credits << lit("<b>Qt v.%1</b> - Copyright (c) 2012 Nokia Corporation.").arg(QLatin1String(QT_VERSION_STR));
    credits << lit("<b>FFMpeg %1</b> - Copyright (c) 2000-2012 FFmpeg developers.").arg(versionString(QnAppInfo::ffmpegVersion()));
    credits << lit("<b>LAME 3.99.0</b> - Copyright (c) 1998-2012 LAME developers.");
    credits << lit("<b>OpenAL %1</b> - Copyright (c) 2000-2006 %2.").arg(QtvAudioDevice::instance()->versionString()).arg(QtvAudioDevice::instance()->company());
    credits << lit("<b>SIGAR %1</b> - Copyright (c) 2004-2011 VMware Inc.").arg(versionString(QnAppInfo::sigarVersion()));
    credits << lit("<b>Boost %1</b> - Copyright (c) 2000-2012 Boost developers.").arg(versionString(QnAppInfo::boostVersion()));

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
    ui->serversLabel->setText(servers);

    QString supportAddress = QnGlobalSettings::instance()->emailSettings().supportEmail;
    QString supportLink = supportAddress;
    if (QnEmailAddress::isValid(supportAddress))
        supportLink = lit("<a href=mailto:%1>%1</a>").arg(supportAddress);
    else if (!supportAddress.isEmpty())
        supportLink = lit("<a href=%1>%1</a>").arg(supportAddress);
    ui->supportEmailLabel->setText(lit("<b>%1</b>: %2").arg(tr("Support")).arg(supportLink));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnAboutDialog::at_copyButton_clicked() {
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(
         QTextDocumentFragment::fromHtml(ui->versionLabel->text()).toPlainText() +
         QLatin1String("\n") +
         QTextDocumentFragment::fromHtml(ui->gpuLabel->text()).toPlainText() +
         QLatin1String("\n") +
         QTextDocumentFragment::fromHtml(ui->serversLabel->text()).toPlainText()
    );
}



