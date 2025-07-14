// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_storage_stream_recorder.h"

#include <core/resource/camera_resource.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/storage_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <export/sign_helper.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <recording/helpers/recording_context_helpers.h>
#include <utils/common/util.h>
#include <transcoding/transcoding_utils.h>

namespace nx::vms::client::desktop {

using namespace nx::core::transcoding;

namespace {

// 16Kb ought to be enough for anybody.
static const int kMetadataSeekSizeBytes = 16 * 1024;

bool updateInFile(QIODevice* file,
    QnAviArchiveMetadata::Format fileFormat,
    const QByteArray& source,
    const QByteArray& target)
{
    NX_ASSERT(file);
    NX_ASSERT(source.size() == target.size());

    auto pos = -1;
    switch (fileFormat)
    {
        // Mp4 stores metadata at the end of file.
        case QnAviArchiveMetadata::Format::mp4:
        {
            const auto offset = file->size() - kMetadataSeekSizeBytes;
            file->seek(offset);
            const auto data = file->read(kMetadataSeekSizeBytes);
            pos = data.indexOf(source);
            if (pos >= 0)
                pos += offset;
            break;
        }
        default:
        {
            file->seek(0);
            const auto data = file->read(kMetadataSeekSizeBytes);
            pos = data.indexOf(source);
            break;
        }
    }

    if (pos < 0)
        return false;

    file->seek(pos);
    file->write(target);
    return true;
}

} // namespace

ExportStorageStreamRecorder::ExportStorageStreamRecorder(
    const QnResourcePtr& dev, QnAbstractStreamDataProvider* mediaProvider):
    QnStreamRecorder(dev),
    nx::StorageRecordingContext(true),
    m_mediaProvider(mediaProvider),
    m_timestampStitcher(std::chrono::milliseconds(MAX_FRAME_DURATION_MS), std::chrono::milliseconds(33))
{
}

ExportStorageStreamRecorder::~ExportStorageStreamRecorder()
{
    stop();
}

void ExportStorageStreamRecorder::setPreciseStartPosition(int64_t startTimeUs)
{
    m_preciseStartTimeUs = startTimeUs;
    if (isTranscodingEnabled())
        setPreciseStartDateTime(m_preciseStartTimeUs);
}

void ExportStorageStreamRecorder::setTranscodeFilters(const FilterChain& filters)
{
    m_transcodeFilters = filters;
}

void ExportStorageStreamRecorder::writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex)
{
    if (md->dataType == QnAbstractMediaData::AUDIO && m_audioTranscoder)
    {
        QnAbstractMediaDataPtr result;
        bool inputDataDepleted = false;
        do
        {
            m_audioTranscoder->transcodePacket(
                inputDataDepleted ? QnConstAbstractMediaDataPtr() : md, &result);

            if (result && result->dataSize() > 0)
                StorageRecordingContext::writeData(result, streamIndex);

            inputDataDepleted = true;
        } while (result);
    }
    else if (md->dataType == QnAbstractMediaData::VIDEO && m_videoTranscoder)
    {
        QnAbstractMediaDataPtr result;
        m_videoTranscoder->transcodePacket(md, &result);
        if (result && result->dataSize() > 0)
            StorageRecordingContext::writeData(result, streamIndex);
    }
    else
    {
        StorageRecordingContext::writeData(md, streamIndex);
    }
}

int ExportStorageStreamRecorder::getStreamIndex(const QnConstAbstractMediaDataPtr& mediaData) const
{
    if (mediaData->dataType == QnAbstractMediaData::VIDEO && m_videoTranscoder)
        return 0;

    return QnStreamRecorder::getStreamIndex(mediaData);
}

void ExportStorageStreamRecorder::setServerTimeZone(QTimeZone value)
{
    m_timeZone = std::move(value);
}

bool ExportStorageStreamRecorder::isTranscodingEnabled() const
{
    return m_transcodeFilters && m_transcodeFilters->isTranscodingRequired();
}

