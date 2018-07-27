#include "server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <nx/utils/uuid.h>

#include <common/common_module.h>

#include <client/client_message_processor.h>

#include <transcoding/file_transcoder.h>

#include <ui/models/notification_sound_model.h>

#include <utils/media/audio_player.h>

namespace {

const QLatin1String folder("notifications");
const QLatin1String targetContainter("mp3");
const QLatin1String titleTag("Title");  // TODO: #GDM replace with database field

}

namespace nx {
namespace client {
namespace desktop {

ServerNotificationCache::ServerNotificationCache(QObject *parent) :
    base_type(folder, parent),
    m_model(new QnNotificationSoundModel(this))
{
    connect(qnClientMessageProcessor,   &QnClientMessageProcessor::fileAdded,                   this,   &ServerNotificationCache::at_fileAddedEvent);
    connect(qnClientMessageProcessor,   &QnClientMessageProcessor::fileUpdated,                 this,   &ServerNotificationCache::at_fileUpdatedEvent);
    connect(qnClientMessageProcessor,   &QnClientMessageProcessor::fileRemoved,                 this,   &ServerNotificationCache::at_fileRemovedEvent);
    connect(qnClientMessageProcessor,   &QnClientMessageProcessor::initialResourcesReceived,    this,   &ServerNotificationCache::getFileList);

    connect(this, &ServerFileCache::fileListReceived,  this,   &ServerNotificationCache::at_fileListReceived);
    connect(this, &ServerFileCache::fileDownloaded,    this,   &ServerNotificationCache::at_fileAdded);
    connect(this, &ServerFileCache::fileUploaded,      this,   &ServerNotificationCache::at_fileAdded);
    connect(this, &ServerFileCache::fileDeleted,       this,   &ServerNotificationCache::at_fileRemoved);
}

ServerNotificationCache::~ServerNotificationCache() {

}

QnNotificationSoundModel* ServerNotificationCache::persistentGuiModel() const {
    return m_model;
}

bool ServerNotificationCache::storeSound(const QString &filePath, int maxLengthMSecs, const QString &customTitle) {
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

bool ServerNotificationCache::updateTitle(const QString &filename, const QString &title) {
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

void ServerNotificationCache::clear() {
    base_type::clear();
    m_model->init();
}

void ServerNotificationCache::at_fileAddedEvent(const QString &filename) {
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

void ServerNotificationCache::at_fileUpdatedEvent(const QString &filename) {
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

void ServerNotificationCache::at_fileRemovedEvent(const QString &filename) {
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

void ServerNotificationCache::at_soundConverted(const QString &filePath) {
    if (!isConnectedToServer())
        return;

    QString filename = QFileInfo(filePath).fileName();
    m_model->addUploading(filename);
    uploadFile(filename);
}

void ServerNotificationCache::at_fileListReceived(const QStringList &filenames, OperationResult status) {
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    m_model->loadList(filenames);
    foreach (QString filename, filenames) {
        downloadFile(filename);
    }
}

void ServerNotificationCache::at_fileAdded(const QString &filename, OperationResult status) {
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    // TODO: #GDM #Business think about getTagValueAsync
    QString title = AudioPlayer::getTagValue(getFullPath(filename), titleTag);
    if (!title.isEmpty())
        m_model->updateTitle(filename, title);
}

void ServerNotificationCache::at_fileRemoved(const QString &filename, OperationResult status) {
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    int row = m_model->rowByFilename(filename);
    if (row > 0)
        m_model->removeRow(row);
}

} // namespace desktop
} // namespace client
} // namespace nx
