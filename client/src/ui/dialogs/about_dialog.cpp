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

    setWindowTitle(tr("About %1").arg(QString::fromLatin1(APPLICATION_NAME)));

    QString version = 
        tr("<b>%1</b> Version: %2 (%3)").
        arg(QString::fromLatin1(APPLICATION_NAME)).
        arg(QString::fromLatin1(APPLICATION_VERSION)).
        arg(QString::fromLatin1(APPLICATION_REVISION));

    QString credits = 
        tr(
            "<b>%1 %2</b> uses the following external libraries:<br />\n"
            "<br />\n"
            "<b>Qt v.%3</b> - Copyright (c) 2012 Nokia Corporation.<br />\n"
            "<b>FFMpeg %4</b> - Copyright (c) 2000-2012 the FFmpeg developers.<br />\n"
            "<b>Color Picker v2.6 Qt Solution</b> - Copyright (c) 2009 Nokia Corporation.<br />"
            "<b>LAME 3.99.0</b> - Copyright (c) 1998-2012 the LAME developers.<br />"
            "<b>OpenAL %5</b> - Copyright (c) 2000-2006 %6<br />"
        ).
        arg(QString::fromLatin1(ORGANIZATION_NAME) + QLatin1String("(tm)")).
        arg(QString::fromLatin1(APPLICATION_NAME)).
        arg(QString::fromLatin1(QT_VERSION_STR)).
        arg(QString::fromLatin1(FFMPEG_VERSION)).
        arg(QtvAudioDevice::instance()->versionString()).
        arg(QtvAudioDevice::instance()->company());

#ifndef Q_OS_DARWIN
    credits += tr("<b>Bespin style</b> - Copyright (c) 2007-2010 Thomas Luebking.<br />");
#endif

    int maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    QString gpu = 
        tr(
            "<b>OpenGL version</b>: %1.<br />"
            "<b>OpenGL renderer</b>: %2.<br/>"
            "<b>OpenGL vendor</b>: %3.<br />"
            "<b>OpenGL max texture size</b>: %4.<br />"
        ).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_VERSION)))).
        arg(QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)))).
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





