#include "stream_recorder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <common/common_module.h>

#include <core/resource/resource_consumer.h>
#include <core/resource/resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/security_cam_resource.h>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/config.h>

#include <plugins/resource/avi/avi_archive_delegate.h>
#include <nx/streaming/archive_stream_reader.h>

#include "transcoding/ffmpeg_audio_transcoder.h"
#include "transcoding/ffmpeg_video_transcoder.h"
#include "transcoding/filters/contrast_image_filter.h"
#include "transcoding/filters/time_image_filter.h"
#include "transcoding/filters/fisheye_image_filter.h"
#include "transcoding/filters/rotate_image_filter.h"
#include "transcoding/filters/crop_image_filter.h"
#include "transcoding/filters/tiled_image_filter.h"
#include "transcoding/filters/scale_image_filter.h"

#include "decoders/video/ffmpeg_video_decoder.h"
#include "export/sign_helper.h"

#include <utils/common/util.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace {

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4352;
static const int STORE_QUEUE_SIZE = 50;

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


QnStreamRecorder::StreamRecorderContext::StreamRecorderContext(
    const QString& fileName,
    const QnStorageResourcePtr& storage)
    :
    fileName(fileName),
    storage(storage)
{
}

QString QnStreamRecorder::errorString(StreamRecorderError errCode)
{
    switch (errCode)
    {
        case StreamRecorderError::noError:
            return QString();
        case StreamRecorderError::containerNotFound:
            return tr("Corresponding container in FFMPEG library was not found.");
        case StreamRecorderError::fileCreate:
            return tr("Could not create output file for video recording.");
        case StreamRecorderError::videoStreamAllocation:
            return tr("Could not allocate output stream for recording.");
        case StreamRecorderError::audioStreamAllocation:
            return tr("Could not allocate output audio stream.");
        case StreamRecorderError::invalidAudioCodec:
            return tr("Invalid audio codec information.");
        case StreamRecorderError::incompatibleCodec:
            return tr("Video or audio codec is incompatible with the selected format.");
        case StreamRecorderError::fileWrite:
            return tr("File write error. Not enough free space.");
        case StreamRecorderError::invalidResourceType:
            return tr("Invalid resource type for data export.");
        default:
            return QString();
    }
}

QnStreamRecorder::QnStreamRecorder(const QnResourcePtr& dev):
    QnAbstractDataConsumer(STORE_QUEUE_SIZE),
    QnResourceConsumer(dev),
    QnCommonModuleAware(dev->commonModule()),
    m_device(dev),
    m_firstTime(true),
    m_truncateInterval(0),
    m_fixedFileName(false),
    m_endDateTime(AV_NOPTS_VALUE),
    m_startDateTime(AV_NOPTS_VALUE),
    m_currentTimeZone(-1),
    m_waitEOF(false),
    m_forceDefaultCtx(true),
    m_packetWrited(false),
    m_currentChunkLen(0),
    m_prebufferingUsec(0),
    m_EofDateTime(AV_NOPTS_VALUE),
    m_endOfData(false),
    m_lastProgress(-1),
    m_needCalcSignature(false),
    m_mediaProvider(0),
    m_mdctx(EXPORT_SIGN_METHOD),
    m_container(QLatin1String("matroska")),
    m_needReopen(false),
    m_isAudioPresent(false),
    m_audioTranscoder(0),
    m_videoTranscoder(0),
    m_dstAudioCodec(AV_CODEC_ID_NONE),
    m_dstVideoCodec(AV_CODEC_ID_NONE),
    m_serverTimeZoneMs(Qn::InvalidUtcOffset),
    m_nextIFrameTime(AV_NOPTS_VALUE),
    m_truncateIntervalEps(0),
    m_recordingFinished(false),
    m_role(StreamRecorderRole::serverRecording),
    m_gen(m_rd()),
    m_forcedAudioLayout(nullptr),
    m_disableRegisterFile(false)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
    memset(m_motionFileList, 0, sizeof(m_motionFileList));
}

QnStreamRecorder::~QnStreamRecorder()
{
    stop();
    close();
    delete m_audioTranscoder;
    delete m_videoTranscoder;
}

void QnStreamRecorder::updateSignatureAttr(StreamRecorderContext* context)
{
    NX_VERBOSE(this) "SignVideo: update signature at" << context->fileName;
    QScopedPointer<QIODevice> file(context->storage->open(context->fileName, QIODevice::ReadWrite));
    if (!file)
    {
        NX_VERBOSE(this) "SignVideo: could not open the file";
        return;
    }

    // Placeholder start for actual signature. Really QnSignHelper::signSize() bytes written.
    const auto placeholder = QnSignHelper::getSignMagic();

    // Update old metadata (except mp4).
    if (context->fileFormat != QnAviArchiveMetadata::Format::mp4)
    {
        const bool tagUpdated = updateInFile(file.data(),
            context->fileFormat,
            placeholder,
            QnSignHelper::getSignFromDigest(getSignature()));
        NX_ASSERT(tagUpdated, "SignVideo: signature tag was not updated");
    }

    // Update new metadata.
    auto& metadata = context->metadata;
    QByteArray signPattern = metadata.signature;
    QByteArray signPlaceholder = signPattern;

    NX_ASSERT(signPattern.indexOf(placeholder) >= 0, "Sign magic must be present in metadata");
    signPattern.replace(QnSignHelper::getSignMagic(),
        QnSignHelper::getSignFromDigest(getSignature()));

    metadata.signature = QnSignHelper::makeSignature(signPattern);

    //New metadata is stored as json, so signature is written base64 - encoded.
    const bool metadataUpdated = updateInFile(file.data(),
        context->fileFormat,
        signPlaceholder.toBase64(),
        metadata.signature.toBase64());
    NX_ASSERT(metadataUpdated, "SignVideo: metadata tag was not updated");
}

