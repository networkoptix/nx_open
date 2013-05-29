#include "stream_recorder.h"
#include "core/resource/resource_consumer.h"
#include "core/datapacket/abstract_data_packet.h"
#include "core/datapacket/media_data_packet.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "utils/common/util.h"
#include "decoders/video/ffmpeg.h"
#include "export/sign_helper.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "transcoding/ffmpeg_audio_transcoder.h"
#include "transcoding/ffmpeg_video_transcoder.h"

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4352;

static const int STORE_QUEUE_SIZE = 50;

QnStreamRecorder::QnStreamRecorder(QnResourcePtr dev):
    QnAbstractDataConsumer(STORE_QUEUE_SIZE),
    m_device(dev),
    m_firstTime(true),
    m_truncateInterval(0),
    m_endDateTime(AV_NOPTS_VALUE),
    m_startDateTime(AV_NOPTS_VALUE),
    m_stopOnWriteError(true),
    m_currentTimeZone(-1),
    m_waitEOF(false),
    m_forceDefaultCtx(true),
    m_formatCtx(0),
    m_packetWrited(false),
    m_startOffset(0),
    m_prebufferingUsec(0),
    m_EofDateTime(AV_NOPTS_VALUE),
    m_endOfData(false),
    m_lastProgress(-1),
    m_needCalcSignature(false),
    m_mdctx(EXPORT_SIGN_METHOD),
    m_container(QLatin1String("matroska")),
    m_videoChannels(0),
    m_ioContext(0),
    m_needReopen(false),
    m_isAudioPresent(false),
    m_audioTranscoder(0),
    m_dstAudioCodec(CODEC_ID_NONE),
    m_dstVideoCodec(CODEC_ID_NONE),
    m_onscreenDateOffset(0),
    m_role(Role_ServerRecording),
    m_serverTimeZoneMs(Qn::InvalidUtcOffset),
    m_nextIFrameTime(AV_NOPTS_VALUE),
    m_truncateIntervalEps(0)
{
    srand(QDateTime::currentMSecsSinceEpoch());
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
    memset(m_motionFileList, 0, sizeof(m_motionFileList));
    memset(m_videoTranscoder, 0, sizeof(m_videoTranscoder));
}

QnStreamRecorder::~QnStreamRecorder()
{
    stop();
    close();
    delete m_audioTranscoder;
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        delete m_videoTranscoder[i];
}

/* It is impossible to write avi/mkv attribute in the end
*  So, write magic on startup, then update it
*/
void QnStreamRecorder::updateSignatureAttr()
{
    QIODevice* file = m_storage->open(m_fileName, QIODevice::ReadWrite);
    if (!file)
        return;
    QByteArray data = file->read(1024*16);
    int pos = data.indexOf(QnSignHelper::getSignMagic());
    if (pos == -1) {
        delete file;
        return; // no signature found
    }
    file->seek(pos);
    file->write(QnSignHelper::getSignFromDigest(getSignature()));
    delete file;
}

void QnStreamRecorder::close()
{
    if (m_formatCtx) 
    {
        if (m_packetWrited)
            av_write_trailer(m_formatCtx);

        if (m_startDateTime != qint64(AV_NOPTS_VALUE))
        {
            qint64 fileDuration = m_startDateTime != qint64(AV_NOPTS_VALUE)  ? m_endDateTime/1000 - m_startDateTime/1000 : 0; // bug was here! rounded sum is not same as rounded summand!
            fileFinished(fileDuration, m_fileName, m_mediaProvider, QnFfmpegHelper::getFileSizeByIOContext(m_ioContext));
        }

        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
        {
            if (m_formatCtx->streams[i]->codec->codec)
                avcodec_close(m_formatCtx->streams[i]->codec);
        }
        
        if (m_ioContext)
        {
            QnFfmpegHelper::closeFfmpegIOContext(m_ioContext);
#ifndef SIGN_FRAME_ENABLED
            if (m_needCalcSignature)
                updateSignatureAttr();
#endif
            m_ioContext = 0;
            if (m_formatCtx)
                m_formatCtx->pb = 0;
        }

        if (m_formatCtx) 
            avformat_close_input(&m_formatCtx);
        m_formatCtx = 0;

    }
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        if (m_motionFileList[i])
            m_motionFileList[i]->close();
    }
    m_packetWrited = false;
    m_endDateTime = m_startDateTime = AV_NOPTS_VALUE;

    markNeedKeyData();
    m_firstTime = true;
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
        QnAbstractMediaDataPtr d;
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
        QnAbstractMediaDataPtr media = m_prebuffer.at(i);
        if (media->dataType == QnAbstractMediaData::VIDEO && media->timestamp > baseTime && (media->flags & AV_PKT_FLAG_KEY))
            return media->timestamp;
    }
    return AV_NOPTS_VALUE;
}

