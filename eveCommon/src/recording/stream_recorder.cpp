#include "stream_recorder.h"
#include "core/resource/resource_consumer.h"
#include "core/datapacket/datapacket.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "storage_manager.h"
#include "../../eveClient/src/client_util.h"

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4352;

QnStreamRecorder::QnStreamRecorder(QnResourcePtr dev):
QnAbstractDataConsumer(15),
m_device(dev),
m_firstTime(true),
m_packetWrited(false),
m_firstTimestamp(-1),
m_formatCtx(0),
m_truncateInterval(0),
m_currentChunkLen(0),
m_prevDateTime(-1),
m_currentDateTime(-1)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
}

QnStreamRecorder::~QnStreamRecorder()
{
	cleanup();
}

void QnStreamRecorder::cleanup()
{
    if (m_formatCtx) 
    {
        if (m_truncateInterval > 0)
        {
            av_metadata_set2(&m_formatCtx->metadata, "start_time", QString::number(m_prevDateTime/1000).toAscii().data(), 0);
            av_metadata_set2(&m_formatCtx->metadata, "end_time", QString::number(m_currentDateTime/1000).toAscii().data(), 0);
            qnStorage->addFileInfo(m_prevDateTime, m_currentDateTime, m_fileName);
        }

        if (m_packetWrited)
            av_write_trailer(m_formatCtx);
        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
            avcodec_close(m_formatCtx->streams[i]->codec);
        avio_close(m_formatCtx->pb);
        avformat_free_context(m_formatCtx);
        m_formatCtx = 0;
    }
    m_packetWrited = false;
    m_firstTimestamp = -1;

	for (int i = 0; i < CL_MAX_CHANNELS; ++i)
		m_gotKeyFrame[i] = false;
	m_firstTime = true;
}

bool QnStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr md = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (md == 0)
        return true; // skip non media data at current version
    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

    if (m_firstTime)
    {
        if (vd == 0)
            return true; // skip audio packets before first video packet
        if (!initFfmpegContainer(vd))
        {
            emit recordingFailed(m_lastErrMessage);
            m_needStop = true;
            return false;
        }
        m_firstTime = false;
        emit recordingStarted();
    }

    int channel = md->channelNumber;
    if (channel >= m_formatCtx->nb_streams)
        return true; // skip packet

    if (m_firstTimestamp == -1)
        m_firstTimestamp = md->timestamp;
    
    if (md->flags & AV_PKT_FLAG_KEY)
    {
        m_gotKeyFrame[channel] = true;
        if (m_truncateInterval > 0 && md->timestamp - m_firstTimestamp > m_currentChunkLen)
        {
            m_currentChunkLen += m_truncateInterval - (md->timestamp - m_firstTimestamp);
            
            m_prevDateTime = m_currentDateTime;
            m_currentDateTime = md->timestamp;
            cleanup();
            return processData(data);
        }
    }
    else if (vd && !m_gotKeyFrame[channel]) // did not got key frame so far
        return true;

    AVPacket avPkt;
    av_init_packet(&avPkt);
        AVStream* stream = m_formatCtx->streams[channel];
    AVRational srcRate = {1, 1000000};
    avPkt.pts = av_rescale_q(md->timestamp, srcRate, stream->time_base);
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
}

void QnStreamRecorder::endOfRun()
{
	cleanup();
}

bool QnStreamRecorder::initFfmpegContainer(QnCompressedVideoDataPtr mediaData)
{
    if (m_currentDateTime == -1)
        m_currentDateTime = mediaData->timestamp;
    QnResourcePtr resource = mediaData->dataProvider->getResource();
    // allocate container
    AVOutputFormat * outputCtx = av_guess_format("matroska",NULL,NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = "No MKV container in FFMPEG library";
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    QString fileExt = QString(outputCtx->extensions).split(',')[0];

    QFileInfo fi(m_device->getUniqueId());
    if (m_truncateInterval == 0)  {
        m_fileName = getTempRecordingDir() + fi.baseName();
    }
    else {
        m_fileName = qnStorage->getFileName(m_currentDateTime, fi.baseName());
    }

    m_fileName += QString(".") + fileExt;
    QString url = QString("ufile:") + m_fileName;
    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, url.toUtf8().constData());

    if (err < 0) {
        m_lastErrMessage = QString("Can't create output file '") + m_fileName + QString("' for video recording.");
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }

    outputCtx->video_codec = mediaData->compressionType;
    
    QnAbstractMediaStreamDataProvider* mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (mediaData->dataProvider);

    Q_ASSERT(mediaProvider);

    
    if (av_set_parameters(m_formatCtx, NULL) < 0) {
        m_lastErrMessage = "Can't initialize output format parameters for recording video camera";
        cl_log.log(m_lastErrMessage, cl_logERROR);
        return false;
    }
    QnMediaResourcePtr mediaDev = qSharedPointerDynamicCast<QnMediaResource>(resource);
    QString layoutStr = QnArchiveStreamReader::serializeLayout(mediaDev->getVideoLayout(mediaProvider));
    av_metadata_set2(&m_formatCtx->metadata, "video_layout", layoutStr.toAscii().data(), 0);
    QnResourceAudioLayout* audioLayout = mediaDev->getAudioLayout(mediaProvider);

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
        videoCodecCtx->codec_id = outputCtx->video_codec;
        videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        AVRational defaultFrameRate = {1, 30};
        videoCodecCtx->time_base = defaultFrameRate;
        videoCodecCtx->width = mediaData->width;
        videoCodecCtx->height = mediaData->height;
        
        if (mediaData->compressionType == CODEC_ID_MJPEG)
            videoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
        else
            videoCodecCtx->pix_fmt = PIX_FMT_YUV420P;
        videoCodecCtx->bit_rate = 1000000 * 6;

        videoCodecCtx->sample_aspect_ratio.num = 1;
        videoCodecCtx->sample_aspect_ratio.den = 1;
        videoStream->sample_aspect_ratio = videoCodecCtx->sample_aspect_ratio;

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
