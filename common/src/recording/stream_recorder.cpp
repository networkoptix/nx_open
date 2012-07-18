#include "stream_recorder.h"
#include "core/resource/resource_consumer.h"
#include "core/datapacket/datapacket.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "utils/common/util.h"
#include "decoders/video/ffmpeg.h"
#include "export/sign_helper.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4352;

static const int STORE_QUEUE_SIZE = 50;

extern QMutex global_ffmpeg_mutex;

QnStreamRecorder::QnStreamRecorder(QnResourcePtr dev):
    QnAbstractDataConsumer(STORE_QUEUE_SIZE),
    m_device(dev),
    m_firstTime(true),
    m_packetWrited(false),
    m_formatCtx(0),
    m_truncateInterval(0),
    m_startDateTime(AV_NOPTS_VALUE),
    m_endDateTime(AV_NOPTS_VALUE),
    m_startOffset(0),
    m_forceDefaultCtx(true),
    m_prebufferingUsec(0),
    m_stopOnWriteError(true),
    m_waitEOF(false),
    m_endOfData(false),
    m_lastProgress(-1),
    m_EofDateTime(AV_NOPTS_VALUE),
    m_needCalcSignature(false),
    m_mdctx(EXPORT_SIGN_METHOD),
    m_container("matroska"),
    m_videoChannels(0),
    m_ioContext(0),
    m_needReopen(false),
    m_isAudioPresent(false)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
}

QnStreamRecorder::~QnStreamRecorder()
{
    stop();
	close();
}

void QnStreamRecorder::close()
{
    if (m_formatCtx) 
    {
		if (m_packetWrited)
			av_write_trailer(m_formatCtx);

        if (m_startDateTime != AV_NOPTS_VALUE)
        {
            qint64 fileDuration = m_startDateTime != AV_NOPTS_VALUE  ? (m_endDateTime - m_startDateTime)/1000 : 0;
            fileFinished(fileDuration, m_fileName, m_mediaProvider, m_storage->getFileSizeByIOContext(m_ioContext));
        }

        QMutexLocker mutex(&global_ffmpeg_mutex);
        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
        {
            if (m_formatCtx->streams[i]->codec->codec)
                avcodec_close(m_formatCtx->streams[i]->codec);
        }
        
        if (m_ioContext)
        {
            m_storage->closeFfmpegIOContext(m_ioContext);
            m_ioContext = 0;
            if (m_formatCtx)
                m_formatCtx->pb = 0;
        }

        if (m_formatCtx) 
            avformat_close_input(&m_formatCtx);
        m_formatCtx = 0;

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
        QnAbstractMediaDataPtr d = m_prebuffer.dequeue();
        if (needSaveData(d))
            saveData(d);
        else
            markNeedKeyData();
    }
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

    if (m_EofDateTime != AV_NOPTS_VALUE && md->timestamp > m_EofDateTime)
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

        stop();
        return true;
    }

    m_prebuffer << md;
    while (!m_prebuffer.isEmpty() && md->timestamp-m_prebuffer.first()->timestamp >= m_prebufferingUsec)
    {
        QnAbstractMediaDataPtr d = m_prebuffer.dequeue();
        if (needSaveData(d))
            saveData(d);
        else
            markNeedKeyData();
    }

    if (m_waitEOF && m_dataQueue.size() == 0) {
        close();
        m_waitEOF = false;
    }

    if (m_EofDateTime != AV_NOPTS_VALUE && m_EofDateTime > m_startDateTime)
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
    if (m_endDateTime != AV_NOPTS_VALUE && md->timestamp - m_endDateTime > MAX_FRAME_DURATION*2*1000ll && m_truncateInterval > 0) {
        // if multifile recording allowed, recreate file if recording hole is detected
        qDebug() << "Data hole detected for camera" << m_device->getUniqueId() << ". Diff between packets=" << (md->timestamp - m_endDateTime)/1000 << "ms";
        close();
    }
    else if (m_startDateTime != AV_NOPTS_VALUE && md->timestamp - m_startDateTime > m_truncateInterval*3 && m_truncateInterval > 0) {
        // if multifile recording allowed, recreate file if recording hole is detected
        qDebug() << "Too long time when no I-frame detected (file length exceed " << (md->timestamp - m_startDateTime)/1000000 << "sec. Close file";
        close();
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
    

    if (md->dataType == QnAbstractMediaData::META_V1)
        return saveMotion(md);

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
            emit recordingFailed(m_lastErrMessage);
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
        if (m_truncateInterval > 0 && md->timestamp - m_startDateTime > m_truncateInterval)
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

    if (streamIndex >= m_formatCtx->nb_streams)
        return true; // skip packet

    AVPacket avPkt;
    av_init_packet(&avPkt);
    AVStream* stream = m_formatCtx->streams[streamIndex];
    AVRational srcRate = {1, 1000000};
    Q_ASSERT(stream->time_base.num && stream->time_base.den);

    Q_ASSERT(md->timestamp >= 0);

    m_endDateTime = md->timestamp;

    avPkt.pts = av_rescale_q(md->timestamp-m_startDateTime, srcRate, stream->time_base);
    avPkt.dts = avPkt.pts;
    if(md->flags & AV_PKT_FLAG_KEY)
        avPkt.flags |= AV_PKT_FLAG_KEY;
    avPkt.stream_index= streamIndex;
    avPkt.data = (quint8*) md->data.data();
    avPkt.size = md->data.size();

    if (av_write_frame(m_formatCtx, &avPkt) < 0) 
        cl_log.log(QLatin1String("AV packet write error"), cl_logWARNING);
    else {
        m_packetWrited = true;
        if (m_needCalcSignature) 
        {
            AVCodecContext* srcCodec = m_formatCtx->streams[0]->codec;
            QnSignHelper::updateDigest(srcCodec, m_mdctx, avPkt.data, avPkt.size);
            //EVP_DigestUpdate(m_mdctx, (const char*)avPkt.data, avPkt.size);
        }
    }

    return true;
};

