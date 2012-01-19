#include "aboutdialog.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>

#include "version.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint)
{
    m_informationGroupBox = new QGroupBox(this);
    m_versionLabel = new QLabel(m_informationGroupBox);

    m_creditsGroupBox = new QGroupBox(this);
    m_creditsLabel = new QLabel(m_creditsGroupBox);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
    connect(buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SLOT(close()));


    QVBoxLayout *informationGroupBoxLayout = new QVBoxLayout;
    informationGroupBoxLayout->addWidget(m_versionLabel);
    m_informationGroupBox->setLayout(informationGroupBoxLayout);

    QVBoxLayout *creditsGroupBoxLayout = new QVBoxLayout;
    creditsGroupBoxLayout->addWidget(m_creditsLabel);
    m_creditsGroupBox->setLayout(creditsGroupBoxLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_informationGroupBox);
    mainLayout->addWidget(m_creditsGroupBox);
    mainLayout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    retranslateUi();
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void AboutDialog::retranslateUi()
{
    setWindowTitle(tr("About %1").arg(QString::fromLatin1(APPLICATION_NAME)));

    m_informationGroupBox->setTitle(tr("Information"));
    m_creditsGroupBox->setTitle(tr("Credits"));

    QString version = tr("<b>%1</b> Version: %2 (%3)")
                      .arg(QString::fromLatin1(APPLICATION_NAME))
                      .arg(QString::fromLatin1(APPLICATION_VERSION))
                      .arg(QString::fromLatin1(APPLICATION_REVISION));
    QString credits = tr("<b>%1 %2</b> uses the following external libraries:<br />\n"
                         "<br />\n"
                         "<b>Qt v.%3</b> - Copyright (c) 2012 the Nokia Corporation<br />\n"
                         "<b>FFMpeg %4</b> - Copyright (c) 2000-2012 the FFmpeg developers")
                      .arg(QString::fromLatin1(ORGANIZATION_NAME) + QLatin1String("(tm)"))
                      .arg(QString::fromLatin1(APPLICATION_NAME))
                      .arg(QString::fromLatin1(QT_VERSION_STR))
                      .arg(QString::fromLatin1(FFMPEG_VERSION));
#ifndef Q_OS_DARWIN
    //if (QApplication::style()->objectName() == QLatin1String("Bespin"))
    credits += tr("<br /><br /><b>Bespin style</b> - Copyright (C) 2007-2010 Thomas Luebking");
#endif

    m_versionLabel->setText(version);
    m_creditsLabel->setText(credits);
}
