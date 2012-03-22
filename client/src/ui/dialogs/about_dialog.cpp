#include "about_dialog.h"
#include "ui_about_dialog.h"

#include <QtCore/QEvent>

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>

#include <ui/graphics/opengl/gl_functions.h>

#include "version.h"

QnAboutDialog::QnAboutDialog(QWidget *parent): 
    QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::AboutDialog())
{
    ui->setupUi(this);

    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

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
            "<b>Color Picker v2.6 Qt Solution</b> - Copyright (c) 2009 Nokia Corporation."
        ).
        arg(QString::fromLatin1(ORGANIZATION_NAME) + QLatin1String("(tm)")).
        arg(QString::fromLatin1(APPLICATION_NAME)).
        arg(QString::fromLatin1(QT_VERSION_STR)).
        arg(QString::fromLatin1(FFMPEG_VERSION));

#ifndef Q_OS_DARWIN
    credits += tr("<br /><b>Bespin style</b> - Copyright (c) 2007-2010 Thomas Luebking.");
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