bool QnStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    if (m_needReopen)
    {
        m_needReopen = false;
        close();
    }

    QnAbstractMediaDataPtr md = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!md)
        return true; // skip unknown data

    if (m_EofDateTime != qint64(AV_NOPTS_VALUE) && md->timestamp > m_EofDateTime)
    {
        if (!m_endOfData) 
        {
            bool isOK = true;
            if (m_needCalcSignature && !m_firstTime)
                isOK = addSignatureFrame(m_lastErrMessage);
            if (isOK)
                emit recordingFinished(m_fileName);
            else
                emit recordingFailed(m_lastErrMessage);
            m_endOfData = true;
        }

        pleaseStop();
        return true;
    }

    if (md->dataType == QnAbstractMediaData::META_V1)
    {
        if (needSaveData(md))
            saveData(md);
    }
    else {
        m_prebuffer.push(md);
        if (m_prebufferingUsec == 0) 
        {
            m_nextIFrameTime = AV_NOPTS_VALUE;
            while (!m_prebuffer.isEmpty())
            {
                QnAbstractMediaDataPtr d;
                m_prebuffer.pop(d);
                if (needSaveData(d))
                    saveData(d);
                else if (md->dataType == QnAbstractMediaData::VIDEO)
                    markNeedKeyData();
            }
        }
        else 
        {
            bool isKeyFrame = md->dataType == QnAbstractMediaData::VIDEO && (md->flags & AV_PKT_FLAG_KEY);
            if (m_nextIFrameTime == (qint64)AV_NOPTS_VALUE && isKeyFrame)
                m_nextIFrameTime = md->timestamp;

            if (m_nextIFrameTime != (qint64)AV_NOPTS_VALUE && md->timestamp - m_nextIFrameTime >= m_prebufferingUsec)
            {
                while (!m_prebuffer.isEmpty() && m_prebuffer.front()->timestamp < m_nextIFrameTime)
                {
                    QnAbstractMediaDataPtr d;
                    m_prebuffer.pop(d);
                    if (needSaveData(d))
                        saveData(d);
                    else if (md->dataType == QnAbstractMediaData::VIDEO) {
                        markNeedKeyData();
                    }
                }
                m_nextIFrameTime = findNextIFrame(m_nextIFrameTime);
            }
        }
    }

    if (m_waitEOF && m_dataQueue.size() == 0) {
        close();
        m_waitEOF = false;
    }

    if (m_EofDateTime != qint64(AV_NOPTS_VALUE) && m_EofDateTime > m_startDateTime)
    {
        int progress = ((md->timestamp - m_startDateTime)*100ll) /(m_EofDateTime - m_startDateTime);
        if (progress != m_lastProgress) {
            emit recordingProgress(progress);
            m_lastProgress = progress;
        }
    }

    return true;
}