void QnStreamRecorder::close()
{
    for (size_t i = 0; i < m_recordingContextVector.size(); ++i)
    {
        if (m_packetWrited)
            av_write_trailer(m_recordingContextVector[i].formatCtx);


        qint64 fileSize = 0;
        if (m_recordingContextVector[i].formatCtx)
        {
            if (m_startDateTime != qint64(AV_NOPTS_VALUE))
                fileSize = QnFfmpegHelper::getFileSizeByIOContext(m_recordingContextVector[i].formatCtx->pb);

            QnFfmpegHelper::closeFfmpegIOContext(m_recordingContextVector[i].formatCtx->pb);
#ifndef SIGN_FRAME_ENABLED
            if (m_needCalcSignature)
                updateSignatureAttr(&m_recordingContextVector[i]);
#endif
            m_recordingContextVector[i].formatCtx->pb = nullptr;
            avformat_close_input(&m_recordingContextVector[i].formatCtx);
        }
        m_recordingContextVector[i].formatCtx = 0;

        if (m_startDateTime != qint64(AV_NOPTS_VALUE))
        {
            qint64 fileDuration = m_startDateTime !=
                qint64(AV_NOPTS_VALUE) ? m_endDateTime / 1000 - m_startDateTime / 1000 : 0; // bug was here! rounded sum is not same as rounded summand!

            if (m_lastError.lastError != StreamRecorderError::fileCreate && !m_disableRegisterFile)
                fileFinished(
                    fileDuration,
                    m_recordingContextVector[i].fileName,
                    m_mediaProvider,
                    fileSize);
        }
    }

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_motionFileList[i])
            m_motionFileList[i]->close();
    }

    m_packetWrited = false;
    m_endDateTime = m_startDateTime = AV_NOPTS_VALUE;

    markNeedKeyData();
    m_firstTime = true;
    m_prevAudioFormat.reset();

    if (m_recordingFinished)
    {
        // close may be called multiple times, so we have to reset flag m_recordingFinished
        m_recordingFinished = false;
        emit recordingFinished(m_lastError, QString());
    }
}

void QnStreamRecorder::markNeedKeyData()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_gotKeyFrame[i] = false;
}

void QnStreamRecorder::flushPrebuffer()
{
    while (!m_prebuffer.isEmpty())
    {
        QnConstAbstractMediaDataPtr d;
        m_prebuffer.pop(d);
        if (needSaveData(d))
            saveData(d);
        else
            markNeedKeyData();
    }
    m_nextIFrameTime = AV_NOPTS_VALUE;
}

qint64 QnStreamRecorder::findNextIFrame(qint64 baseTime)
{
    for (int i = 0; i < m_prebuffer.size(); ++i)
    {
        const QnConstAbstractMediaDataPtr& media = m_prebuffer.at(i);
        if (media->dataType == QnAbstractMediaData::VIDEO && media->timestamp > baseTime && (media->flags & AV_PKT_FLAG_KEY))
            return media->timestamp;
    }
    return AV_NOPTS_VALUE;
}

