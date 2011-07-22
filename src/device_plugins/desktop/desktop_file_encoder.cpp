#include "desktop_file_encoder.h"

#include <QAudioInput>

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4351;
static const int AUDIO_QUEUE_MAX_SIZE = 128;

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
#include "data/mediadata.h"
}

AVSampleFormat audioFormatQtToFfmpeg(const QAudioFormat& fmt)
{
    int s = fmt.sampleSize();
    QAudioFormat::SampleType st = fmt.sampleType();
    if (fmt.sampleSize() == 8)
        return AV_SAMPLE_FMT_U8;
    else if(fmt.sampleSize() == 16 && fmt.sampleType() == QAudioFormat::SignedInt)
        return AV_SAMPLE_FMT_S16;
    else if(fmt.sampleSize() == 32 && fmt.sampleType() == QAudioFormat::SignedInt)
        return AV_SAMPLE_FMT_S32;
    else if(fmt.sampleSize() == 32 && fmt.sampleType() == QAudioFormat::Float)
        return AV_SAMPLE_FMT_FLT;
    else
        return AV_SAMPLE_FMT_NONE;
}

struct FffmpegLog
{
    static void av_log_default_callback_impl(void* ptr, int level, const char* fmt, va_list vl)
    {
        Q_UNUSED(level)
            Q_UNUSED(ptr)
            Q_ASSERT(fmt && "NULL Pointer");

        if (!fmt) {
            return;
        }
        static char strText[1024 + 1];
        vsnprintf(&strText[0], sizeof(strText) - 1, fmt, vl);
        va_end(vl);

        qDebug() << "ffmpeg library: " << strText;
    }
};


class CaptureAudioStream: public QIODevice
{
public:
    CaptureAudioStream(DesktopFileEncoder* encoder)
        : QIODevice(),
        m_encoder(encoder),
        m_currentBufferSize(0),
        m_nextAudioBuffer(0)
    {
        m_nextAudioBuffer = new CLAbstractMediaData(CL_MEDIA_ALIGNMENT, m_encoder->audioPacketSize());
        m_nextAudioBuffer->timestamp = m_encoder->currentTime();
    }
    ~CaptureAudioStream()
    {
        delete m_nextAudioBuffer;
    }
protected:
    // QIODevice
    virtual qint64 readData ( char * data, qint64 maxSize ) { return -1; }
    virtual bool isOpen () const { return true; }
    virtual bool isReadable () const {  return false; }
    virtual bool isWritable() const {  return true; }
    virtual bool isSequential () const {  return true;  }

    qint64 writeData ( const char * data, qint64 maxSize )
    {
        int currentMaxSize = maxSize;
        while (currentMaxSize > 0)
        {
            int dataToWrite = qMin(currentMaxSize, m_encoder->audioPacketSize() - m_currentBufferSize);
            m_nextAudioBuffer->data.write(data, dataToWrite);
            data += dataToWrite;
            currentMaxSize -= dataToWrite;
            m_currentBufferSize += dataToWrite;
            if (m_currentBufferSize == m_encoder->audioPacketSize())
            {
                m_encoder->m_audioQueue.push(m_nextAudioBuffer);
                if (m_encoder->m_audioQueue.size() > 16)
                {
                    int ff = 4;
                }
                m_nextAudioBuffer = new CLAbstractMediaData(CL_MEDIA_ALIGNMENT, m_encoder->audioPacketSize());
                m_nextAudioBuffer->timestamp = m_encoder->currentTime();
                m_currentBufferSize = 0;
            }
        }
        return maxSize;
    }
private:
    DesktopFileEncoder* m_encoder;
    int m_currentBufferSize;
    CLAbstractMediaData* m_nextAudioBuffer;
};