void ExportStorageStreamRecorder::adjustMetaData(QnAviArchiveMetadata& metaData) const
{
    metaData.signature = QnSignHelper::addSignatureFiller(QnSignHelper::getSignMagic());

    if (NX_ASSERT(m_timeZone.isValid()))
    {
        // Store timezone offset for playback compatibility with older clients.
        metaData.timeZoneOffset =
            std::chrono::seconds(m_timeZone.offsetFromUtc(QDateTime::currentDateTime()));
        metaData.timeZoneId = m_timeZone.id();
    }

    if (isTranscodingEnabled())
    {
        metaData.dewarpingParams.enabled = false;
        return;
    }
}
void ExportStorageStreamRecorder::beforeIoClose(StorageContext& context)
{
    updateSignatureAttr(&context);
}

void ExportStorageStreamRecorder::updateSignatureAttr(StorageContext* context)
{
    NX_VERBOSE(this, "SignVideo: update signature of '%1'",
        nx::utils::url::hidePassword(context->fileName));

    QScopedPointer<QIODevice> file(context->storage->open(context->fileName, QIODevice::ReadWrite));
    if (!file)
    {
        NX_WARNING(this, "SignVideo: could not open the file '%1'",
            nx::utils::url::hidePassword(context->fileName));
        return;
    }

    QByteArray placeholder = QnSignHelper::addSignatureFiller(QnSignHelper::getSignMagic());
    const auto systemContext = SystemContext::fromResource(m_resource);
    QByteArray signature =
        QnSignHelper::addSignatureFiller(m_signer.buildSignature(
            systemContext->licensePool(), systemContext->currentServerId()));

    //New metadata is stored as json, so signature is written base64 - encoded.
    const bool metadataUpdated = updateInFile(file.data(),
        context->fileFormat,
        placeholder.toBase64(),
        signature.toBase64());

    if (!metadataUpdated)
        NX_WARNING(this, "SignVideo: metadata tag was not updated");
}

void ExportStorageStreamRecorder::onSuccessfulPacketWrite(
    AVCodecParameters* avCodecParams, const uint8_t* data, int size)
{
    m_signer.processMedia(avCodecParams, data, size);
}

void ExportStorageStreamRecorder::fileFinished(
    qint64 /*durationMs*/, const QString& /*fileName*/, qint64 /*fileSize*/, qint64 /*startTimeMs*/)
{
}

bool ExportStorageStreamRecorder::fileStarted(
    qint64 /*startTimeMs*/, int /*timeZone*/, const QString& /*fileName*/)
{
    return true;
}

void ExportStorageStreamRecorder::initMetadataStream(StorageContext& /*context*/)
{
}

CodecParametersPtr ExportStorageStreamRecorder::getVideoCodecParameters(
    const QnConstCompressedVideoDataPtr& videoData)
{
    if (isTranscodingEnabled())
    {
        if (m_dstVideoCodec == AV_CODEC_ID_NONE)
        {
            m_dstVideoCodec = findVideoEncoder(
                m_resource->systemContext()->globalSettings()->defaultExportVideoCodec());
        }

        QnFfmpegVideoTranscoder::Config config;
        config.targetCodecId = m_dstVideoCodec;
        config.useMultiThreadEncode = true;
        config.quality = m_transcodeQuality;
        config.startTimeUs = m_preciseStartTimeUs;
        config.decoderConfig.mtDecodePolicy = MultiThreadDecodePolicy::enabled;
        if (m_transcoderFixedFrameRate)
            config.fixedFrameRate = m_transcoderFixedFrameRate;
        config.params = suggestMediaStreamParams(m_dstVideoCodec, m_transcodeQuality);

        if (auto camera = m_resource.dynamicCast<QnVirtualCameraResource>())
            config.sourceResolution = camera->maxVideoResolution();

        m_videoTranscoder = std::make_unique<QnFfmpegVideoTranscoder>(
            config,
            appContext()->currentSystemContext()->metrics().get());
        m_videoTranscoder->setFilterChain(*m_transcodeFilters);
        if (!m_videoTranscoder->open(videoData))
        {
            NX_WARNING(this, "Failed to open video transcoder");
            return nullptr;
        }
        return std::make_shared<CodecParameters>(m_videoTranscoder->getCodecContext());
    }

    if (videoData->context)
        return std::make_shared<CodecParameters>(videoData->context->getAvCodecParameters());

    QSharedPointer<CLVideoDecoderOutput> outFrame(new CLVideoDecoderOutput());
    QnFfmpegVideoDecoder decoder(
        DecoderConfig(),
        appContext()->currentSystemContext()->metrics().get(),
        videoData);
    decoder.decode(videoData, &outFrame);
    return std::make_shared<CodecParameters>(decoder.getContext());
}

