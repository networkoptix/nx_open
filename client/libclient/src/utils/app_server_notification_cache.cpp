#include "app_server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <nx/utils/uuid.h>

#include <client/client_message_processor.h>

#include <transcoding/file_transcoder.h>

#include <ui/models/notification_sound_model.h>

#include <utils/media/audio_player.h>

namespace {
    const QLatin1String folder("notifications");
    const QLatin1String targetContainter("mp3");
    const QLatin1String titleTag("Title");  //TODO: #GDM replace with database field
}

QnAppServerNotificationCache::QnAppServerNotificationCache(QObject *parent) :
    base_type(folder, parent),
    m_model(new QnNotificationSoundModel(this))
{
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::fileAdded,                   this,   &QnAppServerNotificationCache::at_fileAddedEvent);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::fileUpdated,                 this,   &QnAppServerNotificationCache::at_fileUpdatedEvent);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::fileRemoved,                 this,   &QnAppServerNotificationCache::at_fileRemovedEvent);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::initialResourcesReceived,    this,   &QnAppServerNotificationCache::getFileList);

    connect(this, &QnAppServerFileCache::fileListReceived,  this,   &QnAppServerNotificationCache::at_fileListReceived);
    connect(this, &QnAppServerFileCache::fileDownloaded,    this,   &QnAppServerNotificationCache::at_fileAdded);
    connect(this, &QnAppServerFileCache::fileUploaded,      this,   &QnAppServerNotificationCache::at_fileAdded);
    connect(this, &QnAppServerFileCache::fileDeleted,       this,   &QnAppServerNotificationCache::at_fileRemoved);
}

QnAppServerNotificationCache::~QnAppServerNotificationCache() {

}

QnNotificationSoundModel* QnAppServerNotificationCache::persistentGuiModel() const {
    return m_model;
}

bool QnAppServerNotificationCache::storeSound(const QString &filePath, int maxLengthMSecs, const QString &customTitle) {
    if (!isConnectedToServer())
        return false;

    QString uuid = QnUuid::createUuid().toString();
    QString newFilename = uuid.mid(1, uuid.size() - 2) + QLatin1String(".mp3");

    QString title = customTitle;
    if (title.isEmpty())
        title = AudioPlayer::getTagValue(filePath, titleTag);
    if (title.isEmpty()) {
        title = QFileInfo(filePath).fileName();
        title = title.mid(0, title.lastIndexOf(QLatin1Char('.')));
    }

    ensureCacheFolder();
    FileTranscoder* transcoder = new FileTranscoder();
    transcoder->setSourceFile(filePath);
    transcoder->setDestFile(getFullPath(newFilename));
    transcoder->setContainer(targetContainter);
    transcoder->setAudioCodec(AV_CODEC_ID_MP3);
    transcoder->addTag(titleTag, title);

    if (maxLengthMSecs > 0)
        transcoder->setTranscodeDurationLimit(maxLengthMSecs);

    connect(transcoder, SIGNAL(done(QString)), this, SLOT(at_soundConverted(QString)));
    connect(transcoder, SIGNAL(done(QString)), transcoder, SLOT(deleteLater()));
    return transcoder->startAsync();
}

bool QnAppServerNotificationCache::updateTitle(const QString &filename, const QString &title) {
    if (!isConnectedToServer())
        return false;

    bool result = FileTranscoder::setTagValue( getFullPath(filename), titleTag, title );
    if (result) {
        m_model->updateTitle(filename, title);
        if (m_updatingFiles.contains(filename))
            m_updatingFiles[filename] += 1;
        else
            m_updatingFiles[filename] = 1;
        uploadFile(filename);
    }
    return result;
}

void QnAppServerNotificationCache::clear() {
    base_type::clear();
    m_model->init();
}

void QnAppServerNotificationCache::at_fileAddedEvent(const QString &filename) {
    if (!isConnectedToServer())
        return;

    if (!filename.startsWith(folder))
        return;

    QString localFilename = filename.mid(QString(folder).size() + 1);

    // will do nothing if we have added this file
    if (m_model->rowByFilename(localFilename) > 0)
        return;

    m_model->addDownloading(localFilename);
    downloadFile(localFilename);
}

void QnAppServerNotificationCache::at_fileUpdatedEvent(const QString &filename) {
    if (!isConnectedToServer())
        return;

    if (!filename.startsWith(folder))
        return;

    QString localFilename = filename.mid(QString(folder).size() + 1);
    if (m_updatingFiles.contains(localFilename)) {
        m_updatingFiles[localFilename] -= 1;
        if (m_updatingFiles[localFilename] <= 0)
            m_updatingFiles.remove(localFilename);
        return;
    }

    QFile::remove(getFullPath(localFilename));
    downloadFile(localFilename);
}

void QnAppServerNotificationCache::at_fileRemovedEvent(const QString &filename) {
    if (!isConnectedToServer())
        return;

    if (!filename.startsWith(folder))
        return;

    QString localFilename = filename.mid(QString(folder).size() + 1);

    // double removing is quite safe
    QFile::remove(getFullPath(localFilename));
    int row = m_model->rowByFilename(localFilename);
    if (row > 0)
        m_model->removeRow(row);
}

void QnAppServerNotificationCache::at_soundConverted(const QString &filePath) {
    if (!isConnectedToServer())
        return;

    QString filename = QFileInfo(filePath).fileName();
    m_model->addUploading(filename);
    uploadFile(filename);
}

void QnAppServerNotificationCache::at_fileListReceived(const QStringList &filenames, OperationResult status) {
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    m_model->loadList(filenames);
    foreach (QString filename, filenames) {
        downloadFile(filename);
    }
}

void QnAppServerNotificationCache::at_fileAdded(const QString &filename, OperationResult status) {
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    //TODO: #GDM #Business think about getTagValueAsync
    QString title = AudioPlayer::getTagValue(getFullPath(filename), titleTag);
    if (!title.isEmpty())
        m_model->updateTitle(filename, title);
}

void QnAppServerNotificationCache::at_fileRemoved(const QString &filename, OperationResult status) {
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    int row = m_model->rowByFilename(filename);
    if (row > 0)
        m_model->removeRow(row);
}