bool QnStreamRecorder::processData(const QnAbstractDataPacketPtr& nonConstData)
{
    if (m_needReopen)
    {
        m_needReopen = false;
        NX_VERBOSE(this, lm("processData() EXIT: Reopening"));
        close();
    }

    QnConstAbstractMediaDataPtr md =
        std::dynamic_pointer_cast<const QnAbstractMediaData>(nonConstData);
    if (!md)
    {
        NX_VERBOSE(this, lm("processData() EXIT: Unknown data"));
        return true; // skip unknown data
    }
    if (m_EofDateTime != qint64(AV_NOPTS_VALUE) && md->timestamp > m_EofDateTime)
    {
        if (!m_endOfData)
        {
            bool isOk = true;
            if (m_needCalcSignature && !m_firstTime)
                isOk = addSignatureFrame();

            if (isOk)
                m_lastError = StreamRecorderErrorStruct(
                    StreamRecorderError::noError,
                    QnStorageResourcePtr()
                );

            m_recordingFinished = true;
            m_endOfData = true;
            NX_VERBOSE(this, lm("processData() END: Stopping; m_endOfData: false; error: %1")
                .arg(isOk ? "true" : "false"));
        }
        else
        {
            NX_VERBOSE(this, lm("processData() END: Stopping; m_endOfData: true"));
        }

        pleaseStop();
        return true;
    }

    if (md->dataType == QnAbstractMediaData::META_V1)
    {
        NX_VERBOSE(this, lm("processData(): META_V1"));
        if (needSaveData(md))
            saveData(md);
    }
    else
    {
        m_prebuffer.push(md);
        if (m_prebufferingUsec == 0)
        {
            NX_VERBOSE(this, lm("processData(): no pre-buffering"));
            m_nextIFrameTime = AV_NOPTS_VALUE;
            while (!m_prebuffer.isEmpty())
            {
                QnConstAbstractMediaDataPtr d;
                m_prebuffer.pop(d);
                if (needSaveData(d))
                    saveData(d);
                else if (md->dataType == QnAbstractMediaData::VIDEO)
                    markNeedKeyData();
            }
        }
        else
        {
            bool isKeyFrame = md->dataType == QnAbstractMediaData::VIDEO
                && (md->flags & AV_PKT_FLAG_KEY);
            if (m_nextIFrameTime == (qint64) AV_NOPTS_VALUE && isKeyFrame)
                m_nextIFrameTime = md->timestamp;
            NX_VERBOSE(this, lm("processData(): isKeyFrame: %1 (dataType == VIDEO: %2; flags & VA_PKT_FLAG_KEY: %3)")
                .arg(isKeyFrame ? "true" : "false")
                .arg(md->dataType == QnAbstractMediaData::VIDEO ? "true" : "false")
                .arg(md->flags & AV_PKT_FLAG_KEY ? "true" : "false")
            );

            if (m_nextIFrameTime != (qint64) AV_NOPTS_VALUE
                && md->timestamp - m_nextIFrameTime >= m_prebufferingUsec)
            {
                NX_VERBOSE(this, lm("processData(): find next I-Frame"));
                while (!m_prebuffer.isEmpty() && m_prebuffer.front()->timestamp < m_nextIFrameTime)
                {
                    QnConstAbstractMediaDataPtr d;
                    m_prebuffer.pop(d);
                    if (needSaveData(d))
                        saveData(d);
                    else if (md->dataType == QnAbstractMediaData::VIDEO)
                    {
                        markNeedKeyData();
                    }
                }
                m_nextIFrameTime = findNextIFrame(m_nextIFrameTime);
            }
        }
    }

    if (m_waitEOF && m_dataQueue.size() == 0)
    {
        NX_VERBOSE(this, lm("processData(): closing"));
        close();
        m_waitEOF = false;
    }

    if (m_EofDateTime != qint64(AV_NOPTS_VALUE) && m_EofDateTime > m_startDateTime)
    {
        const int progress =
            ((md->timestamp - m_startDateTime) * 100LL) / (m_EofDateTime - m_startDateTime);
        if (progress != m_lastProgress)
        {
            NX_VERBOSE(this, lm("processData(): progress"));
            emit recordingProgress(progress);
            m_lastProgress = progress;
        }
    }

    NX_VERBOSE(this, lm("processData() END"));
    return true;
}

void QnStreamRecorder::cleanFfmpegContexts()
{
    for (size_t i = 0; i < m_recordingContextVector.size(); ++i)
    {
        if (m_recordingContextVector[i].formatCtx)
        {
            QnFfmpegHelper::closeFfmpegIOContext(m_recordingContextVector[i].formatCtx->pb);
            m_recordingContextVector[i].formatCtx->pb = 0;
            avformat_close_input(&m_recordingContextVector[i].formatCtx);
        }
    }
}

