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
    QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::AboutDialog())
{
    ui->setupUi(this);

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
        tr("<b>%1</b> version %2 (%3)").
        arg(QLatin1String(QN_APPLICATION_NAME)).
        arg(QLatin1String(QN_APPLICATION_VERSION)).
        arg(QLatin1String(QN_APPLICATION_REVISION));

    QString credits = 
        tr(
            "<b>%1 %2</b> uses the following external libraries:<br />\n"
            "<br />\n"
            "<b>Qt v.%3</b> - Copyright (c) 2012 Nokia Corporation.<br />\n"
            "<b>FFMpeg %4</b> - Copyright (c) 2000-2012 FFmpeg developers.<br />\n"
            "<b>Color Picker v2.6 Qt Solution</b> - Copyright (c) 2009 Nokia Corporation.<br />\n"
            "<b>LAME 3.99.0</b> - Copyright (c) 1998-2012 LAME developers.<br />\n"
            "<b>OpenAL %5</b> - Copyright (c) 2000-2006 %6<br />\n"
            "<b>SIGAR %7</b> - Copyright (c) 2004-2011 VMware Inc.<br />\n"
            "<b>Boost %8</b> - Copyright (c) 2000-2012 Boost developers.<br />\n"
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
    credits += tr("<b>Bespin style</b> - Copyright (c) 2007-2010 Thomas Luebking.<br />");
#endif

    int maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize); // TODO: using opengl calls here is BAD. use estimate?

    QString gpu = 
        tr(
            "<b>OpenGL version</b>: %1.<br />"
            "<b>OpenGL renderer</b>: %2.<br/>"
            "<b>OpenGL vendor</b>: %3.<br />"
            "<b>OpenGL max texture size</b>: %4.<br />"
        ).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VERSION)))).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)))). // TODO: same shit, OpenGL calls.
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VENDOR)))).
        arg(maxTextureSize);

    
    ui->versionLabel->setText(version);
    ui->creditsLabel->setText(credits);
    ui->gpuLabel->setText(gpu);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnAboutDialog::at_copyButton_clicked() {
    QClipboard *clipboard = QApplication::clipboard();

    clipboard->setText(
         QTextDocumentFragment::fromHtml(ui->versionLabel->text()).toPlainText() + 
         QLatin1String("\n") +
         QTextDocumentFragment::fromHtml(ui->gpuLabel->text()).toPlainText()
    );
}





