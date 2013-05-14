#include "app_server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <transcoding/file_transcoder.h>

#include <ui/models/notification_sound_model.h>

#include <utils/media/audio_player.h>

namespace {
    const QLatin1String folder("notifications");
    const QLatin1String targetContainter("mp3");
    const QLatin1String titleTag("Title");
}

QnAppServerNotificationCache::QnAppServerNotificationCache(QObject *parent) :
    base_type(folder, parent),
    m_model(new QnNotificationSoundModel(this))
{
    connect(this, SIGNAL(fileListReceived(QStringList,bool)),   this, SLOT(at_fileListReceived(QStringList,bool)));
    connect(this, SIGNAL(fileDownloaded(QString,bool)),         this, SLOT(at_fileAdded(QString,bool)));
    connect(this, SIGNAL(fileUploaded(QString,bool)),           this, SLOT(at_fileAdded(QString, bool)));

    getFileList();
}

QnAppServerNotificationCache::~QnAppServerNotificationCache() {

}

QnNotificationSoundModel* QnAppServerNotificationCache::persistentGuiModel() const {
    return m_model;
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
    transcoder->addTag(titleTag, title);

    if (maxLengthMSecs > 0)
        transcoder->setTranscodeDurationLimit(maxLengthMSecs);

    connect(transcoder, SIGNAL(done(QString)), this, SLOT(at_soundConverted(QString)));
    connect(transcoder, SIGNAL(done(QString)), transcoder, SLOT(deleteLater()));
    transcoder->startAsync();
}

void QnAppServerNotificationCache::at_soundConverted(const QString &filePath) {
    QString filename = QFileInfo(filePath).fileName();
    m_model->addUploading(filename);
    uploadFile(filename);
}

void QnAppServerNotificationCache::at_fileListReceived(const QStringList &filenames, bool ok) {
    if (!ok)
        return;

    m_model->loadList(filenames);
    foreach (QString filename, filenames) {
        downloadFile(filename);
    }
}

void QnAppServerNotificationCache::at_fileAdded(const QString &filename, bool ok) {
    if (!ok)
        return;

    //TODO: #GDM think about getTagValueAsync
    QString title = AudioPlayer::getTagValue(getFullPath(filename), titleTag);
    if (!title.isEmpty())
        m_model->updateTitle(filename, title);
}