bool QnStreamRecorder::saveData(const QnConstAbstractMediaDataPtr& md)
{
    if (md->dataType == QnAbstractMediaData::META_V1)
    {
        NX_VERBOSE(this, "saveData(): META_V1");
        return saveMotion(std::dynamic_pointer_cast<const QnMetaDataV1>(md));
    }

    if (m_endDateTime != qint64(AV_NOPTS_VALUE)
        && md->timestamp - m_endDateTime > MAX_FRAME_DURATION * 2 * 1000LL
        && m_truncateInterval > 0)
    {
        // If multifile recording is allowed, recreate the file if a recording hole is detected.
        NX_DEBUG(this, lm("Data hole detected for camera %1. Diff between packets: %2 ms")
            .arg(m_device->getUniqueId())
            .arg((md->timestamp - m_endDateTime) / 1000));
        close();
    }
    else if (m_startDateTime != qint64(AV_NOPTS_VALUE))
    {
        if (md->timestamp - m_startDateTime > m_truncateInterval * 3 && m_truncateInterval > 0)
        {
            // if multifile recording allowed, recreate file if recording hole is detected
            NX_DEBUG(this, lm(
                "Too long time when no I-frame detected (file length exceed %1 sec. Close file")
                .arg((md->timestamp - m_startDateTime) / 1000000));
            close();
        }
        else if (md->timestamp < m_startDateTime - 1000LL * 1000)
        {
            NX_DEBUG(this, lm("Time translated into the past for %1 s. Close file")
                .arg((md->timestamp - m_startDateTime) / 1000000));
            close();
        }
    }

    if (md->dataType == QnAbstractMediaData::AUDIO && m_truncateInterval > 0)
    {
        NX_VERBOSE(this, "saveData(): AUDIO");
        // TODO: m_firstTime should not be used to guard comparison with m_prevAudioFormat.
        QnCodecAudioFormat audioFormat(md->context);
        if (!m_prevAudioFormat.is_initialized())
            m_prevAudioFormat = audioFormat;
        else if (m_prevAudioFormat != audioFormat)
            close(); // restart recording file if audio format is changed
    }

    QnConstCompressedVideoDataPtr vd = std::dynamic_pointer_cast<const QnCompressedVideoData>(md);
    //if (!vd)
    //    return true; // ignore audio data

    if (vd && !m_gotKeyFrame[vd->channelNumber] && !(vd->flags & AV_PKT_FLAG_KEY))
    {
        NX_VERBOSE(this, "saveData(): VIDEO; skip data");
        return true; // skip data
    }

    const QnMediaResource* mediaDev = dynamic_cast<const QnMediaResource*>(m_device.data());
    if (m_firstTime)
    {
        if (vd == 0 && mediaDev->hasVideo(md->dataProvider))
        {
            NX_VERBOSE(this, "saveData(): AUDIO; skip audio packets before first video packet");
            return true; // skip audio packets before first video packet
        }
        if (!initFfmpegContainer(md))
        {
            if (!m_recordingContextVector.empty())
                m_recordingFinished = true;

            // clear formatCtx and ioCtx
            cleanFfmpegContexts();

            m_needStop = true;
            NX_VERBOSE(this, "saveData(): first time; initFfmpegContainer() failed");
            return false;
        }

        NX_VERBOSE(this, "saveData(): first time; recording started");
        m_firstTime = false;
        emit recordingStarted();
    }

    int channel = md->channelNumber;

    if (md->flags & AV_PKT_FLAG_KEY)
        m_gotKeyFrame[channel] = true;
    if ((md->flags & AV_PKT_FLAG_KEY) || !mediaDev->hasVideo(m_mediaProvider))
    {
        if (m_truncateInterval > 0
            && md->timestamp - m_startDateTime > (m_truncateInterval + m_truncateIntervalEps))
        {
            m_endDateTime = md->timestamp;
            close();
            m_endDateTime = m_startDateTime = md->timestamp;

            return saveData(md);
        }
    }

    int streamIndex = channel;
    if (md->dataType == QnAbstractMediaData::VIDEO && m_videoTranscoder)
        streamIndex = 0;

    if ((uint) streamIndex >= m_recordingContextVector[0].formatCtx->nb_streams)
    {
        NX_VERBOSE(this, "saveData(): skip packet");
        return true; // skip packet
    }

    m_endDateTime = md->timestamp;

    if (md->dataType == QnAbstractMediaData::AUDIO && m_audioTranscoder)
    {
        QnAbstractMediaDataPtr result;
        bool inputDataDepleted = false;
        do
        {
            m_audioTranscoder->transcodePacket(
                inputDataDepleted ? QnConstAbstractMediaDataPtr() : md, &result);
            if (result && result->dataSize() > 0)
                writeData(result, streamIndex);
            inputDataDepleted = true;
        } while (result);
    }
    else if (md->dataType == QnAbstractMediaData::VIDEO && m_videoTranscoder)
    {
        QnAbstractMediaDataPtr result;
        m_videoTranscoder->transcodePacket(md, &result);
        if (result && result->dataSize() > 0)
            writeData(result, streamIndex);
    }
    else
    {
        writeData(md, streamIndex);
    }

    return true;
}

qint64 QnStreamRecorder::getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md)
{
    return md->timestamp - m_startDateTime;
}

