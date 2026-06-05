// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nov_media_export.h"

#include <core/resource/camera_resource.h>
#include <core/resource/storage_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <export/sign_helper.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/export/tools/export_utils.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/timezone_helper.h>
#include <nx/vms/common/system_settings.h>
#include <recording/helpers/recording_context_helpers.h>

namespace nx::vms::client::desktop {

NovMediaExport::NovMediaExport(
    const QnMediaResourcePtr& resource,
    QnAbstractStreamDataProvider* mediaProvider,
    const QnStorageResourcePtr& storage,
    const QString& filename,
    int64_t startTimeUs,
    int64_t endTimeUs)
    :
    QnStreamRecorder(resource),
    nx::StorageRecordingContext(/*exportMode*/ true),
    m_baseFileName(filename),
    m_storage(storage),
    m_mediaProvider(mediaProvider),
    m_timeZone(timeZone(resource))
{
    setContainer("mkv");
    setProgressBounds(startTimeUs, endTimeUs);
}

NovMediaExport::~NovMediaExport()
{
    stop();
}

void NovMediaExport::adjustMetaData(QnAviArchiveMetadata& metaData) const
{
    metaData.signature = QnSignHelper::addSignatureFiller(QnSignHelper::getSignMagic());

    if (NX_ASSERT(m_timeZone.isValid()))
    {
        // Store timezone offset for playback compatibility with older clients.
        metaData.timeZoneOffset =
            std::chrono::seconds(m_timeZone.offsetFromUtc(QDateTime::currentDateTime()));
        metaData.timeZoneId = m_timeZone.id();
    }
}

void NovMediaExport::onSuccessfulPacketWrite(
    AVCodecParameters* avCodecParams, const uint8_t* data, int size)
{
    m_signer.processMedia(avCodecParams, data, size);
}

qint64 NovMediaExport::getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md)
{
    return md->timestamp - startTimeUs();
}

void NovMediaExport::initializeRecordingContext(int64_t startTimeUs)
{
    NX_ASSERT(m_recordingContext.isEmpty());
    int64_t startTimeMs = startTimeUs / 1000;
    auto filename = m_baseFileName + "_" + QString::number(startTimeMs);
    m_startTimestamps.push_back(startTimeMs);
    m_recordingContext = StorageContext(filename, m_storage);
    NX_DEBUG(this, "Export to new file: %1", filename);
    StorageRecordingContext::initializeRecordingContext(startTimeUs);

    // For metadata use first file only
    if (m_metadataFileName.isEmpty())
        m_metadataFileName = m_recordingContext.fileName;
}

bool NovMediaExport::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (!motion)
    {
        NX_WARNING(this, "Empty motion data");
        return false;
    }

    int channel = motion->channelNumber;
    if (channel >= CL_MAX_CHANNELS)
    {
        NX_WARNING(this, "Invalid motion data: %1", motion);
        return false;
    }

    if (!motion->isEmpty())
    {
        if (!m_motionFileList[channel])
        {
            m_motionFileList[channel].reset(new QBuffer());
            m_motionFileList[channel]->open(QIODevice::ReadWrite);
        }

        NX_VERBOSE(this, "Saving motion, timestamp %1 us", motion->timestamp);
        motion->serialize(m_motionFileList[channel].get());
    }
    return true;
}

bool NovMediaExport::writeFile(const QString& path, const QByteArray& data)
{
    NX_DEBUG(this, "Creating file: %1", path);
    std::unique_ptr<QIODevice> file(m_storage->open(path, QIODevice::WriteOnly));
    if (!file)
    {
        NX_WARNING(this, "Failed to create file: %1", path);
        setLastError(nx::recording::Error::Code::fileCreate);
        return false;
    }
    if (file->write(data) == -1)
    {
        NX_WARNING(this, "Failed to write to file: %1", path);
        setLastError(nx::recording::Error::Code::fileWrite);
        return false;
    }
    return true;
}