CodecParametersConstPtr ExportStorageStreamRecorder::getAudioCodecParameters(
    const CodecParametersConstPtr& sourceCodecParams, const std::string& container)
{
    AVCodecID dstAudioCodec = AV_CODEC_ID_NONE;
    if (container == "avi")
    {
        dstAudioCodec = AV_CODEC_ID_MP3; //< Transcode audio to MP3.
    }
    else if (container == "mp4")
    {
        if (sourceCodecParams->getCodecId() != AV_CODEC_ID_MP3
            && sourceCodecParams->getCodecId() != AV_CODEC_ID_AAC)
        {
            dstAudioCodec = AV_CODEC_ID_MP3; //< Transcode audio to MP3.
        }
    }

    // In the case of MP2 We don't have information about bitrate until we get the first
    // audio packet. This leads to problems with playback in VLC, so we always
    // "transcode" MP2 to MP3.
    const bool transcodingIsRequired =
        sourceCodecParams->getCodecId() == AV_CODEC_ID_MP2 || m_forceAudioTranscoding;

    if (transcodingIsRequired && dstAudioCodec == AV_CODEC_ID_NONE)
        dstAudioCodec = AV_CODEC_ID_MP3;

    if ((dstAudioCodec == AV_CODEC_ID_NONE || dstAudioCodec == sourceCodecParams->getCodecId())
        && !transcodingIsRequired)
    {
        return sourceCodecParams;
    }

    QnFfmpegAudioTranscoder::Config config;
    config.targetCodecId = dstAudioCodec;
    m_audioTranscoder = std::make_unique<QnFfmpegAudioTranscoder>(config);
    if (container == "mp4"
        && dstAudioCodec == AV_CODEC_ID_MP3
        && sourceCodecParams->getSampleRate() < kMinMp4Mp3SampleRate)
    {
        m_audioTranscoder->setSampleRate(kMinMp4Mp3SampleRate);
    }

    if (!m_audioTranscoder->open(sourceCodecParams))
        return nullptr;

    return std::make_shared<CodecParameters>(m_audioTranscoder->getCodecParameters());
}

void ExportStorageStreamRecorder::setTranscoderFixedFrameRate(int value)
{
    m_transcoderFixedFrameRate = value;
}

void ExportStorageStreamRecorder::setTranscoderQuality(Qn::StreamQuality quality)
{
    m_transcodeQuality = quality;
}

void ExportStorageStreamRecorder::addRecordingContext(
    const QString& fileName, const QnStorageResourcePtr& storage)
{
    if (storage.dynamicCast<QnLayoutFileStorageResource>())
    {
        NX_DEBUG(this, "Disable timestamp stitcher, as the NX media format is used");
        m_stitchTimestampGaps = false;
    }

    m_recordingContext = StorageContext(fileName, storage);
}

bool ExportStorageStreamRecorder::addRecordingContext(const QString& fileName)
{
    const auto storage = QnStorageResourcePtr(
        appContext()->storagePluginFactory()->createStorage(fileName));

    if (!storage)
        return false;

    addRecordingContext(fileName, storage);
    return true;
}

QString ExportStorageStreamRecorder::fixedFileName() const
{
    if (!m_recordingContext.isEmpty())
        return m_recordingContext.fileName;

    return QString();
}

bool ExportStorageStreamRecorder::needToTruncate(const QnConstAbstractMediaDataPtr& /*md*/) const
{
    return false;
}

void ExportStorageStreamRecorder::onSuccessfulWriteData(const QnConstAbstractMediaDataPtr& /*md*/)
{
}

