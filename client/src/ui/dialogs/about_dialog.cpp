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

#include "core/resource/resource_type.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource/media_server_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/style/globals.h>

#include <utils/common/email.h>

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

    if(menu()->canTrigger(Qn::ShowcaseAction)) {
        QPushButton* showcaseButton = new QPushButton(this);
        showcaseButton->setText(action(Qn::ShowcaseAction)->text());
        connect(showcaseButton, &QPushButton::clicked, action(Qn::ShowcaseAction), &QAction::trigger);
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


QString QnAboutDialog::connectedServers() const {
    QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();

    QnSoftwareVersion latestVersion = watcher->latestVersion();
    QnSoftwareVersion latestMsVersion = watcher->latestVersion(Qn::ServerComponent);

    // if some component is newer than the newest mediaserver, focus on its version
    if (QnWorkbenchVersionMismatchWatcher::versionMismatches(latestVersion, latestMsVersion))
        latestMsVersion = latestVersion;

    QString servers;
    foreach(const QnAppInfoMismatchData &data, watcher->mismatchData()) {
        if (data.component != Qn::ServerComponent)
            continue;

        QnMediaServerResourcePtr resource = data.resource.dynamicCast<QnMediaServerResource>();
        if(!resource) 
            continue;
                      
        QString server = tr("Server at %2: v%1<br/>").arg(data.version.toString()).arg(QUrl(resource->getUrl()).host());

        bool updateRequested = QnWorkbenchVersionMismatchWatcher::versionMismatches(data.version, latestMsVersion, true);

        if (updateRequested)
            server = QString(lit("<font color=\"%1\">%2</font>")).arg(qnGlobals->errorTextColor().name()).arg(server);

        servers += server;
    }

    return servers;
}




void QnAboutDialog::retranslateUi()
{
    ui->retranslateUi(this);

    m_copyButton->setText(tr("Copy to Clipboard"));

    QString version = 
        tr(
            "<b>%1</b> version %2 (%3).<br/>\n"
            "Built for %5-%6 with %7.<br/>\n"
        ).
        arg(qApp->applicationDisplayName()).
        arg(QApplication::applicationVersion()).
        arg(QnAppInfo::applicationRevision()).
        arg(QnAppInfo::applicationPlatform()).
        arg(QnAppInfo::applicationArch()).
        arg(QnAppInfo::applicationCompiler());

//    QnSoftwareVersion ecsVersion = QnAppServerConnectionFactory::currentVersion();
    QString servers = connectedServers();
    if (servers.isEmpty())
        servers = tr("<b>Client</b> is not connected to <b>Server</b>.<br>");
    
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
    credits << lit("<b>Bespin style</b> - Copyright (c) 2007-2010 Thomas Luebking.");

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);

    QStringList gpu;
     // TODO: #Elric same shit, OpenGL calls.
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL version")).arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VERSION))));
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL renderer")).arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER))));
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL vendor")).arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VENDOR))));
    gpu << lit("<b>%1</b>: %2.").arg(tr("OpenGL max texture size")).arg(maxTextureSize);
    
    ui->versionLabel->setText(version);
    ui->creditsLabel->setText(credits.join(lit("<br>\n")));
    ui->gpuLabel->setText(gpu.join(lit("<br>\n")));
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



