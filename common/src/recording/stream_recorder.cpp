#include "stream_recorder.h"
#include "core/resource/resource_consumer.h"
#include "core/datapacket/datapacket.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "utils/common/util.h"

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4352;

static const int STORE_QUEUE_SIZE = 50;

extern QMutex global_ffmpeg_mutex;

QnStreamRecorder::QnStreamRecorder(QnResourcePtr dev):
QnAbstractDataConsumer(STORE_QUEUE_SIZE),
m_device(dev),
m_firstTime(true),
m_packetWrited(false),
m_firstTimestamp(-1),
m_formatCtx(0),
m_truncateInterval(0),
m_currentChunkLen(0),
m_startDateTime(AV_NOPTS_VALUE),
m_endDateTime(-1),
m_lastPacketTime(AV_NOPTS_VALUE),
m_startOffset(0),
m_forceDefaultCtx(true),
m_prebufferingUsec(0),
m_stopOnWriteError(true),
m_waitEOF(false),
m_endOfData(false),
m_lastProgress(-1),
m_EofDateTime(AV_NOPTS_VALUE)
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
        if (m_startDateTime != AV_NOPTS_VALUE)
        {
            fileFinished((m_endDateTime - m_startDateTime)/1000, m_fileName);
            m_startDateTime = AV_NOPTS_VALUE;
        }

        if (m_packetWrited)
            av_write_trailer(m_formatCtx);

        QMutexLocker mutex(&global_ffmpeg_mutex);
        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
        {
            if (m_formatCtx->streams[i]->codec->codec)
                avcodec_close(m_formatCtx->streams[i]->codec);
        }
        if (m_formatCtx->pb)
            avio_close(m_formatCtx->pb);
        avformat_free_context(m_formatCtx);
        m_formatCtx = 0;
        m_lastPacketTime = AV_NOPTS_VALUE;
    }
    m_packetWrited = false;
    m_firstTimestamp = -1;

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
    QMutexLocker lock(&m_closeAsyncMutex);
    QnAbstractMediaDataPtr md = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!md)
        return true; // skip unknown data

    if (m_EofDateTime != AV_NOPTS_VALUE && md->timestamp > m_EofDateTime)
    {
        if (!m_endOfData)
            emit recordingFinished(m_fileName);
        m_endOfData = true;
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
    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(md);

    if (m_lastPacketTime != AV_NOPTS_VALUE)
    {
        if (md->timestamp - m_lastPacketTime > MAX_FRAME_DURATION*1000ll && m_currentChunkLen > 0) {
            // if multifile recording allowed, recreate file if recording hole is detected
            close();
            m_lastPacketTime = md->timestamp;
        }
    }

    if (md->dataType == QnAbstractMediaData::META_V1)
        return saveMotion(md);

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
    if (channel >= m_formatCtx->nb_streams)
        return true; // skip packet

    if (m_firstTimestamp == -1)
        m_firstTimestamp = md->timestamp;

    m_endDateTime = md->timestamp;

    if (md->flags & AV_PKT_FLAG_KEY)
    {
        m_gotKeyFrame[channel] = true;
        if (m_truncateInterval > 0 && md->timestamp - m_firstTimestamp > m_currentChunkLen)
        {
            m_currentChunkLen += m_truncateInterval - (md->timestamp - m_firstTimestamp);
            while (m_currentChunkLen <= 0)
                m_currentChunkLen += m_truncateInterval;
            //cl_log.log("chunkLen=" , m_currentChunkLen/1000000.0, cl_logALWAYS);
            close();
            m_startDateTime = m_endDateTime;

            return saveData(md);
        }
    }
    else if (vd && !m_gotKeyFrame[channel]) // did not got key frame so far
        return true;


    AVPacket avPkt;
    av_init_packet(&avPkt);
    AVStream* stream = m_formatCtx->streams[channel];
    AVRational srcRate = {1, 1000000};
    Q_ASSERT(stream->time_base.num && stream->time_base.den);
    m_lastPacketTime = md->timestamp;
    avPkt.pts = av_rescale_q(m_lastPacketTime-m_firstTimestamp, srcRate, stream->time_base);
    if(md->flags & AV_PKT_FLAG_KEY)
        avPkt.flags |= AV_PKT_FLAG_KEY;
    avPkt.stream_index= channel;
    avPkt.data = (quint8*) md->data.data();
    avPkt.size = md->data.size();

    if (av_write_frame(m_formatCtx, &avPkt) < 0) 
        cl_log.log(QLatin1String("AV packet write error"), cl_logWARNING);
    else 
        m_packetWrited = true;

    return true;
};

void QnStreamRecorder::endOfRun()
{
	close();
}

