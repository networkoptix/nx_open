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

#include "openal/qtvaudiodevice.h"
#include "version.h"

namespace {
    QString versionString(const char *version) {
        QString result = QLatin1String(version);
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
    foreach(const QnVersionMismatchData &data, watcher->mismatchData()) {
        if (data.component != Qn::ServerComponent)
            continue;

        QnMediaServerResourcePtr resource = data.resource.dynamicCast<QnMediaServerResource>();
        if(!resource) 
            continue;
                      
        QString server = tr("Server v%1 at %2<br/>").arg(data.version.toString()).arg(QUrl(resource->getUrl()).host());

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
        arg(QLatin1String(QN_APPLICATION_NAME)).
        arg(QApplication::applicationVersion()).
        arg(QLatin1String(QN_APPLICATION_REVISION)).
        arg(QLatin1String(QN_APPLICATION_PLATFORM)).
        arg(QLatin1String(QN_APPLICATION_ARCH)).
        arg(QLatin1String(QN_APPLICATION_COMPILER));

//    QnSoftwareVersion ecsVersion = QnAppServerConnectionFactory::currentVersion();
    QString servers = connectedServers();
    if (servers.isEmpty())
        servers = tr("<b>Client</b> is not connected to <b>Server</b>.<br>");
    
    QString credits = 
        tr(
            "<b>%1 %2</b> uses the following external libraries:<br/>\n"
            "<br />\n"
            "<b>Qt v.%3</b> - Copyright (c) 2012 Nokia Corporation.<br/>\n"
            "<b>FFMpeg %4</b> - Copyright (c) 2000-2012 FFmpeg developers.<br/>\n"
            "<b>LAME 3.99.0</b> - Copyright (c) 1998-2012 LAME developers.<br/>\n"
            "<b>OpenAL %5</b> - Copyright (c) 2000-2006 %6.<br/>\n"
            "<b>SIGAR %7</b> - Copyright (c) 2004-2011 VMware Inc.<br/>\n"
            "<b>Boost %8</b> - Copyright (c) 2000-2012 Boost developers.<br/>\n"
        ).
        arg(QLatin1String(QN_ORGANIZATION_NAME) + QLatin1String("(tm)")).
        arg(QLatin1String(QN_APPLICATION_NAME)).
        arg(QLatin1String(QT_VERSION_STR)).
        arg(versionString(QN_FFMPEG_VERSION)).
        arg(QtvAudioDevice::instance()->versionString()).
        arg(QtvAudioDevice::instance()->company()).
        arg(versionString(QN_SIGAR_VERSION)).
        arg(versionString(QN_BOOST_VERSION));

#ifndef Q_OS_DARWIN
    credits += tr("<b>Bespin style</b> - Copyright (c) 2007-2010 Thomas Luebking.<br/>");
#endif

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);

    QString gpu = 
        tr(
            "<b>OpenGL version</b>: %1.<br/>\n"
            "<b>OpenGL renderer</b>: %2.<br/>\n"
            "<b>OpenGL vendor</b>: %3.<br/>\n"
            "<b>OpenGL max texture size</b>: %4.<br/>\n"
        ).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VERSION)))).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)))). // TODO: #Elric same shit, OpenGL calls.
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VENDOR)))).
        arg(maxTextureSize);

    
    ui->versionLabel->setText(version);
    ui->creditsLabel->setText(credits);
    ui->gpuLabel->setText(gpu);
    ui->serversLabel->setText(servers);

    QString emailLink = lit("<a href=mailto:%1>%1</a>").arg(QnGlobalSettings::instance()->emailSettings().supportEmail);
    ui->supportEmailLabel->setText(tr("<b>Email</b>: %1").arg(emailLink));
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