int DesktopFileEncoder::initIOContext()
{
    enum {
        MAX_READ_SIZE = 1024 * 4*2
    };

    struct IO_ffmpeg {
        static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
        {
            DesktopFileEncoder* stream = reinterpret_cast<DesktopFileEncoder *>(opaque);
            return stream ? stream->writePacketImpl(buf, bufSize) : 0;
        }
        static qint32 readPacket(void *opaque, quint8* buf, int size)
        {
            Q_ASSERT(opaque && "NULL Pointer");
            DesktopFileEncoder* stream = reinterpret_cast<DesktopFileEncoder *>(opaque);
            return stream ? stream->readPacketImpl(buf, size) : 0;
        }
        static qint64 seek(void* opaque, qint64 offset, qint32 whence)
        {
            Q_ASSERT(opaque && "NULL Pointer");
            DesktopFileEncoder* stream = reinterpret_cast<DesktopFileEncoder *>(opaque);
            return stream ? stream->seekPacketImpl(offset, whence) : 0;

        }
    };
    m_buffer.resize(MAX_READ_SIZE);
    m_iocontext = av_alloc_put_byte(&m_buffer[0], MAX_READ_SIZE, 1, this, &IO_ffmpeg::readPacket, &IO_ffmpeg::writePacket, &IO_ffmpeg::seek);
    return m_iocontext != 0;
}
qint32 DesktopFileEncoder::writePacketImpl(quint8* buf, qint32 bufSize)
{
    Q_ASSERT(buf && "NULL Pointer");
    return m_device ? m_device->write(reinterpret_cast<const char *>(buf), bufSize) : 0;
}
qint32 DesktopFileEncoder::readPacketImpl(quint8* buf, quint32 bufSize)
{
    Q_ASSERT(buf && "NULL Pointer");
    Q_UNUSED(bufSize);
    return 0;
}
qint64 DesktopFileEncoder::seekPacketImpl(qint64 offset, qint32 whence)
{
    Q_UNUSED(whence);
    if (!m_device) {
        return -1;
    }
    if (whence != SEEK_END && whence != SEEK_SET && whence != SEEK_CUR) {
        return -1;
    }
    if (SEEK_END == whence && -1 == offset) {
        return UINT_MAX;
    }
    if (SEEK_END == whence) {
        offset = m_device->size() - offset;
        offset = m_device->seek(offset) ? offset : -1;
        return offset;
    }
    if (SEEK_CUR == whence) {
        offset += m_device->pos();
        offset = m_device->seek(offset) ? offset : -1;
        return offset;
    }
    if (SEEK_SET == whence) {
        offset = m_device->seek(offset) ? offset : -1;
        return offset;
    }
    return -1;
}





DesktopFileEncoder::DesktopFileEncoder(const QString& fileName, 
                                       int desktopNum,
                                       const QAudioDeviceInfo audioDevice):
    CLLongRunnable(),
    m_videoBuf(0),
    m_videoBufSize(0),
    m_videoCodecCtx(0),
    m_initialized(false),
    m_fileName(fileName),
    m_device(0),
    m_iocontext(0),
    m_formatCtx(0),
    m_grabber(0),
    m_desktopNum(desktopNum),
    m_audioDevice(audioDevice),
    m_audioInput(0),
    m_audioOStream(0),

    m_audioFramesCount(0),
    m_audioFrameDuration(0),
    m_storedAudioPts(0),
    m_maxAudioJitter(0)

{
    //m_encoderCodecName = "libx264";
    m_encoderCodecName = "mpeg2video";
    m_needStop = false;
    m_audioQueue.setMaxSize(AUDIO_QUEUE_MAX_SIZE);

    openStream();
    start();
}

DesktopFileEncoder::~DesktopFileEncoder()
{
    m_needStop = true;
    wait();
    closeStream();
    delete m_audioInput;
    delete m_audioOStream;
}