void QnStreamRecorder::writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex)
{
    AVRational srcRate = {1, 1000000};

    for (size_t i = 0; i < m_recordingContextVector.size(); ++i)
    {
#if 0 // for storage balancing algorithm test
        if (md && m_recordingContextVector[i].storage)
            m_recordingContextVector[i].storage->addWrited(md->dataSize());
#endif
        AVStream* stream = m_recordingContextVector[i].formatCtx->streams[streamIndex];
        NX_ASSERT(stream->time_base.num && stream->time_base.den);

        NX_ASSERT(md->timestamp >= 0);

        QnFfmpegAvPacket avPkt;
        qint64 dts = av_rescale_q(getPacketTimeUsec(md), srcRate, stream->time_base);
        if (stream->cur_dts > 0)
            avPkt.dts = qMax((qint64)stream->cur_dts + 1, dts);
        else
            avPkt.dts = dts;
        const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(md.get());
        if (video && video->pts != AV_NOPTS_VALUE)
            avPkt.pts = av_rescale_q(video->pts - m_startDateTime, srcRate, stream->time_base) + (avPkt.dts - dts);
        else
            avPkt.pts = avPkt.dts;

        if (md->flags & AV_PKT_FLAG_KEY)
            avPkt.flags |= AV_PKT_FLAG_KEY;
        avPkt.data = const_cast<quint8*>((const quint8*)md->data());    //const_cast is here because av_write_frame accepts non-const pointer, but does not modify object
        avPkt.size = static_cast<int>(md->dataSize());
        avPkt.stream_index = streamIndex;

        if (avPkt.pts < avPkt.dts)
        {
            avPkt.pts = avPkt.dts;
            NX_LOG(QLatin1String("Timestamp error: PTS < DTS. Fixed."), cl_logWARNING);
        }

        auto startWriteTime = std::chrono::high_resolution_clock::now();
        int ret = av_write_frame(
            m_recordingContextVector[i].formatCtx,
            &avPkt
        );
        auto endWriteTime = std::chrono::high_resolution_clock::now();

        m_recordingContextVector[i].totalWriteTimeNs +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                endWriteTime - startWriteTime
                ).count();

        if (m_recordingContextVector[i].formatCtx->pb->error == -1)
        {   // pb->error == -1 means that pb->write_data returned -1
            // (see libavformat::aviobuf.c writeout() function)
            // that in order means that ffmpegHelper::ffmpegWritePacket returned -1
            // that in order means that QIOdevice::writeData() implementation for example
            // in QnBufferedFile or QnLayoutFile returned -1
            // that in order means that disk write operation failed.
            m_recordingFinished = true;
            m_lastError = StreamRecorderErrorStruct(
                StreamRecorderError::fileWrite,
                m_recordingContextVector[i].storage
            );
            m_needStop = true;
            return;
        }

        if (ret < 0)
        {
            NX_LOG(QLatin1String("AV packet write error"), cl_logWARNING);
        }
        else
        {
            m_packetWrited = true;
            if (m_needCalcSignature)
            {
                if (md->dataType == QnAbstractMediaData::VIDEO && (md->flags & AV_PKT_FLAG_KEY))
                    m_lastIFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(md);
                AVCodecContext* srcCodec = m_recordingContextVector[i].formatCtx->streams[streamIndex]->codec;
                NX_VERBOSE(this) "SignVideo: add video packet of size" << avPkt.size;
                QnSignHelper::updateDigest(srcCodec, m_mdctx, avPkt.data, avPkt.size);
                //EVP_DigestUpdate(m_mdctx, (const char*)avPkt.data, avPkt.size);
            }
        }
    }
}

void QnStreamRecorder::endOfRun()
{
    close();
}