bool QnStreamRecorder::saveData(QnAbstractMediaDataPtr md)
{
    if (md->dataType == QnAbstractMediaData::META_V1)
        return saveMotion(md.dynamicCast<QnMetaDataV1>());

    if (m_endDateTime != qint64(AV_NOPTS_VALUE) && md->timestamp - m_endDateTime > MAX_FRAME_DURATION*2*1000ll && m_truncateInterval > 0) {
        // if multifile recording allowed, recreate file if recording hole is detected
        qDebug() << "Data hole detected for camera" << m_device->getUniqueId() << ". Diff between packets=" << (md->timestamp - m_endDateTime)/1000 << "ms";
        close();
    }
    else if (m_startDateTime != qint64(AV_NOPTS_VALUE)) {
        if (md->timestamp - m_startDateTime > m_truncateInterval*3 && m_truncateInterval > 0) {
            // if multifile recording allowed, recreate file if recording hole is detected
            qDebug() << "Too long time when no I-frame detected (file length exceed " << (md->timestamp - m_startDateTime)/1000000 << "sec. Close file";
            close();
        }
        else if (md->timestamp < m_startDateTime - 1000ll*1000) {
            qDebug() << "Time translated into the past for " << (md->timestamp - m_startDateTime)/1000000 << "sec. Close file";
            close();
        }
    }

    if (md->dataType == QnAbstractMediaData::AUDIO && m_truncateInterval > 0)
    {
        QnCompressedAudioDataPtr ad = qSharedPointerDynamicCast<QnCompressedAudioData>(md);
        QnCodecAudioFormat audioFormat(ad->context);
        if (!m_firstTime && audioFormat != m_prevAudioFormat) {
            close(); // restart recording file if audio format is changed
        }
        m_prevAudioFormat = audioFormat; 
    }
    
    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(md);
    //if (!vd)
    //    return true; // ignore audio data

    if (vd && !m_gotKeyFrame[vd->channelNumber] && !(vd->flags & AV_PKT_FLAG_KEY))
        return true; // skip data

    if (m_firstTime)
    {
        if (vd == 0)
            return true; // skip audio packets before first video packet
        if (!initFfmpegContainer(vd))
        {
            if (!m_fileName.isEmpty())
                emit recordingFailed(m_lastErrMessage); // ommit storageFailure because of exists separate error if no storages at all
            if (m_stopOnWriteError)
            {
                m_needStop = true;
                return false;
            }
            else {
                return true; // ignore data
            }
        }

        m_firstTime = false;
        emit recordingStarted();
    }

    int channel = md->channelNumber;

    if (md->flags & AV_PKT_FLAG_KEY)
    {
        m_gotKeyFrame[channel] = true;
        if (m_truncateInterval > 0 && md->timestamp - m_startDateTime > (m_truncateInterval+m_truncateIntervalEps))
        {
            m_endDateTime = md->timestamp;
            close();
            m_endDateTime = m_startDateTime = md->timestamp;

            return saveData(md);
        }
    }

    int streamIndex = channel;
    //if (md->dataType == QnAbstractMediaData::AUDIO)
    //    streamIndex += m_videoChannels;

    if ((uint)streamIndex >= m_formatCtx->nb_streams)
        return true; // skip packet

    m_endDateTime = md->timestamp;

    if (md->dataType == QnAbstractMediaData::AUDIO && m_audioTranscoder)
    {
        QnAbstractMediaDataPtr result;
        do {
            m_audioTranscoder->transcodePacket(md, &result);
            if (result && result->data.size() > 0)
                writeData(result, streamIndex);
            md.clear();
        } while (result);
    }
    else if (md->dataType == QnAbstractMediaData::VIDEO && m_videoTranscoder[md->channelNumber])
    {
        QnAbstractMediaDataPtr result;
        m_videoTranscoder[md->channelNumber]->transcodePacket(md, &result);
        if (result && result->data.size() > 0)
            writeData(result, streamIndex);
    }
    else {
        writeData(md, streamIndex);
    }

    return true;
}

void QnStreamRecorder::writeData(QnAbstractMediaDataPtr md, int streamIndex)
{
    AVRational srcRate = {1, 1000000};
    AVStream* stream = m_formatCtx->streams[streamIndex];
    Q_ASSERT(stream->time_base.num && stream->time_base.den);

    Q_ASSERT(md->timestamp >= 0);

    AVPacket avPkt;
    av_init_packet(&avPkt);

    qint64 dts = av_rescale_q(md->timestamp-m_startDateTime, srcRate, stream->time_base);
    if (stream->cur_dts > 0)
        avPkt.dts = qMax((qint64)stream->cur_dts+1, dts);
    else
        avPkt.dts = dts;
    QnCompressedVideoDataPtr video = md.dynamicCast<QnCompressedVideoData>();
    if (video && (quint64)video->pts != AV_NOPTS_VALUE)
        avPkt.pts = av_rescale_q(video->pts-m_startDateTime, srcRate, stream->time_base) + (avPkt.dts-dts);
    else
        avPkt.pts = avPkt.dts;

    if(md->flags & AV_PKT_FLAG_KEY)
        avPkt.flags |= AV_PKT_FLAG_KEY;
    avPkt.data = (quint8*) md->data.data();
    avPkt.size = md->data.size();
    avPkt.stream_index= streamIndex;

    if (av_write_frame(m_formatCtx, &avPkt) < 0) 
        cl_log.log(QLatin1String("AV packet write error"), cl_logWARNING);
    else {
        m_packetWrited = true;
        if (m_needCalcSignature) 
        {
            if (md->dataType == QnAbstractMediaData::VIDEO && (md->flags & AV_PKT_FLAG_KEY))
                m_lastIFrame = md.dynamicCast<QnCompressedVideoData>();
            AVCodecContext* srcCodec = m_formatCtx->streams[streamIndex]->codec;
            QnSignHelper::updateDigest(srcCodec, m_mdctx, avPkt.data, avPkt.size);
            //EVP_DigestUpdate(m_mdctx, (const char*)avPkt.data, avPkt.size);
        }
    }
}

