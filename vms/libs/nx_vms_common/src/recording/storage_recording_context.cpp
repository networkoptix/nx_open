// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_recording_context.h"

#include <chrono>
#include <string>
#include <thread>

#include <core/resource/media_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/utils.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <utils/media/ffmpeg_io_context.h>

#include "helpers/recording_context_helpers.h"
#include "stream_recorder.h"

namespace nx {

using namespace std::chrono;
using namespace recording;

namespace {

QString fileExtensionFromContainer(const QString& container)
{
    AVOutputFormat* outputCtx = av_guess_format(container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        throw ErrorEx(
            Error::Code::containerNotFound,
            NX_FMT("No '%1' container in FFMPEG library.", container));
    }

    return QString(outputCtx->extensions).split(',')[0];
}

bool convertExtraDataToMp4(
    const QnCompressedVideoData* videoData, CodecParametersPtr codecParameters)
{
    if (nx::media::isAnnexb(videoData)) // Video data will be converted before call av_write_frame.
    {
        auto extradata = nx::media::buildExtraDataMp4(videoData);
        if (extradata.empty())
        {
            NX_WARNING(NX_SCOPE_TAG, "Failed to build extradata in mp4 format");
            return false;
        }
        if (codecParameters)
            codecParameters->setExtradata(extradata.data(), extradata.size());
    }
    return true;
}

} // namespace

StorageRecordingContext::StorageRecordingContext()
{
}

void StorageRecordingContext::initializeRecordingContext(
    const QnConstAbstractMediaDataPtr& mediaData)
{
    if (m_recordingContext.fileName.isEmpty())
        throw ErrorEx(Error::Code::fileCreate, "No output file has been provided");

    const auto fileExt = fileExtensionFromContainer(m_container);
    const auto dFileExt = '.' + fileExt;
    if (!m_recordingContext.fileName.endsWith(dFileExt, Qt::CaseInsensitive))
        m_recordingContext.fileName += dFileExt;

    m_recordingContext.fileFormat = QnAviArchiveMetadata::formatFromExtension(fileExt);
    if (avformat_alloc_output_context2(
        &m_recordingContext.formatCtx, nullptr, nullptr,
        m_recordingContext.fileName.toUtf8().constData()) < 0)
    {
        std::this_thread::sleep_for(500ms); //< Avoid createFile flood.
        throw ErrorEx(
            Error::Code::fileCreate,
            "Failed to initialize Ffmpeg. Not enough memory.",
            m_recordingContext.storage);
    }

    m_recordingContext.formatCtx->start_time = mediaData->timestamp;
}

int StorageRecordingContext::streamCount() const
{
    if (m_recordingContext.isEmpty())
        return 0;

    return m_recordingContext.formatCtx->nb_streams;
}

CodecParametersPtr StorageRecordingContext::getVideoCodecParameters(
    const QnConstCompressedVideoDataPtr& videoData)
{
    CodecParametersPtr codecParameters;
    if (videoData->context)
        codecParameters = std::make_shared<CodecParameters>(videoData->context->getAvCodecParameters());

    if (!codecParameters)
        codecParameters = QnFfmpegHelper::createVideoCodecParametersAnnexB(videoData.get());

    return codecParameters;
}

CodecParametersConstPtr StorageRecordingContext::getAudioCodecParameters(
    const CodecParametersConstPtr& sourceCodecParams, const QString& /*container*/)
{
    return sourceCodecParams;
}

void StorageRecordingContext::allocateFfmpegObjects(
    const QnConstAbstractMediaDataPtr& mediaData,
    const QnConstResourceVideoLayoutPtr& videoLayout,
    const AudioLayoutConstPtr& audioLayout,
    StorageContext& context)
{
    if (videoLayout)
    {
        if (auto videoData = std::dynamic_pointer_cast<const QnCompressedVideoData>(mediaData))
        {
            for (int i = 0; i < videoLayout->channelCount(); ++i)
            {
                auto codecParams = getVideoCodecParameters(videoData);
                if (!codecParams)
                    throw ErrorEx(Error::Code::incompatibleCodec, "No video codec parameters");
                if (m_container == "matroska")
                {
                    if (!convertExtraDataToMp4(videoData.get(), codecParams))
                        throw ErrorEx(Error::Code::incompatibleCodec, "Failed to convert codec parameters");
                }
                auto avCodecParams = codecParams->getAvCodecParameters();
                if (!avCodecParams)
                    throw ErrorEx(Error::Code::invalidAudioCodec, "Invalid video codec");
                if (!nx::media::fillExtraDataAnnexB(
                    videoData.get(), &avCodecParams->extradata, &avCodecParams->extradata_size))
                {
                    throw ErrorEx(Error::Code::videoStreamAllocation, "Failed to build extra data");
                }

                if (!helpers::addStream(codecParams, context.formatCtx))
                {
                    throw ErrorEx(
                        Error::Code::videoStreamAllocation, "Can't allocate output video stream");
                }
            }
        }
    }

    if (audioLayout)
    {
        for (const auto& track: audioLayout->tracks())
        {
            auto codecParameters = getAudioCodecParameters(track.codecParams, m_container);
            if (!codecParameters)
                throw ErrorEx(Error::Code::invalidAudioCodec, "Invalid audio codec");
            if (!helpers::addStream(codecParameters, context.formatCtx))
            {
                throw ErrorEx(
                    Error::Code::audioStreamAllocation, "Can't allocate output audio stream");
            }
        }
    }

    initMetadataStream(context);
    initIoContext(context);
    writeHeader(context);
}

void StorageRecordingContext::writeHeader(StorageContext& context)
{
    if (int rez = avformat_write_header(context.formatCtx, 0); rez < 0)
    {
        QString codecs;
        for (int i = 0; i < context.formatCtx->nb_streams; ++i)
        {
            if (context.formatCtx->streams[i] && context.formatCtx->streams[i]->codecpar)
                codecs += QString::number(context.formatCtx->streams[i]->codecpar->codec_id) + " ";
        }
        throw ErrorEx(
            Error::Code::incompatibleCodec,
            NX_FMT("Video or audio codec is incompatible with '%1' format. Try another format. Ffmpeg error: %2, codecs: %3",
                m_container, QnFfmpegHelper::avErrorToString(rez), codecs));
    }
}

void StorageRecordingContext::cleanFfmpegContexts()
{
    if (m_recordingContext.isEmpty())
        return;

    nx::utils::media::closeFfmpegIOContext(m_recordingContext.formatCtx->pb);
    m_recordingContext.formatCtx->pb = 0;
    avformat_free_context(m_recordingContext.formatCtx);
    m_recordingContext.formatCtx = nullptr;
}

void StorageRecordingContext::initIoContext(StorageContext& context)
{
    context.formatCtx->pb = nx::utils::media::createFfmpegIOContext(
        context.storage, context.fileName, QIODevice::WriteOnly);

    if (context.formatCtx->pb == nullptr)
    {
        std::this_thread::sleep_for(500ms); // avoid createFile flood
        throw ErrorEx(
            Error::Code::fileCreate,
            NX_FMT("Can't create output file '%1'", nx::utils::url::hidePassword(context.fileName)));
    }
}

qint64 StorageRecordingContext::getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md)
{
    return md->timestamp - startTimeUs();
}