bool QnStreamRecorder::initFfmpegContainer(const QnConstAbstractMediaDataPtr& mediaData)
{
    m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (mediaData->dataProvider);
    //NX_ASSERT(m_mediaProvider); //< Commented out since

    m_endDateTime = m_startDateTime = mediaData->timestamp;

    // allocate container
    AVOutputFormat * outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        m_lastError = StreamRecorderErrorStruct(
            StreamRecorderError::containerNotFound,
            QnStorageResourcePtr()
        );
        NX_LOG(lit("No %1 container in FFMPEG library.").arg(m_container), cl_logERROR);
        return false;
    }

    m_currentTimeZone = currentTimeZone() / 60;
    QString fileExt = QString(QLatin1String(outputCtx->extensions)).split(QLatin1Char(','))[0];

    // get recording context list
    getStoragesAndFileNames(m_mediaProvider);
    if (m_recordingContextVector.empty() ||
        m_recordingContextVector[0].fileName.isEmpty())
    {
        return false;
    }

    QString dFileExt = QLatin1Char('.') + fileExt;

    for (size_t i = 0; i < m_recordingContextVector.size(); ++i)
    {
        auto& context = m_recordingContextVector[i];

        if (!context.fileName.endsWith(dFileExt, Qt::CaseInsensitive))
            context.fileName += dFileExt;
        QString url = context.fileName;

        int err = avformat_alloc_output_context2(
            &context.formatCtx,
            outputCtx,
            0,
            url.toUtf8().constData()
        );

        if (err < 0)
        {
            m_lastError = StreamRecorderErrorStruct(
                StreamRecorderError::fileCreate,
                context.storage
            );
            NX_LOG(lit("Can't create output file '%1' for video recording.")
                .arg(context.fileName), cl_logERROR);

            msleep(500); // avoid createFile flood
            return false;
        }

        const QnMediaResource* mediaDev = dynamic_cast<const QnMediaResource*>(m_device.data());

        // m_forceDefaultCtx: for server archive, if file is recreated - we need to use default context.
        // for exporting AVI files we must use original context, so need to reset "force" for exporting purpose
        bool isTranscode = !m_extraTranscodeParams.isEmpty() || (m_dstVideoCodec != AV_CODEC_ID_NONE && m_dstVideoCodec != mediaData->compressionType);

        const QnConstResourceVideoLayoutPtr& layout = mediaDev->getVideoLayout(m_mediaProvider);

        // Always save with latest version.
        context.metadata.version = QnAviArchiveMetadata::kLatestVersion;

        if (!isTranscode)
        {
            context.metadata.videoLayoutSize = layout->size();
            context.metadata.videoLayoutChannels = layout->getChannels();
            context.metadata.overridenAr = mediaDev->customAspectRatio();
        }

        if (isUtcOffsetAllowed())
            context.metadata.startTimeMs = mediaData->timestamp / 1000LL;

        context.metadata.dewarpingParams = mediaDev->getDewarpingParams();
        if (isTranscode)
            context.metadata.dewarpingParams.enabled = false;

        context.metadata.timeZoneOffset = m_serverTimeZoneMs;

#ifndef SIGN_FRAME_ENABLED
        if (m_needCalcSignature)
        {
            QByteArray signPattern = QnSignHelper::getSignPattern(licensePool());

            // Add server time zone as one more column to sign pattern.
            if (m_serverTimeZoneMs != Qn::InvalidUtcOffset)
                signPattern.append(QByteArray::number(m_serverTimeZoneMs));
            context.metadata.signature = QnSignHelper::makeSignature(signPattern);
        }
#endif

        if (fileExt == lit("avi"))
            context.fileFormat = QnAviArchiveMetadata::Format::avi;
        else if (fileExt == lit("mp4"))
            context.fileFormat = QnAviArchiveMetadata::Format::mp4;
        context.metadata.saveToFile(context.formatCtx, context.fileFormat);

        context.formatCtx->start_time = mediaData->timestamp;

        if (auto videoData = std::dynamic_pointer_cast<const QnCompressedVideoData>(mediaData))
        {
            const int videoChannels = isTranscode ? 1 : layout->channelCount();
            for (int j = 0; j < videoChannels; ++j)
            {
                AVStream* videoStream = avformat_new_stream(
                    context.formatCtx,
                    nullptr
                );

                if (videoStream == 0)
                {
                    m_lastError = StreamRecorderErrorStruct(
                        StreamRecorderError::videoStreamAllocation,
                        context.storage
                    );
                    NX_LOG(lit("Can't allocate output stream for recording."), cl_logERROR);
                    return false;
                }

                videoStream->id = DEFAULT_VIDEO_STREAM_ID + j;
                AVCodecContext* videoCodecCtx = videoStream->codec;
                videoCodecCtx->codec_id = mediaData->compressionType;
                videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
                if (mediaData->compressionType == AV_CODEC_ID_MJPEG)
                    videoCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
                else
                    videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

                if (isTranscode)
                {
                    // transcode video
                    if (m_dstVideoCodec == AV_CODEC_ID_NONE)
                        m_dstVideoCodec = AV_CODEC_ID_MPEG4; // default value
                    m_videoTranscoder = new QnFfmpegVideoTranscoder(m_dstVideoCodec);
                    m_videoTranscoder->setMTMode(true);

                    m_videoTranscoder->open(videoData);
                    m_videoTranscoder->setFilterList(m_extraTranscodeParams.createFilterChain(m_videoTranscoder->getResolution()));
                    m_videoTranscoder->setQuality(Qn::QualityHighest);
                    m_videoTranscoder->open(videoData); // reopen again for new size

                    QnFfmpegHelper::copyAvCodecContex(videoStream->codec, m_videoTranscoder->getCodecContext());
                }
                else if (!m_forceDefaultCtx && mediaData->context && mediaData->context->getWidth() > 0)
                {
                    QnFfmpegHelper::mediaContextToAvCodecContext(videoCodecCtx, mediaData->context);
                }
                else if (m_role == StreamRecorderRole::fileExport || m_role == StreamRecorderRole::fileExportWithEmptyContext)
                {
                    // determine real width and height
                    QSharedPointer<CLVideoDecoderOutput> outFrame(new CLVideoDecoderOutput());
                    QnFfmpegVideoDecoder decoder(mediaData->compressionType, videoData, false);
                    decoder.decode(videoData, &outFrame);
                    if (m_role == StreamRecorderRole::fileExport)
                    {
                        QnFfmpegHelper::copyAvCodecContex(videoStream->codec, decoder.getContext());
                    }
                    else
                    {
                        videoCodecCtx->width = decoder.getWidth();
                        videoCodecCtx->height = decoder.getHeight();
                    }
                }
                else
                {
                    videoCodecCtx->width = qMax(8, videoData->width);
                    videoCodecCtx->height = qMax(8, videoData->height);
                }

                videoCodecCtx->bit_rate = 1000000 * 6;
                videoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
                AVRational defaultFrameRate = {1, 60};
                videoCodecCtx->time_base = defaultFrameRate;

                videoCodecCtx->sample_aspect_ratio.num = 1;
                videoCodecCtx->sample_aspect_ratio.den = 1;

                videoStream->sample_aspect_ratio = videoCodecCtx->sample_aspect_ratio;
                videoStream->first_dts = 0;
            }
        }

        QnConstResourceAudioLayoutPtr audioLayout = m_forcedAudioLayout
            ? m_forcedAudioLayout.dynamicCast<const QnResourceAudioLayout>()
            : mediaDev->getAudioLayout(m_mediaProvider);

        m_isAudioPresent = audioLayout->channelCount() > 0;
        for (int j = 0; j < audioLayout->channelCount(); ++j)
        {
            AVStream* audioStream = avformat_new_stream(
                context.formatCtx,
                nullptr
            );

            if (!audioStream)
            {
                m_lastError = StreamRecorderErrorStruct(
                    StreamRecorderError::audioStreamAllocation,
                    context.storage
                );
                NX_LOG(lit("Can't allocate output audio stream."), cl_logERROR);
                return false;
            }

            audioStream->id = DEFAULT_AUDIO_STREAM_ID + j;
            AVCodecID srcAudioCodec = AV_CODEC_ID_NONE;
            QnConstMediaContextPtr mediaContext = audioLayout->getAudioTrackInfo(j).codecContext;
            if (!mediaContext)
            {
                m_lastError = StreamRecorderErrorStruct(
                    StreamRecorderError::invalidAudioCodec,
                    context.storage
                );
                NX_LOG(lit("Invalid audio codec information."), cl_logERROR);
                return false;
            }

            srcAudioCodec = mediaContext->getCodecId();

            if (m_dstAudioCodec == AV_CODEC_ID_NONE || m_dstAudioCodec == srcAudioCodec)
            {
                QnFfmpegHelper::mediaContextToAvCodecContext(audioStream->codec, mediaContext);

                // avoid FFMPEG bug for MP3 mono. block_align hardcoded inside ffmpeg for stereo channels and it is cause problem
                if (srcAudioCodec == AV_CODEC_ID_MP3 && audioStream->codec->channels == 1)
                    audioStream->codec->block_align = 0;
            }
            else
            {
                // transcode audio
                m_audioTranscoder = new QnFfmpegAudioTranscoder(m_dstAudioCodec);
                m_audioTranscoder->open(mediaContext);
                QnFfmpegHelper::copyAvCodecContex(audioStream->codec, m_audioTranscoder->getCodecContext());
            }
            audioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            audioStream->first_dts = 0;
        }

        initIoContext(
            context.storage,
            context.fileName,
            &context.formatCtx->pb);

        if (context.formatCtx->pb == 0)
        {
            avformat_close_input(&context.formatCtx);
            m_lastError = StreamRecorderErrorStruct(StreamRecorderError::fileCreate,
                context.storage);
            NX_LOG(lit("Can't create output file '%1'.").arg(url), cl_logERROR);
            m_recordingFinished = true;
            msleep(500); // avoid createFile flood
            return false;
        }

        int rez = avformat_write_header(context.formatCtx, 0);
        if (rez < 0)
        {
            QnFfmpegHelper::closeFfmpegIOContext(context.formatCtx->pb);
            context.formatCtx->pb = nullptr;
            avformat_close_input(&context.formatCtx);
            m_lastError = StreamRecorderErrorStruct(
                StreamRecorderError::incompatibleCodec,
                context.storage
            );
            NX_LOG(lit("Video or audio codec is incompatible with %1 format. Try another format. Ffmpeg error: %2").
                arg(m_container).arg(QnFfmpegHelper::getErrorStr(rez)), cl_logERROR);
            return false;
        }

        if (!m_disableRegisterFile)
            fileStarted(
                m_startDateTime / 1000,
                m_currentTimeZone,
                context.fileName,
                m_mediaProvider
            );

        if (m_truncateInterval > 0)
        {
            std::uniform_int_distribution<qint64> dis(1, m_truncateInterval / 4);

            if (m_truncateInterval > 4000000ll)
                m_truncateIntervalEps = dis(m_gen);
        }
        else
        {
            m_truncateIntervalEps = 0;
        }

    } // for each storage

    return true;
}