bool DesktopFileEncoder::init()
{
    m_grabber = new CLBufferedScreenGrabber(m_desktopNum);

    avcodec_init();
    av_register_all();


    av_log_set_callback(FffmpegLog::av_log_default_callback_impl);


    m_videoBufSize = avpicture_get_size((PixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height());
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = avcodec_alloc_frame();
    avpicture_alloc((AVPicture*) m_frame, m_grabber->format(), m_grabber->width(), m_grabber->height() );

    AVCodec* videoCodec = avcodec_find_encoder_by_name(m_encoderCodecName);
    if(videoCodec == 0)
    {
        cl_log.log("Can't find encoder", cl_logWARNING);
        return false;
    }
    
    // allocate container
    m_device = new QFile(m_fileName);
    m_device->open(QIODevice::WriteOnly);

    m_formatCtx = avformat_alloc_context();
    initIOContext();
    m_formatCtx->pb = m_iocontext;
    m_outputCtx = av_guess_format("mpegts",NULL,NULL);

    m_outputCtx->video_codec = videoCodec->id;

    m_formatCtx->oformat = m_outputCtx;

    if (av_set_parameters(m_formatCtx, NULL) < 0) {
        cl_log.log("Invalid output format parameters", cl_logERROR);
        return false;
    }

    m_videoOutStream = NULL;
    m_videoOutStream = av_new_stream(m_formatCtx, DEFAULT_VIDEO_STREAM_ID);
    if (!m_videoOutStream)
    {
        cl_log.log("Can't create output stream for encoding", cl_logWARNING);
        return false;
    }
    m_videoCodecCtx = m_videoOutStream->codec;
    m_videoCodecCtx->codec_id = m_outputCtx->video_codec;
    m_videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO ;
    
    
    m_videoCodecCtx->time_base = m_grabber->getFrameRate();

    m_videoCodecCtx->pix_fmt = m_grabber->format(); 
    m_videoCodecCtx->width = m_grabber->width();
    m_videoCodecCtx->height = m_grabber->height();

    
    m_videoCodecCtx->gop_size = 12;
    m_videoCodecCtx->has_b_frames = 0;
    m_videoCodecCtx->max_b_frames = 0;
    m_videoCodecCtx->me_threshold = 0;
    m_videoCodecCtx->intra_dc_precision = 0;
    m_videoCodecCtx->strict_std_compliance = 0;
    m_videoCodecCtx->me_method = ME_EPZS;
    m_videoCodecCtx->bit_rate = 1024*1024*8;

    m_videoCodecCtx->sample_aspect_ratio.num = 1;
    m_videoCodecCtx->sample_aspect_ratio.den = 1;
    

    m_videoOutStream->sample_aspect_ratio.den = 1;
    m_videoOutStream->sample_aspect_ratio.num = 1;


    if (avcodec_open(m_videoCodecCtx, videoCodec) < 0)
    {
        cl_log.log("Can't initialize encoder", cl_logWARNING);
        return false;
    }

    m_videoOutStream->first_dts = 0;
    m_videoStreamIndex = m_videoOutStream->index;
    //m_videoOutStream->pts.num = m_videoOutStream->time_base.num;
    //m_videoOutStream->pts.den = m_videoOutStream->time_base.den;



    // init audio capture

    m_audioFormat = m_audioDevice.preferredFormat();
    m_audioFormat.setSampleRate(48000);
    m_audioFormat.setSampleSize(16);
    //m_audioFormat.setChannels(1);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    m_audioBuf = (quint8*) av_malloc(FF_MIN_BUFFER_SIZE);
    m_audioOutStream = NULL;
    m_audioOutStream = av_new_stream(m_formatCtx, DEFAULT_AUDIO_STREAM_ID);
    if (!m_audioOutStream)
    {
        cl_log.log("Can't create output audio stream for encoding", cl_logWARNING);
        return false;
    }

    AVCodec* audioCodec = avcodec_find_encoder_by_name("aac");
    //AVCodec* audioCodec = avcodec_find_encoder_by_name("libmp3lame");
    if(audioCodec == 0)
    {
        cl_log.log("Can't find audio MP3 encoder", cl_logWARNING);
        return false;
    }
    m_outputCtx->audio_codec = audioCodec->id;

    m_audioCodecCtx = m_audioOutStream->codec;
    m_audioCodecCtx->codec_id = m_outputCtx->audio_codec;
    m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_audioCodecCtx->sample_fmt = audioFormatQtToFfmpeg(m_audioFormat);
    m_audioCodecCtx->channels = m_audioFormat.channels();
    m_audioCodecCtx->sample_rate = m_audioFormat.frequency();
    AVRational audioRational = {1, m_audioCodecCtx->sample_rate};
    m_audioCodecCtx->time_base	 = audioRational;
    m_audioCodecCtx->bit_rate = 64000 * m_audioCodecCtx->channels;

    //AVCodec* audioCodec = avcodec_find_encoder_by_name("mp3");

    if (avcodec_open(m_audioCodecCtx, audioCodec) < 0)
    {
        cl_log.log("Can't initialize encoder", cl_logWARNING);
        return false;
    }

    av_write_header(m_formatCtx);

    m_audioFrameDuration = m_audioCodecCtx->frame_size / (double) m_audioCodecCtx->sample_rate;
    m_audioFrameDuration *= m_audioOutStream->time_base.den / (double) m_audioOutStream->time_base.num;
    
    // 50 ms as max jitter
    // QT uses 25fps timer for audio grabbing, so jitter 40ms + 10ms reserved.
    m_maxAudioJitter = m_audioOutStream->time_base.den / m_audioOutStream->time_base.num / 20; 



    if (!m_audioDevice.isFormatSupported(m_audioFormat))
    {
        cl_log.log("Unsupported audio format specified for capturing!", cl_logERROR);
        return false;
    }

    m_audioInput = new QAudioInput ( m_audioDevice, m_audioFormat);
    //m_audioInput->moveToThread(QApplication::instance()->thread());

    //m_audioOStream = new QFile("c:/audio_captire.wav");
    m_audioOStream = new CaptureAudioStream(this);
    m_audioOStream->open(QIODevice::WriteOnly | QIODevice::Truncate);
    m_audioInput->start(m_audioOStream);

    return true;
}

void DesktopFileEncoder::run()
{
    while (!m_needStop)
    {
        void* capturedData = m_grabber->getNextFrame();
        if (!capturedData)
            continue;
        m_grabber->capturedDataToFrame(capturedData, m_frame);

        m_frame->pts = m_frame->coded_picture_number;

        int out_size = avcodec_encode_video(m_videoCodecCtx, m_videoBuf, m_videoBufSize, m_frame);
        if (out_size < 1)
            continue;

        AVStream * stream = m_formatCtx->streams[m_videoStreamIndex];
        AVPacket videoPkt;
        av_init_packet(&videoPkt);
        if (m_videoCodecCtx->coded_frame->pts != AV_NOPTS_VALUE)
            videoPkt.pts = av_rescale_q(m_videoCodecCtx->coded_frame->pts, m_videoCodecCtx->time_base, stream->time_base);

        //cl_log.log("pkt.pts=" , pkt.pts, cl_logALWAYS);

        if(m_videoCodecCtx->coded_frame->key_frame)
            videoPkt.flags |= AV_PKT_FLAG_KEY;

        videoPkt.stream_index= stream->index;
        videoPkt.data = m_videoBuf;
        videoPkt.size = out_size;

        // write all audio frames
        while (m_audioQueue.size() > 0)
        {
            AVRational r;
            r.num = 1;
            r.den = 1000;
            CLAbstractMediaData* audioData = m_audioQueue.front();

            qint64 audioPts =  av_rescale_q(audioData->timestamp, r, stream->time_base); // + 20000;
            qint64 expectedAudioPts = m_storedAudioPts + m_audioFramesCount * m_audioFrameDuration; // + 20000;
            int audioJitter = qAbs(audioPts - expectedAudioPts);

            qDebug() << "audio jitter" <<  audioJitter;

            if (audioJitter < m_maxAudioJitter)
            {
                audioPts = expectedAudioPts;
            }
            else {
                m_storedAudioPts = audioPts;
                m_audioFramesCount = 1;
            }
    
            if (audioPts > videoPkt.pts)
                break;

            m_audioFramesCount++;

            AVPacket audioPacket;
            m_audioQueue.pop(audioData);

            // todo: add audio resample here

            const short* buffer = (const short*) audioData->data.data();
            int aEncoded = avcodec_encode_audio(m_audioCodecCtx, m_audioBuf, FF_MIN_BUFFER_SIZE, buffer);
            if (aEncoded > 0)
            {
                av_init_packet(&audioPacket);
                audioPacket.pts = audioPts;
                audioPacket.data = m_audioBuf;
                audioPacket.size = aEncoded;
                audioPacket.stream_index = m_audioOutStream->index;
                if (av_write_frame(m_formatCtx,&audioPacket)<0)	
                {
                    QString s ="AUDIO av_write_frame(formatCtx,&audioPacket) error. current time:"+QTime::currentTime().toString() +"\n";
                    s+="packet pts: " + QString::number(audioPacket.pts) + "dts:"+QString::number(audioPacket.dts) + "\n";
                    s+="error code: " + QString::number(m_iocontext->error) + "\n";
                    s+="eof_reached: " + QString::number(m_iocontext->eof_reached) + "\n";
                    s+="pos: " + QString::number(m_iocontext->pos) + "\n";
                    s+="write_flag: " + QString::number(m_iocontext->write_flag) + "\n";
                    cl_log.log(s, cl_logWARNING);
                }
            }
            audioData->releaseRef();
        }

        if (av_write_frame(m_formatCtx,&videoPkt)<0)	
        {
            QString s ="VIDEO av_write_frame(formatCtx,&videoPkt) error. current time:"+QTime::currentTime().toString() +"\n";
            s+="packet pts: " + QString::number(videoPkt.pts) + "dts:"+QString::number(videoPkt.dts) + "\n";
            s+="error code: " + QString::number(m_iocontext->error) + "\n";
            s+="eof_reached: " + QString::number(m_iocontext->eof_reached) + "\n";
            s+="pos: " + QString::number(m_iocontext->pos) + "\n";
            s+="write_flag: " + QString::number(m_iocontext->write_flag) + "\n";
            cl_log.log(s, cl_logWARNING);
        }

    }
    closeStream();
}

void DesktopFileEncoder::openStream()
{
    if (init())
        m_initialized = true;
}

void DesktopFileEncoder::closeStream()
{
    delete m_grabber;
    m_grabber = 0;

    if (m_formatCtx)
        avformat_free_context(m_formatCtx);
    m_formatCtx = 0;
    m_videoCodecCtx = 0;
    m_audioCodecCtx = 0;

    if (m_frame)
        av_free(m_frame);
    m_frame = 0;
    if (m_videoBuf)
        av_free(m_videoBuf);
    m_videoBuf = 0;

    av_free(av_alloc_put_byte);
    m_iocontext = 0;
    delete m_device;
    m_device = 0;

    m_initialized = false;
}

qint64 DesktopFileEncoder::currentTime() const
{
    return m_grabber->currentTime();
}


int DesktopFileEncoder::audioPacketSize()
{
    return m_audioCodecCtx->frame_size * m_audioFormat.channels() * m_audioFormat.sampleSize()/8;
}