AVMediaType StorageRecordingContext::streamAvMediaType(
    QnAbstractMediaData::DataType dataType, int streamIndex) const
{
    if (dataType == QnAbstractMediaData::GENERIC_METADATA)
    {
        streamIndex = m_recordingContext.metadataStream
            ? m_recordingContext.metadataStream->index : -1;
    }

    if (!m_recordingContext.formatCtx ||
        streamIndex < 0 ||
        streamIndex >= (int) m_recordingContext.formatCtx->nb_streams)
    {
        return AVMEDIA_TYPE_UNKNOWN;
    }

    return m_recordingContext.formatCtx->streams[streamIndex]->codecpar->codec_type;
}

void StorageRecordingContext::writeData(const QnConstAbstractMediaDataPtr& mediaData, int streamIndex)
{
    auto md = mediaData;
    auto videoData = dynamic_cast<const QnCompressedVideoData*>(mediaData.get());
    if (m_container == "matroska" && videoData && nx::media::isAnnexb(videoData))
    {
        md = m_annexbToMp4.process(videoData);
        if (!md)
        {
            NX_WARNING(this, "Conversion failed, cannot convert to MP4 format");
            md = mediaData;
        }
    }

    using namespace std::chrono;
    AVRational srcRate = { 1, 1'000'000 };

    auto& context = m_recordingContext;
    const uint8_t* packetData = nullptr;
    int packetDataSize = 0;
    QByteArray metadataPacketData;
    if (md->dataType == QnAbstractMediaData::GENERIC_METADATA)
    {
        if (!context.metadataStream)
        {
            m_lastError = Error(Error::Code::metadataStreamAllocation, context.storage);
            NX_WARNING(this, "Received metadata packet but there is no metadata stream allocated");
            return;
        }

        streamIndex = context.metadataStream->index;
        auto metadataPacket = std::dynamic_pointer_cast<const QnAbstractCompressedMetadata>(md);
        NX_ASSERT(metadataPacket);
        metadataPacketData = nx::recording::helpers::serializeMetadataPacket(metadataPacket);
    }

    AVStream* stream = context.formatCtx->streams[streamIndex];
    NX_ASSERT(stream->time_base.num && stream->time_base.den);
    NX_ASSERT(md->timestamp >= 0);
    NX_ASSERT(
        md->dataType != QnAbstractMediaData::UNKNOWN
        && toAvMediaType(md->dataType) == stream->codecpar->codec_type);

    QnFfmpegAvPacket avPkt;
    int64_t timestamp = getPacketTimeUsec(md);
    int64_t timestampOffsetUsec = md->timestamp - timestamp;
    qint64 dts = av_rescale_q(timestamp, srcRate, stream->time_base);

    if (stream->cur_dts > 0)
        avPkt.dts = qMax((qint64)stream->cur_dts + 1, dts);
    else
        avPkt.dts = dts;

    const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(md.get());
    if (video && video->pts != AV_NOPTS_VALUE
        && !video->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        auto pts = video->pts - timestampOffsetUsec;
        avPkt.pts = av_rescale_q(
            pts, srcRate, stream->time_base) + (avPkt.dts - dts);
    }
    else
    {
        avPkt.pts = avPkt.dts;
    }

    if ((md->flags & AV_PKT_FLAG_KEY) && md->dataType != QnAbstractMediaData::GENERIC_METADATA)
        avPkt.flags |= AV_PKT_FLAG_KEY;

    if (!metadataPacketData.isEmpty())
    {
        packetData = const_cast<quint8*>((const quint8*)metadataPacketData.constData());
        packetDataSize = int(metadataPacketData.size());
    }
    else
    {
        packetData = const_cast<quint8*>((const quint8*)md->data());
        packetDataSize = static_cast<int>(md->dataSize());
    }
    avPkt.data = const_cast<uint8_t*>(packetData);  // const_cast is here because av_write_frame accepts non-const pointer, but does not modify object
    avPkt.size = packetDataSize;
    avPkt.stream_index = streamIndex;

    if (avPkt.pts < avPkt.dts)
    {
        avPkt.pts = avPkt.dts;
        NX_WARNING(this, "Timestamp error: PTS < DTS. Fixed.");
    }

    auto startWriteTime = high_resolution_clock::now();
    int ret;
    if (isInterleavedStream())
        ret = av_interleaved_write_frame(context.formatCtx, &avPkt);
    else
        ret = av_write_frame(context.formatCtx, &avPkt);
    auto endWriteTime = high_resolution_clock::now();

    context.totalWriteTimeNs +=
        duration_cast<nanoseconds>(endWriteTime - startWriteTime).count();

    if (ret < 0 || context.formatCtx->pb->error == -1) //< Disk write operation failed.
    {
        m_lastError = Error(Error::Code::fileWrite, context.storage);
        NX_WARNING(
            this,
            "Write to '%1' failed. Ffmpeg error: '%2'",
            nx::utils::url::hidePassword(context.fileName),
            ret < 0 ? QnFfmpegHelper::avErrorToString(ret) : "Write failed");

        return;
    }

    m_packetWritten = true;
    onSuccessfulPacketWrite(
        context.formatCtx->streams[streamIndex]->codecpar, packetData, packetDataSize);
}