bool QnStreamRecorder::initFfmpegContainer(QnCompressedVideoDataPtr mediaData)
{
    if (m_startDateTime == AV_NOPTS_VALUE)
        m_startDateTime = mediaData->timestamp;
    //QnResourcePtr resource = mediaData->dataProvider->getResource();
    // allocate container
    AVOutputFormat * outputCtx = av_guess_format("matroska",NULL,NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = "No MKV container in FFMPEG library";
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QString fileExt = QString(outputCtx->extensions).split(',')[0];

    m_fileName = fillFileName();
    m_fileName += QString(".") + fileExt;
    QString url = QString("ufile:") + m_fileName;

    QMutexLocker mutex(&global_ffmpeg_mutex);
    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, url.toUtf8().constData());

    if (err < 0) {
        m_lastErrMessage = QString("Can't create output file '") + m_fileName + QString("' for video recording.");
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    fileStarted(m_startDateTime/1000, m_fileName);

    outputCtx->video_codec = mediaData->compressionType;
    
    QnAbstractMediaStreamDataProvider* mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (mediaData->dataProvider);
    Q_ASSERT(mediaProvider);

    
    if (av_set_parameters(m_formatCtx, NULL) < 0) {
        m_lastErrMessage = "Can't initialize output format parameters for recording video camera";
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }
    QnMediaResourcePtr mediaDev = qSharedPointerDynamicCast<QnMediaResource>(m_device);
    QString layoutStr = QnArchiveStreamReader::serializeLayout(mediaDev->getVideoLayout(mediaProvider));
    av_metadata_set2(&m_formatCtx->metadata, "video_layout", layoutStr.toAscii().data(), 0);
    const QnResourceAudioLayout* audioLayout = mediaDev->getAudioLayout(mediaProvider);

    av_metadata_set2(&m_formatCtx->metadata, "start_time", QString::number(m_startOffset+mediaData->timestamp/1000).toAscii().data(), 0);

    m_formatCtx->start_time = mediaData->timestamp;

    const QnVideoResourceLayout* layout = mediaDev->getVideoLayout(mediaProvider);
    for (unsigned int i = 0; i < layout->numberOfChannels(); ++i) 
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
        if (mediaData->context && !m_forceDefaultCtx) 
        {
            avcodec_copy_context(videoCodecCtx, mediaData->context->ctx());
        }
        else {
            videoCodecCtx->codec_id = outputCtx->video_codec;
            videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            AVRational defaultFrameRate = {1, 30};
            videoCodecCtx->time_base = defaultFrameRate;
            videoCodecCtx->width = qMax(8,mediaData->width);
            videoCodecCtx->height = qMax(8,mediaData->height);
            
            if (mediaData->compressionType == CODEC_ID_MJPEG)
                videoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
            else
                videoCodecCtx->pix_fmt = PIX_FMT_YUV420P;
            videoCodecCtx->bit_rate = 1000000 * 6;

            videoCodecCtx->sample_aspect_ratio.num = 1;
            videoCodecCtx->sample_aspect_ratio.den = 1;
        }
        videoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        videoStream->sample_aspect_ratio = videoCodecCtx->sample_aspect_ratio;
        videoStream->first_dts = 0;

        //AVRational srcRate = {1, 1000000};
        //videoStream->first_dts = av_rescale_q(mediaData->timestamp, srcRate, stream->time_base);

        QString layoutData = QString::number(layout->v_position(i)) + QString(",") + QString(layout->h_position(i));
        av_metadata_set2(&videoStream->metadata, "layout_channel", QString::number(i).toAscii().data(), 0);
    }

    for (unsigned int i = 0; i < audioLayout->numberOfChannels(); ++i) 
    {
        AVStream* audioStream = av_new_stream(m_formatCtx, DEFAULT_AUDIO_STREAM_ID+i);
        if (audioStream == 0)
        {
            m_lastErrMessage = "Can't allocate output audio stream";
            cl_log.log(m_lastErrMessage, cl_logERROR);
            return false;
        }

        AVCodecContext* audioCodecCtx = audioStream->codec;
        audioCodecCtx->codec_id = audioLayout->getAudioTrackInfo(i).codec;
        audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;

        //AVRational srcRate = {1, 1000000};
        //audioStream->first_dts = av_rescale_q(mediaData->timestamp, srcRate, audioStream->time_base);
    }

    if ((avio_open(&m_formatCtx->pb, url.toUtf8().constData(), AVIO_FLAG_WRITE)) < 0) 
    {
        m_lastErrMessage = "Can't allocate output stream for video recording.";
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    av_write_header(m_formatCtx);
    return true;
}

void QnStreamRecorder::setTruncateInterval(int seconds)
{
    m_currentChunkLen = m_truncateInterval = seconds * 1000000ll;
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

QString QnStreamRecorder::fillFileName()
{
    return m_fixedFileName;
}

QString QnStreamRecorder::fixedFileName() const
{
    return m_fixedFileName;
}


void QnStreamRecorder::closeOnEOF()
{
    QMutexLocker lock(&m_closeAsyncMutex);
    if (m_dataQueue.size() == 0)
        close();
    else
        m_waitEOF = true;
}


void QnStreamRecorder::setEofDateTime(qint64 value)
{
    m_endOfData = false;
    m_EofDateTime = value;
    m_lastProgress = -1;
}