void QnStreamRecorder::initIoContext(
    const QnStorageResourcePtr& storage,
    const QString& url,
    AVIOContext** context)
{
    *context = QnFfmpegHelper::createFfmpegIOContext(
        storage,
        url,
        QIODevice::WriteOnly);
}

void QnStreamRecorder::setTruncateInterval(int seconds)
{
    m_truncateInterval = seconds * 1000000ll;
}

void QnStreamRecorder::addRecordingContext(
    const QString               &fileName,
    const QnStorageResourcePtr  &storage
)
{
    m_fixedFileName = true;
    m_recordingContextVector.emplace_back(
        fileName,
        storage
    );
}

bool QnStreamRecorder::addRecordingContext(const QString &fileName)
{
    m_fixedFileName = true;
    auto storage = QnStorageResourcePtr(
        QnStoragePluginFactory::instance()->createStorage(
            commonModule(),
            fileName
        )
    );

    if (storage)
    {
        m_recordingContextVector.emplace_back(
            fileName,
            storage
        );
        return true;
    }
    return false;
}

void QnStreamRecorder::setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS])
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionFileList[i] = motionFileList[i];
}

void QnStreamRecorder::setPrebufferingUsec(int value)
{
    m_prebufferingUsec = value;
}

int QnStreamRecorder::getPrebufferingUsec() const
{
    return m_prebufferingUsec;
}