std::optional<nx::recording::Error> StorageRecordingContext::getLastError() const
{
    return m_lastError;
}

void StorageRecordingContext::closeRecordingContext(std::chrono::milliseconds durationMs)
{
    NX_VERBOSE(this, "closeRecordingContext called");
    if (m_packetWritten) //< TODO: #lbusygin Change to `if (m_recordingContext.formatCtx)`.
    {
        onFlush(m_recordingContext);

        av_write_trailer(m_recordingContext.formatCtx);
    }

    qint64 fileSize = 0;
    if (m_recordingContext.formatCtx)
    {
        nx::utils::media::closeFfmpegIOContext(m_recordingContext.formatCtx->pb);
        if (startTimeUs() != qint64(AV_NOPTS_VALUE))
            fileSize =  m_recordingContext.storage->getFileSize(m_recordingContext.fileName);
        beforeIoClose(m_recordingContext);
        m_recordingContext.formatCtx->pb = nullptr;
        avformat_free_context(m_recordingContext.formatCtx);
    }

    m_recordingContext.formatCtx = 0;
    if (startTimeUs() != qint64(AV_NOPTS_VALUE))
    {
        NX_VERBOSE(this, "About to call fileFinished");
        if (!m_lastError || m_lastError->code != Error::Code::fileCreate)
        {
            NX_VERBOSE(
                this, "Calling fileFinished() for '%1', file size: %2, duration: %3",
                nx::utils::url::hidePassword(m_recordingContext.fileName), fileSize, durationMs);
            fileFinished(
                durationMs.count(), m_recordingContext.fileName, fileSize, startTimeUs() / 1000);
        }
        else
        {
            NX_VERBOSE(
                this, "Won't call fileFinished() for '%1' because we failed to create file previously",
                nx::utils::url::hidePassword(m_recordingContext.fileName));
        }
    }
    else if (!m_lastError)
    {
        NX_VERBOSE(
            this, "Won't call fileFinished() for '%1' because of invalid start time and no last error set",
            nx::utils::url::hidePassword(m_recordingContext.fileName));

        m_lastError = Error(Error::Code::dataNotFound);
    }
    else
    {
        NX_VERBOSE(
            this, "Won't call fileFinished() for '%1' because of invalid start time. Last error is: %2",
            nx::utils::url::hidePassword(m_recordingContext.fileName), *m_lastError);
    }

    m_packetWritten = false;
}