void QnStreamRecorder::endOfRun()
{
    close();
}

bool QnStreamRecorder::initFfmpegContainer(QnCompressedVideoDataPtr mediaData)
{
    m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (mediaData->dataProvider);
    Q_ASSERT(m_mediaProvider);
    Q_ASSERT(mediaData->flags & AV_PKT_FLAG_KEY);

    m_endDateTime = m_startDateTime = mediaData->timestamp;

    //QnResourcePtr resource = mediaData->dataProvider->getResource();
    // allocate container
    AVOutputFormat * outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = tr("No %1 container in FFMPEG library.").arg(m_container);
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    m_currentTimeZone = currentTimeZone()/60;
    QString fileExt = QString(QLatin1String(outputCtx->extensions)).split(QLatin1Char(','))[0];
    m_fileName = fillFileName(m_mediaProvider);
    if (m_fileName.isEmpty()) 
        return false;

    QString dFileExt = QLatin1Char('.') + fileExt;
    if (!m_fileName.endsWith(dFileExt, Qt::CaseInsensitive))
        m_fileName += dFileExt;
    QString url = m_fileName; 

    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, url.toUtf8().constData());

    if (err < 0) {
        m_lastErrMessage = tr("Can't create output file '%1' for video recording.").arg(m_fileName);
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QnMediaResourcePtr mediaDev = qSharedPointerDynamicCast<QnMediaResource>(m_device);
    const QnResourceVideoLayout* layout = mediaDev->getVideoLayout(m_mediaProvider);
    QString layoutStr = QnArchiveStreamReader::serializeLayout(layout);
    {
        av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_LayoutInfo, fileExt), layoutStr.toAscii().data(), 0);
        qint64 startTime = m_startOffset+mediaData->timestamp/1000;
        av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_startTime, fileExt), QString::number(startTime).toAscii().data(), 0);
        av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_Software, fileExt), "Network Optix", 0);
#ifndef SIGN_FRAME_ENABLED
        if (m_needCalcSignature) {
            QByteArray signPattern = QnSignHelper::getSignPattern();
            if (m_serverTimeZoneMs != Qn::InvalidUtcOffset)
                signPattern.append(QByteArray::number(m_serverTimeZoneMs)); // add server timeZone as one more column to sign pattern
            while (signPattern.size() < QnSignHelper::getMaxSignSize())
                signPattern.append(" ");
            signPattern = signPattern.mid(0, QnSignHelper::getMaxSignSize());
            av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_Signature, fileExt), signPattern.data(), 0);
        }