void NovMediaExport::reportFinished()
{
    if (m_finishAllFiles)
        emit recordingFinished(getLastError(), m_recordingContext.fileName);
}

void NovMediaExport::writeMotionData()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_motionFileList[i])
        {
            m_motionFileList[i]->close();
            QString path = NX_FMT("motion%1_%2.bin", i, m_baseFileName);
            if (!writeFile(path, m_motionFileList[i]->buffer()))
                return;
        }
    }

    if (!m_startTimestamps.empty())
    {
        QString path = NX_FMT("start_times_%1.txt", m_baseFileName);
        QByteArray buffer;
        for (const auto& time: m_startTimestamps)
            buffer.append(QByteArray::number(time) + " ");

        if (!writeFile(path, buffer))
            return;
    }
}

bool NovMediaExport::saveData(const QnConstAbstractMediaDataPtr& md)
{
    NX_VERBOSE(this, "Export media data: %1", md);
    setHasVideo(m_mediaDevice->hasVideo(m_mediaProvider));

    if (md->dataType == QnAbstractMediaData::VIDEO)
    {
        if (!m_videoCodecParameters
            || (md->context && !m_videoCodecParameters->isEqual(*md->context)))
        {
            NX_DEBUG(this, "New video codec parameters: %1", md->context->toString());
            setVideoLayout(m_mediaDevice->getVideoLayout(m_mediaProvider));
            if (m_fileOpened)
            {
                NX_DEBUG(this, "Video codec parameters have changed. Close file");
                close();
            }
        }
        m_videoCodecParameters = md->context;
    }
    else if (md->dataType == QnAbstractMediaData::AUDIO)
    {
        if (!m_audioCodecParameters
            || (md->context && !m_audioCodecParameters->isEqual(*md->context)))
        {
            NX_DEBUG(this, "New audio codec parameters: %1", md->context->toString());
            setAudioLayout(m_mediaProvider->getAudioLayout());
            if (m_fileOpened)
            {
                NX_DEBUG(this, "Audio codec parameters have changed. Close file");
                close();
            }
        }
        m_audioCodecParameters = md->context;
    }
    else if (auto metadata = std::dynamic_pointer_cast<const QnCompressedMetadata>(md))
    {
        if (metadata->metadataType == MetadataType::MediaStreamEvent)
        {
            auto packet = metadata->toMediaEventPacket();
            if (packet.code != nx::media::StreamEvent::noEvent)
            {
                setLastError(toRecordingError(packet.code));
                return false;
            }
        }
    }

    return QnStreamRecorder::saveData(md);
}

void NovMediaExport::endOfRun()
{
    m_finishAllFiles = true;
    close();
    const auto systemContext = SystemContext::fromResource(m_resource);
    auto signature = m_signer.buildSignature(
        systemContext->licensePool(), systemContext->currentServerId());
    if (m_recordingContext.storage) //< Is file created.
    {
        if (!updateSignatureInFile(
            m_recordingContext.storage, m_metadataFileName, m_recordingContext.fileFormat, signature))
        {
            NX_WARNING(this, "SignVideo: metadata tag was not updated");
        }
    }
    writeMotionData();
}

void NovMediaExport::setLastError(nx::recording::Error::Code code)
{
    m_finishAllFiles = true;
    nx::StorageRecordingContext::setLastError(code);
    close();
    m_needStop = true;
}

std::optional<nx::recording::Error> NovMediaExport::getLastError() const
{
    QnArchiveStreamReader* streamReader = dynamic_cast<QnArchiveStreamReader*>(m_mediaProvider);
    if (streamReader
        && streamReader->lastError().errorCode == CameraDiagnostics::ErrorCode::serverTerminated)
    {
        return nx::recording::Error::Code::temporaryUnavailable;
    }
    return nx::StorageRecordingContext::getLastError();
}

} // namespace nx::vms::client::desktop