void StorageRecordingContext::setLastError(nx::recording::Error::Code code)
{
    m_lastError = code;
}

bool StorageRecordingContext::doPrepareToStart(
    const QnConstAbstractMediaDataPtr& mediaData,
    const QnConstResourceVideoLayoutPtr& videoLayout,
    const AudioLayoutConstPtr& audioLayout,
    const QnAviArchiveMetadata& metadata)
{
    try
    {
        auto& ctx = m_recordingContext;
        m_lastError = std::nullopt;
        initializeRecordingContext(mediaData);
        ctx.metadata = metadata;
        if (!ctx.metadata.saveToFile(ctx.formatCtx, ctx.fileFormat))
        {
            throw ErrorEx(
                Error::Code::fileWrite,
                NX_FMT("Failed to write metadata to file '%1'",
                    nx::utils::url::hidePassword(ctx.fileName)));
        }

        allocateFfmpegObjects(mediaData, videoLayout, audioLayout, ctx);
        if (!fileStarted(
            startTimeUs() / 1000,
            QDateTime::currentDateTime().offsetFromUtc() / 60,
            ctx.fileName))
        {
            throw ErrorEx(
                Error::Code::fileWrite,
                NX_FMT("Failed to initialize catalog record for file '%1'",
                    nx::utils::url::hidePassword(ctx.fileName)));
        }

        return true;
    }
    catch (const ErrorEx& e)
    {
        NX_WARNING(this, "prepareToStart: " + e.message);
        m_lastError = static_cast<Error>(e);
        cleanFfmpegContexts();
        return false;
    }

    return true;
}

void StorageRecordingContext::setContainer(const QString& container)
{
    m_container = container;
    if (m_container == "mkv")
        m_container = "matroska";
}

} // namespace nx
