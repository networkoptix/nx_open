#include "app_server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <transcoding/file_transcoder.h>
#include <utils/media/audio_player.h>

namespace {
    const QLatin1String folder("notifications");
    const QLatin1String targetContainter("mp3");
    const QLatin1String titleTagValue("Comment");
}

QnAppServerNotificationCache::QnAppServerNotificationCache(QObject *parent) :
    base_type(folder, parent),
    m_model(new QStandardItemModel(this))
{
    connect(this, SIGNAL(fileListReceived(QStringList,bool)),   this, SLOT(at_fileListReceived(QStringList,bool)));
    connect(this, SIGNAL(fileDownloaded(QString,bool)),         this, SLOT(at_fileAdded(QString,bool)));
    connect(this, SIGNAL(fileUploaded(QString,bool)),           this, SLOT(at_fileAdded(QString, bool)));

    getFileList();
}

QnAppServerNotificationCache::~QnAppServerNotificationCache() {

}

QStandardItemModel* QnAppServerNotificationCache::persistentGuiModel() const {
    return m_model;
}

QString QnAppServerNotificationCache::titleTag() {
    return titleTagValue;
}

void QnAppServerNotificationCache::storeSound(const QString &filePath, int maxLengthMSecs) {
    QString uuid = QUuid::createUuid().toString();
    QString newFilename = uuid.mid(1, uuid.size() - 2) + QLatin1String(".mp3");
    QString title = QFileInfo(filePath).fileName();
    title = title.mid(0, title.lastIndexOf(QLatin1Char('.')));

    FileTranscoder* transcoder = new FileTranscoder();
    transcoder->setSourceFile(filePath);
    transcoder->setDestFile(getFullPath(newFilename));
    transcoder->setContainer(targetContainter);
    transcoder->setAudioCodec(CODEC_ID_MP3);
    transcoder->addTag(titleTag(), title);

    if (maxLengthMSecs > 0)
        transcoder->setTranscodeDurationLimit(maxLengthMSecs);

    connect(transcoder, SIGNAL(done(QString)), this, SLOT(at_soundConverted(QString)));
    connect(transcoder, SIGNAL(done(QString)), transcoder, SLOT(deleteLater()));
    transcoder->startAsync();
}

void QnAppServerNotificationCache::at_soundConverted(const QString &filePath) {
    uploadFile(QFileInfo(filePath).fileName());
}

void QnAppServerNotificationCache::at_fileListReceived(const QStringList &filenames, bool ok) {
    if (!ok)
        return;

    m_model->clear();
    foreach (QString filename, filenames) {
        downloadFile(filename);
    }
}

void QnAppServerNotificationCache::at_fileAdded(const QString &filename, bool ok) {
    if (!ok)
        return;

    QString title = AudioPlayer::getTagValue(getFullPath(filename), titleTagValue);
    if (title.isEmpty())
        title = filename;

    QStandardItem* item = new QStandardItem(title);
    item->setData(filename);
    m_model->appendRow(item);
}