#endif

        m_formatCtx->start_time = mediaData->timestamp;

        // m_forceDefaultCtx: for server archive, if file is recreated - we need to use default context.
        // for exporting AVI files we must use original context, so need to reset "force" for exporting purpose
        bool isTranscode = m_role == Role_FileExportWithTime || 
            (m_dstVideoCodec != CODEC_ID_NONE && m_dstVideoCodec != mediaData->compressionType) || 
            !m_srcRect.isEmpty();


        m_videoChannels = isTranscode ? 1 : layout->channelCount();
        int bottomRightChannel = 0;
        int hPos = -1, vPos = -1;
        for (int i = 0; i < m_videoChannels; ++i) 
        {
            QPoint position = layout->position(i);
            if (position.x() >= hPos && position.y() >= vPos) {
                bottomRightChannel = i;
                hPos = position.x();
                vPos = position.y();
            }
        }

        for (int i = 0; i < m_videoChannels; ++i) 
        {
            // TODO: #vasilenko avoid using deprecated methods
            AVStream* videoStream = av_new_stream(m_formatCtx, DEFAULT_VIDEO_STREAM_ID+i);
            if (videoStream == 0)
            {
                m_lastErrMessage = tr("Can't allocate output stream for recording.");
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            AVCodecContext* videoCodecCtx = videoStream->codec;
            videoCodecCtx->codec_id = mediaData->compressionType;
            videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            if (mediaData->compressionType == CODEC_ID_MJPEG)
                videoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
            else
                videoCodecCtx->pix_fmt = PIX_FMT_YUV420P;


            if (isTranscode)
            {
                // transcode video
                if (m_dstVideoCodec == CODEC_ID_NONE)
                    m_dstVideoCodec = CODEC_ID_MPEG4; // default value
                m_videoTranscoder[i] = new QnFfmpegVideoTranscoder(m_dstVideoCodec);
                m_videoTranscoder[i]->setMTMode(true);
                if (m_role == Role_FileExportWithTime && i == bottomRightChannel)
                    m_videoTranscoder[i]->setDrawDateTime(QnFfmpegVideoTranscoder::Date_RightBottom);
                m_videoTranscoder[i]->setOnScreenDateOffset(m_onscreenDateOffset);
                m_videoTranscoder[i]->setQuality(QnQualityHighest);
                if (!m_srcRect.isEmpty())
                    m_videoTranscoder[i]->setSrcRect(m_srcRect);
                m_videoTranscoder[i]->setVideoLayout(layout);
                m_videoTranscoder[i]->open(mediaData);

                avcodec_copy_context(videoStream->codec, m_videoTranscoder[i]->getCodecContext());
            }
            else if (!m_forceDefaultCtx && mediaData->context && mediaData->context->ctx()->width > 0)
            {
                AVCodecContext* srcContext = mediaData->context->ctx();
                avcodec_copy_context(videoCodecCtx, srcContext);
            }
            else if (m_role == Role_FileExport || m_role == Role_FileExportWithEmptyContext)
            {
                // determine real width and height
                QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );
                CLFFmpegVideoDecoder decoder(mediaData->compressionType, mediaData, false);
                decoder.decode(mediaData, &outFrame);
                if (m_role == Role_FileExport) 
                {
                    avcodec_copy_context(videoStream->codec, decoder.getContext());
                }
                else {
                    videoCodecCtx->width = decoder.getWidth();
                    videoCodecCtx->height = decoder.getHeight();
                }
            }
            else {
                videoCodecCtx->width = qMax(8,mediaData->width);
                videoCodecCtx->height = qMax(8,mediaData->height);
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

        const QnResourceAudioLayout* audioLayout = mediaDev->getAudioLayout(m_mediaProvider);
        m_isAudioPresent = false;
        for (int i = 0; i < audioLayout->channelCount(); ++i) 
        {
            m_isAudioPresent = true;
            // TODO: #vasilenko avoid using deprecated methods
            AVStream* audioStream = av_new_stream(m_formatCtx, DEFAULT_AUDIO_STREAM_ID+i);
            if (audioStream == 0)
            {
                m_lastErrMessage = tr("Can't allocate output audio stream.");
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            CodecID srcAudioCodec = CODEC_ID_NONE;
            QnMediaContextPtr mediaContext = audioLayout->getAudioTrackInfo(i).codecContext.dynamicCast<QnMediaContext>();
            if (!mediaContext) {
                m_lastErrMessage = tr("Internal server error: invalid audio codec information");
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            srcAudioCodec = mediaContext->ctx()->codec_id;

            if (m_dstAudioCodec == CODEC_ID_NONE || m_dstAudioCodec == srcAudioCodec)
            {
                avcodec_copy_context(audioStream->codec, mediaContext->ctx());

                // avoid FFMPEG bug for MP3 mono. block_align hardcoded inside ffmpeg for stereo channels and it is cause problem
                if (srcAudioCodec == CODEC_ID_MP3 && audioStream->codec->channels == 1)
                    audioStream->codec->block_align = 0; 
            }
            else {
                // transcode audio
                m_audioTranscoder = new QnFfmpegAudioTranscoder(m_dstAudioCodec);
                m_audioTranscoder->open(mediaContext);
                avcodec_copy_context(audioStream->codec, m_audioTranscoder->getCodecContext());
            }
            audioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            audioStream->first_dts = 0;
        }

        m_formatCtx->pb = m_ioContext = QnFfmpegHelper::createFfmpegIOContext(m_storage, url, QIODevice::WriteOnly);
        if (m_ioContext == 0)
        {
            avformat_close_input(&m_formatCtx);
            m_lastErrMessage = tr("Can't create output file '%1'.").arg(url);
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }

        int rez = avformat_write_header(m_formatCtx, 0);
        if (rez < 0) 
        {
            QnFfmpegHelper::closeFfmpegIOContext(m_ioContext);
            m_ioContext = 0;
            m_formatCtx->pb = 0;
            avformat_close_input(&m_formatCtx);
            m_lastErrMessage = tr("Video or audio codec is incompatible with %1 format. Try another format.").arg(m_container);
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }
    }
    fileStarted(m_startDateTime/1000, m_currentTimeZone, m_fileName, m_mediaProvider);
    if (m_truncateInterval > 4000000ll)
        m_truncateIntervalEps = (qrand() % (m_truncateInterval/4000000ll)) * 1000000ll;

    return true;
}

void QnStreamRecorder::setTruncateInterval(int seconds)
{
    m_truncateInterval = seconds * 1000000ll;
}

void QnStreamRecorder::setFileName(const QString& fileName)
{
    m_fixedFileName = fileName;
}

void QnStreamRecorder::setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS])
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionFileList[i] = motionFileList[i];
}

