// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <client/client_message_processor.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/file_cache/file_cache_utils.h>
#include <transcoding/file_transcoder.h>
#include <ui/models/notification_sound_model.h>
#include <utils/media/audio_player.h>

namespace {

constexpr char kFolder[] = "notifications";
constexpr char kTargetContainer[] = "mp3";
constexpr char kTitleTag[] = "Title";  // TODO: #sivanov Replace with database field.

QString getUnsafeLocalFilename(const QString& storedFilePath)
{
    return storedFilePath.mid(QString(kFolder).size() + 1);
}

} // namespace

namespace nx::vms::client::desktop {

ServerNotificationCache::ServerNotificationCache(SystemContext* systemContext, QObject* parent):
    base_type(systemContext, kFolder, parent),
    m_model(new QnNotificationSoundModel(this))
{
    connect(this, &ServerFileCache::fileListReceived,
        this, &ServerNotificationCache::at_fileListReceived);
    connect(this, &ServerFileCache::fileDownloaded,
        this, &ServerNotificationCache::at_fileAdded);
    connect(this, &ServerFileCache::fileUploaded, this,
        [this](const QString& filename, OperationResult status)
        {
            // Upload signal has no path; resolve it from the cached filename.
            at_fileAdded(filename, status, absoluteFilePath(filename));
        });
    connect(this, &ServerFileCache::fileDeleted,
        this, &ServerNotificationCache::at_fileRemoved);
}

ServerNotificationCache::~ServerNotificationCache()
{
}

void ServerNotificationCache::setMessageProcessor(QnClientMessageProcessor* messageProcessor)
{
    if (messageProcessor)
    {
        connect(messageProcessor, &QnClientMessageProcessor::fileAdded, this,
            &ServerNotificationCache::at_fileAddedEvent);
        connect(messageProcessor, &QnClientMessageProcessor::fileUpdated, this,
            &ServerNotificationCache::at_fileUpdatedEvent);
        connect(messageProcessor, &QnClientMessageProcessor::fileRemoved, this,
            &ServerNotificationCache::at_fileRemovedEvent);
        connect(messageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
            &ServerNotificationCache::getFileList);
    }
}

QnNotificationSoundModel* ServerNotificationCache::persistentGuiModel() const
{
    return m_model;
}

bool ServerNotificationCache::storeSound(
    const QString& filePath, int maxLengthMSecs, const QString& customTitle)
{
    if (!isConnectedToServer())
        return false;

    const auto newFilename =
        nx::Uuid::createUuid().toSimpleString().append('.').append(kTargetContainer);

    QString title = customTitle;
    if (title.isEmpty())
        title = AudioPlayer::getTagValue(filePath, kTitleTag);
    if (title.isEmpty())
    {
        title = QFileInfo(filePath).fileName();
        title = title.mid(0, title.lastIndexOf(QLatin1Char('.')));
    }

    ensureCacheFolder();
    const auto destPath = this->absoluteFilePath(newFilename);
    if (destPath.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", newFilename);
        return false;
    }

    FileTranscoder* transcoder = new FileTranscoder(systemContext()->metrics());
    transcoder->setSourceFile(filePath);
    transcoder->setDestFile(destPath);
    transcoder->setContainer(kTargetContainer);
    transcoder->setAudioCodec(AV_CODEC_ID_MP3);
    transcoder->addTag(kTitleTag, title);

    if (maxLengthMSecs > 0)
        transcoder->setTranscodeDurationLimit(maxLengthMSecs);

    connect(transcoder, &FileTranscoder::done, this, &ServerNotificationCache::at_soundConverted);
    connect(transcoder, &FileTranscoder::done, transcoder, &FileTranscoder::deleteLater);
    return transcoder->startAsync();
}

bool ServerNotificationCache::updateTitle(const QString& unsafeFilename, const QString& title)
{
    if (!isConnectedToServer())
        return false;

    const auto safeFilename = file_cache::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", unsafeFilename);
        return false;
    }

    const auto safeFilePath = absoluteFilePath(safeFilename);
    if (!NX_ASSERT(!safeFilePath.isEmpty(),
        "Sanitized file name should always produce a path: %1", unsafeFilename))
    {
        return false;
    }

