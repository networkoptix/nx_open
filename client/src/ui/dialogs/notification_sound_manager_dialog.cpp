#include "notification_sound_manager_dialog.h"
#include "ui_notification_sound_manager_dialog.h"

#include <QtCore/QFileInfo>
#include <QtGui/QFileDialog>

#include <utils/app_server_notification_cache.h>
#include <utils/media/audio_player.h>

QnNotificationSoundManagerDialog::QnNotificationSoundManagerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnNotificationSoundManagerDialog),
    m_cache(new QnAppServerNotificationCache(this)),
    m_loadingCounter(0),
    m_total(0)
{
    ui->setupUi(this);
    ui->removeButton->setVisible(false);

    connect(m_cache, SIGNAL(fileListReceived(QStringList,bool)), this, SLOT(at_fileListReceived(QStringList,bool)));
    connect(m_cache, SIGNAL(fileDownloaded(QString,bool)), this, SLOT(at_fileDownloaded(QString,bool)), Qt::QueuedConnection);
    connect(m_cache, SIGNAL(fileUploaded(QString,bool)), this, SLOT(at_fileUploaded(QString, bool)));

    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(at_playButton_clicked()));
    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(at_addButton_clicked()));

    m_cache->getFileList();
    m_loadingCounter = ui->progressBar->maximum();
    updateLoadingStatus();
}

QnNotificationSoundManagerDialog::~QnNotificationSoundManagerDialog()
{
}


void QnNotificationSoundManagerDialog::updateLoadingStatus() {
    int pageIndex = (m_loadingCounter == 0) ? 0 : 1;
    ui->stackedWidget->setCurrentIndex(pageIndex);

    if (m_loadingCounter > 0) {
        ui->statusLabel->setText(tr("Loading..."));
        ui->progressBar->setValue( (m_total - m_loadingCounter)*ui->progressBar->maximum() / m_total);
    }
}

void QnNotificationSoundManagerDialog::at_fileListReceived(const QStringList &filenames, bool ok) {
    if (!ok)
        //TODO: #GDM show warning message
        return;

    m_total = filenames.size();
    m_loadingCounter = m_total;
    updateLoadingStatus();

    foreach(QString filename, filenames) {
        m_cache->downloadFile(filename);
    }
}

void QnNotificationSoundManagerDialog::at_fileDownloaded(const QString &filename, bool ok) {
    m_loadingCounter--;
    updateLoadingStatus();

    if (!ok)
        //TODO: #GDM show warning message
        return;

    //TODO: #GDM extractMetadata
    QString title = filename;
    ui->listWidget->addItem(new QListWidgetItem(title));
}

void QnNotificationSoundManagerDialog::at_fileUploaded(const QString &filename, bool ok) {
    if (!ok)
        return;

    //TODO: #GDM extractMetadata
    QString title = filename;
    ui->listWidget->addItem(new QListWidgetItem(title));
}

void QnNotificationSoundManagerDialog::at_playButton_clicked() {
    if (!ui->listWidget->currentItem())
        return;

    QString soundUrl = ui->listWidget->currentItem()->text();
    QString filePath = m_cache->getFullPath(soundUrl);
    if (!QFileInfo(filePath).exists())
        return;

    AudioPlayer::playFileAsync(filePath);
}

void QnNotificationSoundManagerDialog::at_addButton_clicked() {

    QString supportedFormats = tr("Sound files");
    supportedFormats += QLatin1String(" (*.wav *.mp3 *.ogg *.wma)");

    QString filename = QFileDialog::getOpenFileName(
                this,
                //tr("Select file..."),
                QString(),
                QString(),
                supportedFormats);
    if (filename.isEmpty())
        return;
    m_cache->storeSound(filename);

}
