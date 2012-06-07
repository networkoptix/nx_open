#include "youtubesettingswidget.h"
#include "ui_youtubesetting.h"

#include <QtCore/QSettings>

#include "youtube_uploader.h"

QnYouTubeSettingsWidget::QnYouTubeSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::YouTubeSettings)
{
    ui->setupUi(this);

    QSettings settings;
    settings.beginGroup(QLatin1String("youtube"));
    ui->loginLineEdit->setText(settings.value(QLatin1String("login")).toString());
    ui->passwordLineEdit->setText(QString::fromAscii(QByteArray::fromBase64(settings.value(QLatin1String("password")).toByteArray())));
    settings.endGroup();

    connect(ui->loginLineEdit, SIGNAL(textChanged(QString)), this, SLOT(authPairChanged()));
    connect(ui->passwordLineEdit, SIGNAL(textChanged(QString)), this, SLOT(authPairChanged()));
    connect(ui->tryAuthButton, SIGNAL(clicked()), this, SLOT(tryAuth()));

    authPairChanged(); // reset
}

QnYouTubeSettingsWidget::~QnYouTubeSettingsWidget()
{
    delete ui;
}

QString QnYouTubeSettingsWidget::login()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("youtube"));

    return settings.value(QLatin1String("login")).toString();
}

QString QnYouTubeSettingsWidget::password()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("youtube"));

    return QString::fromAscii(QByteArray::fromBase64(settings.value(QLatin1String("password")).toByteArray()));
}

void QnYouTubeSettingsWidget::accept()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("youtube"));

    settings.setValue(QLatin1String("login"), ui->loginLineEdit->text());
    settings.setValue(QLatin1String("password"), ui->passwordLineEdit->text().toAscii().toBase64());

    settings.endGroup();
}

void QnYouTubeSettingsWidget::authPairChanged()
{
    ui->tryAuthButton->setEnabled(!ui->loginLineEdit->text().isEmpty() && !ui->passwordLineEdit->text().isEmpty());
    ui->authResultLabel->setText(QString());
}

void QnYouTubeSettingsWidget::tryAuth()
{
    ui->tryAuthButton->setEnabled(false);
    ui->authResultLabel->setText(QString());

    YouTubeUploader *youtubeuploader = new YouTubeUploader(this);
    youtubeuploader->setLogin(ui->loginLineEdit->text());
    youtubeuploader->setPassword(ui->passwordLineEdit->text());

    connect(youtubeuploader, SIGNAL(authFailed()), this, SLOT(authFailed()));
    connect(youtubeuploader, SIGNAL(authFinished()), this, SLOT(authFinished()));

    youtubeuploader->tryAuth();
}

void QnYouTubeSettingsWidget::authFailed()
{
    ui->authResultLabel->setText(tr("Authentication Failed!"));
    ui->tryAuthButton->setEnabled(true);

    sender()->deleteLater();
}

void QnYouTubeSettingsWidget::authFinished()
{
    ui->authResultLabel->setText(tr("Authentication Succeeded"));
    ui->tryAuthButton->setEnabled(true);

    sender()->deleteLater();
}
