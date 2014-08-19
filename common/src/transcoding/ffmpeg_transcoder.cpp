#include "ffmpeg_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QDebug>

#include "utils/common/log.h"

#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

static const int IO_BLOCK_SIZE = 1024*16;

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    Q_UNUSED(opaque)
    Q_UNUSED(buf)
    Q_UNUSED(size)
    Q_ASSERT_X(false, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    return 0;
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    Q_UNUSED(opaque)

    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    if (transcoder->inMiddleOfStream())
        return size; // ignore write

    return transcoder->writeBuffer((char*) buf, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    Q_UNUSED(opaque)
    Q_UNUSED(pos)
    Q_UNUSED(whence)
    //Q_ASSERT_X(false, Q_FUNC_INFO, "This class for streaming encoding! This function call MUST not exists!");
    QnFfmpegTranscoder* transcoder = reinterpret_cast<QnFfmpegTranscoder*> (opaque);
    transcoder->setInMiddleOfStream(!(pos == 0 && whence == SEEK_END));
    return 0;
}

AVIOContext* QnFfmpegTranscoder::createFfmpegIOContext()
{
    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    ioBuffer = (quint8*) av_malloc(1024*32);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        IO_BLOCK_SIZE,
        1,
        this,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);
    ffmpegIOContext->seekable = 0;
    return ffmpegIOContext;
}

static QAtomicInt QnFfmpegTranscoder_count = 0;

QnFfmpegTranscoder::QnFfmpegTranscoder()
:
    QnTranscoder(),
    m_videoEncoderCodecCtx(0),
    m_audioEncoderCodecCtx(0),
    m_videoBitrate(0),
    m_formatCtx(0),
    m_ioContext(0),
    m_baseTime(AV_NOPTS_VALUE),
    m_inMiddleOfStream(false)
{
    NX_LOG( lit("Created new ffmpeg transcoder. Total transcoder count %1").
        arg(QnFfmpegTranscoder_count.fetchAndAddOrdered(1)+1), cl_logDEBUG1 );
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    NX_LOG( lit("Destroying ffmpeg transcoder. Total transcoder count %1").
        arg(QnFfmpegTranscoder_count.fetchAndAddOrdered(-1)-1), cl_logDEBUG1 );
    closeFfmpegContext();
}

void av_free_stream( AVStream* st )
{
    av_free( st->info );
    if( st->codec )
    {
        avcodec_close( st->codec );
        av_free( st->codec );
    }
    av_free( st );
}

extern "C" {
    void av_opt_free(void *obj);
};

int workaround_av_write_trailer(AVFormatContext *s)
{
    int ret = 0, i = 0;
    /*
    for(;;){
        AVPacket pkt;
        ret= interleave_packet(s, &pkt, NULL, 1);
        if(ret<0) //FIXME cleanup needed for ret<0 ?
            goto fail;
        if(!ret)
            break;

        ret= s->oformat->write_packet(s, &pkt);
        if (ret >= 0)
            s->streams[pkt.stream_index]->nb_frames++;

        av_free_packet(&pkt);

        if(ret<0)
            goto fail;
        if(s->pb && s->pb->error)
            goto fail;
    }

    if(s->oformat->write_trailer)
        ret = s->oformat->write_trailer(s);
fail:
*/
    if( s->pb )
        avio_flush(s->pb);
    if(ret == 0)
        ret = s->pb ? s->pb->error : 0;
    for(i=0;i<s->nb_streams;i++) {
        av_freep(&s->streams[i]->priv_data);
        av_freep(&s->streams[i]->index_entries);
    }
    if (s->oformat->priv_class)
        av_opt_free(s->priv_data);
    av_freep(&s->priv_data);
    return ret;
}


void QnFfmpegTranscoder::closeFfmpegContext()
{
    if (m_formatCtx)
    {
        workaround_av_write_trailer(m_formatCtx);
        for (unsigned i = 0; i < m_formatCtx->nb_streams; ++i)
            av_free_stream( m_formatCtx->streams[i] );
        m_formatCtx->nb_streams = 0;
    }

    if (m_ioContext)
    {
        //m_ioContext->opaque = 0;
        //avio_close(m_ioContext);
        av_free(m_ioContext->buffer);
        av_free(m_ioContext);
        m_ioContext = 0;
        if (m_formatCtx)
            m_formatCtx->pb = 0;
    }

    if (m_formatCtx) 
        avformat_close_input(&m_formatCtx);
}

int QnFfmpegTranscoder::setContainer(const QString& container)
{
    m_container = container;
    AVOutputFormat * outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        m_lastErrMessage = tr("Container %1 was not found in FFMPEG library.").arg(container);
        qWarning() << m_lastErrMessage;
        return -1;
    }
    //outputCtx->flags |= AVFMT_VARIABLE_FPS;

    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, "");
    if (err != 0)
    {
        m_lastErrMessage = tr("Could not create output context for format %1.").arg(container);
        qWarning() << m_lastErrMessage;
        return -2;
    }
    if (container == QLatin1String("rtp"))
        m_formatCtx->packet_size = MTU_SIZE;


    return 0;
}