void ExportStorageStreamRecorder::onSuccessfulPrepare()
{
    m_signer.reset();
}

void ExportStorageStreamRecorder::reportFinished()
{
    emit recordingFinished(getLastError(), fixedFileName());
}

void ExportStorageStreamRecorder::afterClose()
{
    m_lastVideoCodec = AV_CODEC_ID_NONE;
    m_audioCodecParameters.reset();
}

void ExportStorageStreamRecorder::onFlush(StorageContext& /*context*/)
{
    if (!m_videoTranscoder)
        return;

    while (true)
    {
        QnAbstractMediaDataPtr result;
        m_videoTranscoder->transcodePacket(QnAbstractMediaDataPtr(), &result);
        if (!result || result->dataSize() == 0)
            break;
        StorageRecordingContext::writeData(result, /*stream index*/0);
    }
}

nx::recording::Error::Code toRecordingError(nx::media::StreamEvent code)
{
    switch (code)
    {
        case nx::media::StreamEvent::tooManyOpenedConnections:
        case nx::media::StreamEvent::forbiddenWithDefaultPassword:
        case nx::media::StreamEvent::forbiddenWithNoLicense:
        case nx::media::StreamEvent::oldFirmware:
            return nx::recording::Error::Code::temporaryUnavailable;
        case nx::media::StreamEvent::cannotDecryptMedia:
            return nx::recording::Error::Code::encryptedArchive;
        default:
            return nx::recording::Error::Code::unknown;
    }
}

bool ExportStorageStreamRecorder::saveData(const QnConstAbstractMediaDataPtr& md)
{
    if (!m_isLayoutsInitialised)
    {
        m_isLayoutsInitialised = true;

        // Panoramic video streams will transcoded to single video, so use default video layout.
        QnConstResourceVideoLayoutPtr videoLayout;
        if (isTranscodingEnabled())
            videoLayout.reset(new QnDefaultResourceVideoLayout());
        else
            videoLayout = m_mediaDevice->getVideoLayout(m_mediaProvider);
        setVideoLayout(videoLayout);
        setAudioLayout(m_mediaDevice->getAudioLayout(m_mediaProvider));
        setHasVideo(m_mediaDevice->hasVideo(m_mediaProvider));
    }

    if (md->dataType == QnAbstractMediaData::VIDEO)
    {
        if (m_lastVideoCodec != AV_CODEC_ID_NONE
            && md->compressionType != m_lastVideoCodec
            && !m_videoTranscoder)
        {
            NX_DEBUG(this, "Video compression type has changed from %1 to %2. Close file",
                m_lastVideoCodec, md->compressionType);

            setLastError(nx::recording::Error::Code::videoTranscodingRequired);
            return false;
        }
        m_lastVideoCodec = md->compressionType;
    }
    else if (md->dataType == QnAbstractMediaData::AUDIO)
    {
        if (m_audioCodecParameters && md->context
            && !m_audioCodecParameters->isEqual(*md->context)
            && !m_audioTranscoder)
        {
            NX_DEBUG(this, "Audio codec parameters have changed from %1 to %2. Close file",
                m_audioCodecParameters->toString(), md->context->toString());

            setLastError(nx::recording::Error::Code::audioTranscodingRequired);
            return false;
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

void ExportStorageStreamRecorder::setLastError(nx::recording::Error::Code code)
{
    nx::StorageRecordingContext::setLastError(code);
    close();
    m_needStop = true;
}

qint64 ExportStorageStreamRecorder::getPacketTimeUsec(const QnConstAbstractMediaDataPtr& data)
{
    auto timestamp = data->timestamp - startTimeUs();
    if (ini().ignoreTimelineGaps && m_stitchTimestampGaps)
    {
        const int index = getStreamIndex(data);
        const bool allowEqual = data->dataType == QnAbstractMediaData::GENERIC_METADATA;
        return m_timestampStitcher.process(
            std::chrono::microseconds(timestamp), index, allowEqual).count();
    }

    return timestamp;
}

std::optional<nx::recording::Error> ExportStorageStreamRecorder::getLastError() const
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