bool QnStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& /*media*/)
{
    return true;
}

bool QnStreamRecorder::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (m_motionHandler)
        return m_motionHandler(motion);

    if (motion && !motion->isEmpty() && m_motionFileList[motion->channelNumber])
        motion->serialize(m_motionFileList[motion->channelNumber].data());
    return true;
}

void QnStreamRecorder::getStoragesAndFileNames(QnAbstractMediaStreamDataProvider* /*provider*/)
{
    NX_ASSERT(!m_recordingContextVector.empty(), "Make sure you've called addRecordingContext()");
}

QString QnStreamRecorder::fixedFileName() const
{
    if (m_fixedFileName)
    {
        Q_ASSERT(!m_recordingContextVector.empty());
        if (!m_recordingContextVector.empty())
            return m_recordingContextVector[0].fileName;
    }
    return QString();
}

void QnStreamRecorder::setEofDateTime(qint64 value)
{
    m_endOfData = false;
    m_EofDateTime = value;
    m_lastProgress = -1;
}

void QnStreamRecorder::setNeedCalcSignature(bool value)
{
    if (m_needCalcSignature == value)
        return;

    m_needCalcSignature = value;

    if (value)
    {
        m_mdctx.reset();
        m_mdctx.addData(EXPORT_SIGN_MAGIC, sizeof(EXPORT_SIGN_MAGIC));
        NX_VERBOSE(this) "SignVideo: init";
    }
}

QByteArray QnStreamRecorder::getSignature() const
{
    return m_mdctx.result();
}

bool QnStreamRecorder::addSignatureFrame()
{
#ifndef SIGN_FRAME_ENABLED
    QByteArray signText = QnSignHelper::getSignPattern(licensePool());
    if (m_serverTimeZoneMs != Qn::InvalidUtcOffset)
        signText.append(QByteArray::number(m_serverTimeZoneMs)); // I've included server timezone to sign to prevent modification this attribute
    NX_VERBOSE(this) "SignVideo: add signature";
    QnSignHelper::updateDigest(nullptr, m_mdctx, (const quint8*)signText.data(), signText.size());
#else
    AVCodecContext* srcCodec = m_formatCtx->streams[0]->codec;
    QnSignHelper signHelper;
    signHelper.setLogo(QPixmap::fromImage(m_logo));
    signHelper.setSign(getSignature());
    QnCompressedVideoDataPtr generatedFrame = signHelper.createSignatureFrame(srcCodec, m_lastIFrame);

    if (generatedFrame == 0)
    {
        m_lastErrMessage = lit("Error during watermark generation for file \"%1\".").arg(m_fileName);
        qWarning() << m_lastErrMessage;
        return false;
    }

    generatedFrame->timestamp = m_endDateTime + 100 * 1000ll;
    m_needCalcSignature = false; // prevent recursive calls
    //for (int i = 0; i < 2; ++i) // 2 - work, 1 - work at VLC
    {
        saveData(generatedFrame);
        generatedFrame->timestamp += 100 * 1000ll;
    }
    m_needCalcSignature = true;
#endif

    return true;
}

void QnStreamRecorder::setRole(StreamRecorderRole role)
{
    m_role = role;
    m_forceDefaultCtx =
        (m_role == StreamRecorderRole::serverRecording)
        || (m_role == StreamRecorderRole::fileExportWithEmptyContext);
}

#ifdef SIGN_FRAME_ENABLED
void QnStreamRecorder::setSignLogo(const QImage& logo)
{
    m_logo = logo;
}
#endif

//void QnStreamRecorder::setStorage(const QnStorageResourcePtr& storage)
//{
//    m_storage = storage;
//}

void QnStreamRecorder::setContainer(const QString& container)
{
    m_container = container;
    // convert short file ext to ffmpeg container name
    if (m_container == QLatin1String("mkv"))
        m_container = QLatin1String("matroska");
}

void QnStreamRecorder::setNeedReopen()
{
    m_needReopen = true;
}

bool QnStreamRecorder::isAudioPresent() const
{
    return m_isAudioPresent;
}

void QnStreamRecorder::setAudioCodec(AVCodecID codec)
{
    m_dstAudioCodec = codec;
}

void QnStreamRecorder::setServerTimeZoneMs(qint64 value)
{
    m_serverTimeZoneMs = value;
}

void QnStreamRecorder::disconnectFromResource()
{
    stop();
    QnResourceConsumer::disconnectFromResource();
}

void QnStreamRecorder::forceAudioLayout(const QnResourceAudioLayoutPtr& layout)
{
    m_forcedAudioLayout = layout;
}

void QnStreamRecorder::disableRegisterFile(bool disable)
{
    m_disableRegisterFile = disable;
}

void QnStreamRecorder::setSaveMotionHandler(MotionHandler handler)
{
    m_motionHandler = handler;
}

void QnStreamRecorder::setExtraTranscodeParams(const QnImageFilterHelper& extraParams)
{
    m_extraTranscodeParams = extraParams;
}

#endif // ENABLE_DATA_PROVIDERS
