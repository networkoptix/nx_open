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
#include <nx/vms/common/system_settings.h>
#include <recording/helpers/recording_context_helpers.h>

namespace nx::vms::client::desktop {

NovMediaExport::NovMediaExport(
    const QnResourcePtr& dev, QnAbstractStreamDataProvider* mediaProvider):
    QnStreamRecorder(dev),
    nx::StorageRecordingContext(true),
    m_mediaProvider(mediaProvider)
{
}

NovMediaExport::~NovMediaExport()
{
    stop();
}

void NovMediaExport::setServerTimeZone(QTimeZone value)
{
    m_timeZone = std::move(value);
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

void NovMediaExport::setStorage(const QString& fileName, const QnStorageResourcePtr& storage)
{
    m_baseFileName = fileName;
    m_storage = storage;
}

void NovMediaExport::setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS])
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionFileList[i] = motionFileList[i];
}

bool NovMediaExport::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (motion && !motion->isEmpty() && m_motionFileList[motion->channelNumber])
    {
        NX_VERBOSE(this,
            "Saving motion, timestamp %1 us, resource: %2",
            motion->timestamp, m_resource);
        motion->serialize(m_motionFileList[motion->channelNumber].data());
    }

    return true;
}

void NovMediaExport::reportFinished()
{
    if (m_finishAllFiles)
    {
        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        {
            if (m_motionFileList[i])
                m_motionFileList[i]->close();
        }
        emit recordingFinished(getLastError(), m_recordingContext.fileName);
    }
}

bool NovMediaExport::saveData(const QnConstAbstractMediaDataPtr& md)
{
    NX_VERBOSE(this, "Export media data: %1", md);
    if (!m_isLayoutsInitialised)
    {
        m_isLayoutsInitialised = true;
        setVideoLayout(m_mediaDevice->getVideoLayout(m_mediaProvider));
        setAudioLayout(m_mediaDevice->getAudioLayout(m_mediaProvider));
        setHasVideo(m_mediaDevice->hasVideo(m_mediaProvider));
    }

    if (md->dataType == QnAbstractMediaData::VIDEO)
    {
        if (m_videoCodecParameters && md->context && !m_videoCodecParameters->isEqual(*md->context))
        {
            NX_DEBUG(this, "Video codec parameters have changed from %1 to %2. Close file",
                m_videoCodecParameters->toString(), md->context->toString());
            close();
        }
        m_videoCodecParameters = md->context;
    }
    else if (md->dataType == QnAbstractMediaData::AUDIO)
    {
        if (m_audioCodecParameters && md->context && !m_audioCodecParameters->isEqual(*md->context))
        {
            NX_DEBUG(this, "Audio codec parameters have changed from %1 to %2. Close file",
                m_audioCodecParameters->toString(), md->context->toString());
            close();
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
