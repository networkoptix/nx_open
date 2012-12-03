#include "about_dialog.h"
#include "ui_about_dialog.h"

#include <QtCore/QEvent>

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtGui/QTextDocumentFragment>

#include <ui/graphics/opengl/gl_functions.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include "api/app_server_connection.h"
#include "core/resource/resource_type.h"
#include "core/resource_managment/resource_pool.h"

#include "openal/qtvaudiodevice.h"
#include "version.h"

namespace {
    QString versionString(const char *version) {
        QString result = QString::fromLatin1(version);
        result.replace(QLatin1String("-SNAPSHOT"), QString());
        return result;
    }

} // anonymous namespace

QnAboutDialog::QnAboutDialog(QWidget *parent): 
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::AboutDialog())
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::About_Help);

    m_copyButton = new QPushButton(this);
    ui->buttonBox->addButton(m_copyButton, QDialogButtonBox::HelpRole);

    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_copyButton, SIGNAL(clicked()), this, SLOT(at_copyButton_clicked()));

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

void QnAboutDialog::retranslateUi()
{
    ui->retranslateUi(this);

    m_copyButton->setText(tr("Copy to Clipboard"));

    setWindowTitle(tr("About %1").arg(QString::fromLatin1(QN_APPLICATION_NAME)));

    QString version = 
        tr(
            "<b>%1</b> version %2 (%3).<br/>\n"
            "Engine version %4.<br/>\n"
            "Built for %5-%6 with %7.<br/>\n"
        ).
        arg(QLatin1String(QN_APPLICATION_NAME)).
        arg(QLatin1String(QN_APPLICATION_VERSION)).
        arg(QLatin1String(QN_APPLICATION_REVISION)).
        arg(QLatin1String(QN_ENGINE_VERSION)).
        arg(QLatin1String(QN_APPLICATION_PLATFORM)).
        arg(QLatin1String(QN_APPLICATION_ARCH)).
        arg(QLatin1String(QN_APPLICATION_COMPILER));

    QString ecsVersion = QnAppServerConnectionFactory::currentVersion();
    QUrl ecsUrl = QnAppServerConnectionFactory::defaultUrl();
    QString servers;

    if (ecsVersion.isEmpty()) {
        servers = tr("<b>Enterprise controller</b> not connected.<br>\n");
    } else {
        servers = tr("<b>Enterprise controller</b> version %1 at %2:%3.<br>\n").
            arg(ecsVersion).
            arg(ecsUrl.host()).
            arg(ecsUrl.port());
    }

    QStringList serverVersions;
    foreach (QnMediaServerResourcePtr server, qnResPool->getResources().filtered<QnMediaServerResource>()) {
        if (server->getStatus() != QnResource::Online)
            continue;

        serverVersions.append(tr("<b>Media Server</b> version %2 at %3.").arg(server->getVersion()).arg(QUrl(server->getUrl()).host()));
    }
    
    if (!ecsVersion.isEmpty() && !serverVersions.isEmpty())
        servers += serverVersions.join(QLatin1String("<br>\n"));

    QString credits = 
        tr(
            "<b>%1 %2</b> uses the following external libraries:<br/>\n"
            "<br />\n"
            "<b>Qt v.%3</b> - Copyright (c) 2012 Nokia Corporation.<br/>\n"
            "<b>FFMpeg %4</b> - Copyright (c) 2000-2012 FFmpeg developers.<br/>\n"
            "<b>Color Picker v2.6 Qt Solution</b> - Copyright (c) 2009 Nokia Corporation.<br/>\n"
            "<b>LAME 3.99.0</b> - Copyright (c) 1998-2012 LAME developers.<br/>\n"
            "<b>OpenAL %5</b> - Copyright (c) 2000-2006 %6.<br/>\n"
            "<b>SIGAR %7</b> - Copyright (c) 2004-2011 VMware Inc.<br/>\n"
            "<b>Boost %8</b> - Copyright (c) 2000-2012 Boost developers.<br/>\n"
        ).
        arg(QString::fromLatin1(QN_ORGANIZATION_NAME) + QLatin1String("(tm)")).
        arg(QString::fromLatin1(QN_APPLICATION_NAME)).
        arg(QString::fromLatin1(QT_VERSION_STR)).
        arg(versionString(QN_FFMPEG_VERSION)).
        arg(QtvAudioDevice::instance()->versionString()).
        arg(QtvAudioDevice::instance()->company()).
        arg(versionString(QN_SIGAR_VERSION)).
        arg(versionString(QN_BOOST_VERSION));

#ifndef Q_OS_DARWIN
    credits += tr("<b>Bespin style</b> - Copyright (c) 2007-2010 Thomas Luebking.<br/>");
#endif

    int maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize); // TODO: using opengl calls here is BAD. use estimate?

    QString gpu = 
        tr(
            "<b>OpenGL version</b>: %1.<br/>\n"
            "<b>OpenGL renderer</b>: %2.<br/>\n"
            "<b>OpenGL vendor</b>: %3.<br/>\n"
            "<b>OpenGL max texture size</b>: %4.<br/>\n"
        ).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VERSION)))).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)))). // TODO: same shit, OpenGL calls.
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VENDOR)))).
        arg(maxTextureSize);

    
    ui->versionLabel->setText(version);
    ui->creditsLabel->setText(credits);
    ui->gpuLabel->setText(gpu);
    ui->serversLabel->setText(servers);
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