void QnStreamRecorder::setStartOffset(qint64 value)
{
    m_startOffset = value;
}

void QnStreamRecorder::setPrebufferingUsec(int value)
{
    m_prebufferingUsec = value;
}

int QnStreamRecorder::getPrebufferingUsec() const
{
    return m_prebufferingUsec;
}


bool QnStreamRecorder::needSaveData(QnAbstractMediaDataPtr media)
{
    Q_UNUSED(media)
    return true;
}

bool QnStreamRecorder::saveMotion(QnMetaDataV1Ptr motion)
{
    if (motion && !motion->isEmpty() && m_motionFileList[motion->channelNumber])
        motion->serialize(m_motionFileList[motion->channelNumber].data());
    return true;
}

QString QnStreamRecorder::fillFileName(QnAbstractMediaStreamDataProvider* provider)
{
    Q_UNUSED(provider);
    if (m_storage == 0)
        m_storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(m_fixedFileName));
    return m_fixedFileName;
}

QString QnStreamRecorder::fixedFileName() const
{
    return m_fixedFileName;
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

    if (value) {
        m_mdctx.reset();
        m_mdctx.addData(EXPORT_SIGN_MAGIC, sizeof(EXPORT_SIGN_MAGIC));
    }
}

QByteArray QnStreamRecorder::getSignature() const
{
    return m_mdctx.result();
}

bool QnStreamRecorder::addSignatureFrame(QString &/*errorString*/)
{
#ifndef SIGN_FRAME_ENABLED
    QByteArray signText = QnSignHelper::getSignPattern();
    if (m_serverTimeZoneMs != Qn::InvalidUtcOffset)
        signText.append(QByteArray::number(m_serverTimeZoneMs)); // I've included server timezone to sign to prevent modification this attribute
    QnSignHelper::updateDigest(0, m_mdctx, (const quint8*) signText.data(), signText.size());
#else
    AVCodecContext* srcCodec = m_formatCtx->streams[0]->codec;
    QnSignHelper signHelper;
    signHelper.setLogo(QPixmap::fromImage(m_logo));
    signHelper.setSign(getSignature());
    QnCompressedVideoDataPtr generatedFrame = signHelper.createSgnatureFrame(srcCodec, m_lastIFrame);

    if (generatedFrame == 0)
    {
        m_lastErrMessage = tr("Error during watermark generation for file '%1'.").arg(m_fileName);
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

void QnStreamRecorder::setRole(Role role)
{
    m_role = role;
    m_forceDefaultCtx = m_role == Role_ServerRecording || m_role == Role_FileExportWithEmptyContext;
}

void QnStreamRecorder::setSignLogo(const QImage& logo)
{
    m_logo = logo;
}

void QnStreamRecorder::setStorage(QnStorageResourcePtr storage)
{
    m_storage = storage;
}

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

void QnStreamRecorder::setAudioCodec(CodecID codec)
{
    m_dstAudioCodec = codec;
}

void QnStreamRecorder::setOnScreenDateOffset(int timeOffsetMs)
{
    m_onscreenDateOffset = timeOffsetMs;
}

void QnStreamRecorder::setServerTimeZoneMs(int value)
{
    m_serverTimeZoneMs = value;
}

void QnStreamRecorder::setSrcRect(const QRectF& srcRect)
{
    m_srcRect = srcRect;
}