    const bool result = FileTranscoder::setTagValue(
        systemContext()->metrics(),
        safeFilePath,
        kTitleTag,
        title);

    if (result)
    {
        m_model->updateTitle(safeFilename, title);
        if (m_updatingFiles.contains(safeFilename))
            m_updatingFiles[safeFilename] += 1;
        else
            m_updatingFiles[safeFilename] = 1;
        uploadFile(safeFilename);
    }
    return result;
}

void ServerNotificationCache::clear()
{
    base_type::clear();
    m_model->init();
}

void ServerNotificationCache::at_fileAddedEvent(const QString& storedFilePath)
{
    if (!isConnectedToServer())
        return;

    if (!storedFilePath.startsWith(kFolder))
        return;

    const auto unsafeLocalFilename = getUnsafeLocalFilename(storedFilePath);
    const auto safeFilename = file_cache::sanitizeFilename(unsafeLocalFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", storedFilePath);
        return;
    }

    // will do nothing if we have added this file
    if (m_model->rowByFilename(safeFilename) > 0)
        return;

    m_model->addDownloading(safeFilename);
    downloadFile(safeFilename);
}

void ServerNotificationCache::at_fileUpdatedEvent(const QString& storedFilePath)
{
    if (!isConnectedToServer())
        return;

    if (!storedFilePath.startsWith(kFolder))
        return;

    const auto unsafeLocalFilename = getUnsafeLocalFilename(storedFilePath);
    const auto safeFilename = file_cache::sanitizeFilename(unsafeLocalFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", storedFilePath);
        return;
    }

    const auto path = absoluteFilePath(safeFilename);
    if (!NX_ASSERT(!path.isEmpty(),
        "Sanitized file name should always produce a path: %1", unsafeLocalFilename))
    {
        return;
    }

    if (m_updatingFiles.contains(safeFilename))
    {
        m_updatingFiles[safeFilename] -= 1;
        if (m_updatingFiles[safeFilename] <= 0)
            m_updatingFiles.remove(safeFilename);
        return;
    }

    QFile::remove(path);
    downloadFile(safeFilename);
}

void ServerNotificationCache::at_fileRemovedEvent(const QString& storedFilePath)
{
    if (!isConnectedToServer())
        return;

    if (!storedFilePath.startsWith(kFolder))
        return;

    const auto unsafeLocalFilename = getUnsafeLocalFilename(storedFilePath);
    const auto safeFilename = file_cache::sanitizeFilename(unsafeLocalFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", storedFilePath);
        return;
    }

    const auto path = absoluteFilePath(safeFilename);
    if (!NX_ASSERT(!path.isEmpty(),
        "Sanitized file name should always produce a path: %1", unsafeLocalFilename))
    {
        return;
    }

    // Double removing is quite safe.
    QFile::remove(path);
    int row = m_model->rowByFilename(safeFilename);
    if (row > 0)
        m_model->removeRow(row);
}

void ServerNotificationCache::at_soundConverted(const QString& convertedPath)
{
    if (!isConnectedToServer())
        return;

    QString filename = QFileInfo(convertedPath).fileName();
    m_model->addUploading(filename);
    uploadFile(filename);
}

void ServerNotificationCache::at_fileListReceived(const QStringList& filenames, OperationResult status)
{
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    m_model->loadList(filenames);
    foreach (QString filename, filenames)
    {
        downloadFile(filename);
    }
}

void ServerNotificationCache::at_fileAdded(
    const QString& filename, OperationResult status, const QString& absolutePath)
{
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    if (absolutePath.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", filename);
        return;
    }

    // TODO: #sivanov Think about getTagValueAsync.
    QString title = AudioPlayer::getTagValue(absolutePath, kTitleTag);
    if (!title.isEmpty())
        m_model->updateTitle(filename, title);
}

void ServerNotificationCache::at_fileRemoved(const QString& filename, OperationResult status)
{
    if (status != OperationResult::ok || !isConnectedToServer())
        return;

    int row = m_model->rowByFilename(filename);
    if (row > 0)
        m_model->removeRow(row);
}

} // namespace nx::vms::client::desktop