void QnStreamRecorder::endOfRun()
{
	close();
}

bool QnStreamRecorder::initFfmpegContainer(QnCompressedVideoDataPtr mediaData)
{
    m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (mediaData->dataProvider);
    Q_ASSERT(m_mediaProvider);

    m_endDateTime = m_startDateTime = mediaData->timestamp;

    //QnResourcePtr resource = mediaData->dataProvider->getResource();
    // allocate container
    AVOutputFormat * outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = QString("No %1 container in FFMPEG library").arg(m_container);
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QString fileExt = QString(outputCtx->extensions).split(',')[0];
    m_fileName = fillFileName(m_mediaProvider);
    if (m_fileName.isEmpty()) 
        return false;

    QString dFileExt = QString(".") + fileExt;
    if (!m_fileName.endsWith(dFileExt, Qt::CaseInsensitive))
        m_fileName += dFileExt;
    QString url = m_fileName; 

    global_ffmpeg_mutex.lock();
    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, url.toUtf8().constData());
    global_ffmpeg_mutex.unlock();

    if (err < 0) {
        m_lastErrMessage = QString("Can't create output file '") + m_fileName + QString("' for video recording.");
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QnMediaResourcePtr mediaDev = qSharedPointerDynamicCast<QnMediaResource>(m_device);
    const QnVideoResourceLayout* layout = mediaDev->getVideoLayout(m_mediaProvider);
    QString layoutStr = QnArchiveStreamReader::serializeLayout(layout);

    {
        QMutexLocker mutex(&global_ffmpeg_mutex);
        
        av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_LayoutInfo, fileExt), layoutStr.toAscii().data(), 0);
        qint64 startTime = m_startOffset+mediaData->timestamp/1000;
        av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_startTime, fileExt), QString::number(startTime).toAscii().data(), 0);
        av_dict_set(&m_formatCtx->metadata, QnAviArchiveDelegate::getTagName(QnAviArchiveDelegate::Tag_Software, fileExt), "Network Optix", 0);

        m_formatCtx->start_time = mediaData->timestamp;

        m_videoChannels = layout->numberOfChannels();

        for (unsigned int i = 0; i < m_videoChannels; ++i) 
        {
            AVStream* videoStream = av_new_stream(m_formatCtx, DEFAULT_VIDEO_STREAM_ID+i);
            if (videoStream == 0)
            {
                m_lastErrMessage = "Can't allocate output stream for recording.";
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            AVCodecContext* videoCodecCtx = videoStream->codec;

            // m_forceDefaultCtx: for server archive, if file is recreated - we need to use default context.
            // for exporting AVI files we must use original context, so need to reset "force" for exporting purpose
            
            if (!m_forceDefaultCtx && mediaData->context && mediaData->context->ctx()->width > 0)
            {
                AVCodecContext* srcContext = mediaData->context->ctx();
                avcodec_copy_context(videoCodecCtx, srcContext);
            }
            else if (m_role == Role_FileExport)
            {
                // determine real width and height
                CLVideoDecoderOutput outFrame;
                CLFFmpegVideoDecoder decoder(mediaData->compressionType, mediaData, false);
                decoder.decode(mediaData, &outFrame);
                avcodec_copy_context(videoStream->codec, decoder.getContext());
            }
            else {
                videoCodecCtx->codec_id = mediaData->compressionType;
                videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
                videoCodecCtx->width = qMax(8,mediaData->width);
                videoCodecCtx->height = qMax(8,mediaData->height);
                
                if (mediaData->compressionType == CODEC_ID_MJPEG)
                    videoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
                else
                    videoCodecCtx->pix_fmt = PIX_FMT_YUV420P;
            }
            videoCodecCtx->bit_rate = 1000000 * 6;
            videoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
            AVRational defaultFrameRate = {1, 30};
            videoCodecCtx->time_base = defaultFrameRate;

            videoCodecCtx->sample_aspect_ratio.num = 1;
            videoCodecCtx->sample_aspect_ratio.den = 1;

            videoStream->sample_aspect_ratio = videoCodecCtx->sample_aspect_ratio;
            videoStream->first_dts = 0;
        }

        const QnResourceAudioLayout* audioLayout = mediaDev->getAudioLayout(m_mediaProvider);
        m_isAudioPresent = false;
        for (unsigned int i = 0; i < audioLayout->numberOfChannels(); ++i) 
        {
            m_isAudioPresent = true;
            AVStream* audioStream = av_new_stream(m_formatCtx, DEFAULT_AUDIO_STREAM_ID+i);
            if (audioStream == 0)
            {
                m_lastErrMessage = "Can't allocate output audio stream";
                cl_log.log(m_lastErrMessage, cl_logERROR);
                return false;
            }

            avcodec_copy_context(audioStream->codec, audioLayout->getAudioTrackInfo(i).codecContext->ctx());
            audioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            audioStream->first_dts = 0;
        }

        m_formatCtx->pb = m_ioContext = m_storage->createFfmpegIOContext(url, QIODevice::WriteOnly);
        if (m_ioContext == 0)
        {
            avformat_close_input(&m_formatCtx);
            m_lastErrMessage = QString("Can't create output file '%1'").arg(url);
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }

        int rez = avformat_write_header(m_formatCtx, 0);
        if (rez < 0) {
            avformat_close_input(&m_formatCtx);
            m_lastErrMessage = QString("Video or audio codec is incompatible with %1 format. Try other format").arg(m_container);
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }
    }
    fileStarted(m_startDateTime/1000, m_fileName, m_mediaProvider);

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
    return true;
}

bool QnStreamRecorder::saveMotion(QnAbstractMediaDataPtr media)
{
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
    if (value)
    {
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

    AVCodecContext* srcCodec = m_formatCtx->streams[0]->codec;
    QnSignHelper signHelper;
    signHelper.setLogo(m_logo);
    signHelper.setSign(getSignature());
    QnCompressedVideoDataPtr generatedFrame = signHelper.createSgnatureFrame(srcCodec);

    if (generatedFrame == 0)
    {
        m_lastErrMessage = QString("Error during generate watermark for file ") + m_fileName;
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


    return true;
}

void QnStreamRecorder::setRole(Role role)
{
    m_role = role;
    m_forceDefaultCtx = m_role == Role_ServerRecording;
}

void QnStreamRecorder::setSignLogo(QPixmap logo)
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
    if (m_container == QString("mkv"))
        m_container = "matroska";
}

void QnStreamRecorder::setNeedReopen()
{
    m_needReopen = true;
}

bool QnStreamRecorder::isAudioPresent() const
{
    return m_isAudioPresent;
}