int QnFfmpegTranscoder::open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio)
{
    if (m_videoCodec != CODEC_ID_NONE)
    {
        // TODO: #vasilenko avoid using deprecated methods
        AVStream* videoStream = av_new_stream(m_formatCtx, 0);
        if (videoStream == 0)
        {
            m_lastErrMessage = tr("Could not allocate output stream for recording.");
            NX_LOG(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        //videoStream->codec = m_videoEncoderCodecCtx = avcodec_alloc_context3(0);
        m_videoEncoderCodecCtx = videoStream->codec;
        m_videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        m_videoEncoderCodecCtx->codec_id = m_videoCodec;
        m_videoEncoderCodecCtx->pix_fmt = m_videoCodec == CODEC_ID_MJPEG ? PIX_FMT_YUVJ420P : PIX_FMT_YUV420P;

        if (m_vTranscoder)
        {
            m_vTranscoder->open(video);
            if (!m_vTranscoder->getLastError().isEmpty())
                qWarning() << "Can't open video transcoder for RTSP streaming: " << m_vTranscoder->getLastError();

            QnFfmpegVideoTranscoderPtr ffmpegVideoTranscoder = m_vTranscoder.dynamicCast<QnFfmpegVideoTranscoder>();
            if (ffmpegVideoTranscoder->getCodecContext()) {
                avcodec_copy_context(m_videoEncoderCodecCtx, ffmpegVideoTranscoder->getCodecContext());
                m_videoEncoderCodecCtx->stats_out = NULL;   //to avoid double free since avcodec_copy_context does not copy this field
            }
            else {
                m_videoEncoderCodecCtx->width = m_vTranscoder->getResolution().width();
                m_videoEncoderCodecCtx->height = m_vTranscoder->getResolution().height();
            }
            m_videoEncoderCodecCtx->bit_rate = m_vTranscoder->getBitrate();
        }
        else 
        {
            int videoWidth = video->width;
            int videoHeight = video->height;
            if (!video || video->width < 1 || video->height < 1)
            {
                CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
                QSharedPointer<CLVideoDecoderOutput> decodedVideoFrame( new CLVideoDecoderOutput() );
                decoder.decode(video, &decodedVideoFrame);
                videoWidth = decoder.getWidth();
                videoHeight = decoder.getHeight();
                if (videoWidth < 1 || videoHeight < 1)
                {
                    m_lastErrMessage = tr("Could not perform direct stream copy because frame size is undefined.");
                    av_free_stream( videoStream );
                    return -3;
                }
            }

            if (video->context && video->context->ctx()) {
                avcodec_copy_context(m_videoEncoderCodecCtx, video->context->ctx());
                m_videoEncoderCodecCtx->stats_out = NULL;   //to avoid double free since avcodec_copy_context does not copy this field
            }

            m_videoEncoderCodecCtx->width = videoWidth;
            m_videoEncoderCodecCtx->height = videoHeight;
            m_videoEncoderCodecCtx->bit_rate = videoWidth * videoHeight; // auto fill bitrate. 2Mbit for full HD, 1Mbit for 720x768
        }
        m_videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        m_videoEncoderCodecCtx->time_base.num = 1;
        m_videoEncoderCodecCtx->time_base.den = 60;
        m_videoEncoderCodecCtx->gop_size = 30;
        m_videoEncoderCodecCtx->sample_aspect_ratio.num = 1;
        m_videoEncoderCodecCtx->sample_aspect_ratio.den = 1;

        videoStream->sample_aspect_ratio = m_videoEncoderCodecCtx->sample_aspect_ratio;
        videoStream->first_dts = 0;
    }

    if (m_aTranscoder && !m_aTranscoder->open(audio))
    {
        m_audioCodec = CODEC_ID_NONE; // can't open transcoder. disable audio
    }

    if (m_audioCodec != CODEC_ID_NONE)
    {
        //Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented! Under construction!!!");

        // TODO: #vasilenko avoid using deprecated methods
        AVStream* audioStream = av_new_stream(m_formatCtx, 0);
        if (audioStream == 0)
        {
            m_lastErrMessage = tr("Could not allocate output stream for recording.");
            NX_LOG(m_lastErrMessage, cl_logERROR);
            return -1;
        }

        AVCodec* avCodec = avcodec_find_decoder(m_audioCodec);
        if (avCodec == 0)
        {
            m_lastErrMessage = tr("Could not find codec %1.").arg(m_audioCodec);
            av_free_stream( audioStream );
            return -2;
        }
        audioStream->codec = m_audioEncoderCodecCtx = avcodec_alloc_context3(avCodec);
        m_audioEncoderCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioEncoderCodecCtx->codec_id = m_audioCodec;

        if (m_aTranscoder)
        {
            QnFfmpegAudioTranscoderPtr ffmpegAudioTranscoder = m_aTranscoder.dynamicCast<QnFfmpegAudioTranscoder>();
            if (ffmpegAudioTranscoder->getCodecContext()) {
                avcodec_copy_context(m_audioEncoderCodecCtx, ffmpegAudioTranscoder->getCodecContext());
                m_audioEncoderCodecCtx->stats_out = NULL;   //to avoid double free since avcodec_copy_context does not copy this field
            }
            m_audioEncoderCodecCtx->bit_rate = m_aTranscoder->getBitrate();
        }
        else 
        {
            if (audio->context && audio->context->ctx()) {
                avcodec_copy_context(m_audioEncoderCodecCtx, audio->context->ctx());
                m_audioEncoderCodecCtx->stats_out = NULL;   //to avoid double free since avcodec_copy_context does not copy this field
            }
            //m_audioEncoderCodecCtx->bit_rate = 1024 * 96;
        }
        m_audioEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        audioStream->first_dts = 0;
        //audioStream->time_base = m_audioEncoderCodecCtx->time_base;
    }
    if (m_formatCtx->nb_streams == 0)
        return -4;

    m_formatCtx->pb = m_ioContext = createFfmpegIOContext();

    int rez = avformat_write_header(m_formatCtx, 0);
    if (rez < 0) 
    {
        closeFfmpegContext();
        m_lastErrMessage = tr("Video or audio codec is incompatible with container %1.").arg(m_container);
        NX_LOG(m_lastErrMessage, cl_logERROR);
        return -3;
    }
    m_initialized = true;
    return 0;
}

bool QnFfmpegTranscoder::addTag( const QString& name, const QString& value )
{
    return av_dict_set( &m_formatCtx->metadata, name.toUtf8().constData(), value.toUtf8().constData(), 0 ) >= 0;
}

int QnFfmpegTranscoder::transcodePacketInternal(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result)
{
    Q_UNUSED(result)
    if ((quint64)m_baseTime == AV_NOPTS_VALUE)
        m_baseTime = media->timestamp - 1000*100;

    if (m_audioCodec == CODEC_ID_NONE && media->dataType == QnAbstractMediaData::AUDIO)
        return 0;
	else if (m_videoCodec == CODEC_ID_NONE && media->dataType == QnAbstractMediaData::VIDEO)
		return 0;

    AVRational srcRate = {1, 1000000};
    int streamIndex = 0;
    if (m_vTranscoder && m_aTranscoder && media->dataType == QnAbstractMediaData::AUDIO)
        streamIndex = 1;

    AVStream* stream = m_formatCtx->streams[streamIndex];
    AVPacket packet;
    av_init_packet(&packet);
    
    QnAbstractMediaDataPtr transcodedData;
    
    QnCodecTranscoderPtr transcoder;
    if (dynamic_cast<const QnCompressedVideoData*>(media.data()))
        transcoder = m_vTranscoder;
    else
        transcoder = m_aTranscoder;

    bool doTranscoding = true;
    do {
        packet.data = 0;
        packet.size = 0;

        if (transcoder)
        {
            // transcode media
            int errCode = transcoder->transcodePacket(doTranscoding ? media : QnConstAbstractMediaDataPtr(), result ? &transcodedData : NULL);
            if (errCode != 0)
                return errCode;
            if (transcodedData) {
                packet.data = const_cast<quint8*>((const quint8*) transcodedData->data());  //const_cast is here because av_write_frame accepts 
                                                                                            //non-const pointer, but does not modifiy packet buffer
                packet.size = transcodedData->dataSize();
                packet.pts = av_rescale_q(transcodedData->timestamp - m_baseTime, srcRate, stream->time_base);
                if(transcodedData->flags & AV_PKT_FLAG_KEY)
                    packet.flags |= AV_PKT_FLAG_KEY;
            }
        }
        else {
            // direct stream copy
            packet.pts = av_rescale_q(media->timestamp - m_baseTime, srcRate, stream->time_base);
            packet.data = const_cast<quint8*>((const quint8*) media->data());
            packet.size = media->dataSize();
            if((media->dataType == QnAbstractMediaData::AUDIO) || (media->flags & AV_PKT_FLAG_KEY))
                packet.flags |= AV_PKT_FLAG_KEY;
        }
        packet.stream_index = streamIndex;
        packet.dts = packet.pts;
        
        if (packet.size > 0)
        {
            //qDebug() << "packet.pts=" << packet.pts;

            if (av_write_frame(m_formatCtx, &packet) < 0) {
                qWarning() << QLatin1String("Transcoder error: can't write AV packet");
                //return -1; // ignore error and continue
            }
        }
        doTranscoding = false;
    } while (transcoder && transcoder->existMoreData());
    return 0;
}

AVCodecContext* QnFfmpegTranscoder::getVideoCodecContext() const 
{ 
    /*
    QnFfmpegVideoTranscoderPtr ffmpegVideoTranscoder = m_vTranscoder.dynamicCast<QnFfmpegVideoTranscoder>();
    if (ffmpegVideoTranscoder)
        return ffmpegVideoTranscoder->getCodecContext();
    else
    */
        return m_videoEncoderCodecCtx; 
}

AVCodecContext* QnFfmpegTranscoder::getAudioCodecContext() const 
{ 
    return m_audioEncoderCodecCtx; 
}

#endif // ENABLE_DATA_PROVIDERS
