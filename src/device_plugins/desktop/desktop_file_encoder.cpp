#include "desktop_file_encoder.h"
#include <intrin.h>

#include <QAudioInput>

static const int DEFAULT_VIDEO_STREAM_ID = 4113;
static const int DEFAULT_AUDIO_STREAM_ID = 4351;
static const int AUDIO_QUEUE_MAX_SIZE = 256;
static const int BASE_BITRATE = 1000 * 1000 * 8; // bitrate for best quality for fullHD mode;

static const int MAX_VIDEO_JITTER = 2;

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>

    int av_set_string3(void *obj, const char *name, const char *val, int alloc, const AVOption **o_out);
}

#include "data/mediadata.h"

// mux audio 1 and audio 2 to audio1 buffer
// I have used intrisicts for SSE. It is portable for MSVC, GCC (mac, linux), Intel compiler
void stereoAudioMux(qint16* a1, qint16* a2, int lenInShort)
{
    __m128i* audio1 = (__m128i*) a1;
    __m128i* audio2 = (__m128i*) a2;
    for (int i = 0; i < lenInShort/8; ++i)
    {
        //*audio1 = _mm_avg_epu16(*audio1, *audio2);
        *audio1 = _mm_add_epi16(*audio1, *audio2);
        audio1++;
        audio2++;
    }
    int rest = lenInShort % 8;
    if (rest > 0)
    {
        a1 += lenInShort - rest;
        a2 += lenInShort - rest;
        for (int i = 0; i < rest; ++i)
        {
            //*a1 = ((int)*a1 + (int)*a2) >> 1;
            *a1 = ((int)*a1 + (int)*a2);
            a1++;
            a2++;
        }
    }
}

void monoToStereo(qint16* dst, qint16* src, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src;
        *dst++ = *src++;
    }
}

void monoToStereo(qint16* dst, qint16* src1, qint16* src2, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src1++;
        *dst++ = *src2++;
    }
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
        m_nextAudioBuffer(0),
        m_startDelay(0),
        m_isPrimary(true)
    {
    }
    ~CaptureAudioStream()
    {
        delete m_nextAudioBuffer;
    }
    void setPrimary(bool value) { m_isPrimary = value; }
protected:
    // QIODevice
    virtual qint64 readData ( char * data, qint64 maxSize ) { return -1; }
    virtual bool isOpen () const { return true; }
    virtual bool isReadable () const {  return false; }
    virtual bool isWritable() const {  return true; }
    virtual bool isSequential () const {  return true;  }

    qint64 writeData ( const char * data, qint64 maxSize )
    {
        int packetSize = m_encoder->audioPacketSize(m_isPrimary);
        if (m_nextAudioBuffer == 0)
        {
            m_nextAudioBuffer = new CLAbstractMediaData(CL_MEDIA_ALIGNMENT, packetSize);
            m_startDelay = m_encoder->currentTime();
            m_nextAudioBuffer->timestamp = 0;
        }

        int currentMaxSize = maxSize;
        while (currentMaxSize > 0)
        {
            int dataToWrite = qMin(currentMaxSize, packetSize - m_currentBufferSize);
            m_nextAudioBuffer->data.write(data, dataToWrite);
            data += dataToWrite;
            currentMaxSize -= dataToWrite;
            m_currentBufferSize += dataToWrite;
            if (m_currentBufferSize == packetSize)
            {
                if (m_isPrimary)
                    m_encoder->m_audioQueue.push(m_nextAudioBuffer);
                else
                    m_encoder->m_secondAudioQueue.push(m_nextAudioBuffer);
                m_nextAudioBuffer = new CLAbstractMediaData(CL_MEDIA_ALIGNMENT, packetSize);
                m_nextAudioBuffer->timestamp = m_encoder->currentTime() - m_startDelay;
                m_currentBufferSize = 0;
            }
        }
        return maxSize;
    }
private:
    DesktopFileEncoder* m_encoder;
    int m_currentBufferSize;
    CLAbstractMediaData* m_nextAudioBuffer;
    int m_startDelay;
    bool m_isPrimary;
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


DesktopFileEncoder::DesktopFileEncoder ( 
                   const QString& fileName, 
                   int desktopNum, 
                   const QAudioDeviceInfo* audioDevice,
                   const QAudioDeviceInfo* audioDevice2,
                   CLScreenGrapper::CaptureMode captureMode,
                   bool captureCursor,
                   const QSize& captureResolution,
                   float encodeQualuty // in range 0.0 .. 1.0
                   ):
    CLLongRunnable(),
    m_videoBuf(0),
    m_videoBufSize(0),
    m_videoCodecCtx(0),
    m_audioCodecCtx(0),
    m_initialized(false),
    m_fileName(fileName),
    m_device(0),
    m_iocontext(0),
    m_formatCtx(0),
    m_grabber(0),
    m_desktopNum(desktopNum),
    m_audioInput(0),
    m_audioInput2(0),
    m_audioOStream(0),
    m_audioOStream2(0),

    m_audioFramesCount(0),
    m_audioFrameDuration(0),
    m_storedAudioPts(0),
    m_maxAudioJitter(0),

    m_captureMode(captureMode),
    m_captureCursor(captureCursor), 
    m_captureResolution(captureResolution),
    m_encodeQualuty(encodeQualuty),
    m_encodedFrames(0),
    m_useSecondaryAudio(0),
    m_tmpAudioBuffer1(CL_MEDIA_ALIGNMENT, FF_MIN_BUFFER_SIZE),
    m_tmpAudioBuffer2(CL_MEDIA_ALIGNMENT, FF_MIN_BUFFER_SIZE)
{
    m_useAudio = audioDevice || audioDevice2;
    m_useSecondaryAudio = audioDevice && audioDevice2 && audioDevice->deviceName() != audioDevice2->deviceName();

    if (m_useAudio)
        m_audioDevice = audioDevice ? *audioDevice : *audioDevice2;

    if (m_useSecondaryAudio)
        m_audioDevice2 = *audioDevice2;

    m_needStop = false;
    m_audioQueue.setMaxSize(AUDIO_QUEUE_MAX_SIZE);

    openStream();
    start();
}

void DesktopFileEncoder::stop()
{
    m_needStop = true;
}

DesktopFileEncoder::~DesktopFileEncoder()
{
    m_needStop = true;
    wait();
    closeStream();
}

int DesktopFileEncoder::calculateBitrate()
{
    double bitrate = BASE_BITRATE;

    bitrate /=  1920.0*1080.0 / m_grabber->width() / m_grabber->height();

    bitrate *= m_encodeQualuty;
    
    return bitrate;
}

bool DesktopFileEncoder::init()
{
    m_grabber = new CLBufferedScreenGrabber(
            m_desktopNum, 
            CLBufferedScreenGrabber::DEFAULT_QUEUE_SIZE, 
            CLBufferedScreenGrabber::DEFAULT_FRAME_RATE, 
            m_captureMode, 
            m_captureCursor, 
            m_captureResolution);

    avcodec_init();
    av_register_all();


    av_log_set_callback(FffmpegLog::av_log_default_callback_impl);


    m_videoBufSize = avpicture_get_size((PixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height());
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = avcodec_alloc_frame();
    avpicture_alloc((AVPicture*) m_frame, m_grabber->format(), m_grabber->width(), m_grabber->height() );

    QString videoCodecName = "libx264";
    if (m_encodeQualuty <= 0.5)
        videoCodecName = "mpeg2video";
    AVCodec* videoCodec = avcodec_find_encoder_by_name(videoCodecName.toAscii().constData());
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
    m_videoCodecCtx->thread_count = QThread::idealThreadCount();
    
    
    m_videoCodecCtx->time_base = m_grabber->getFrameRate();

    m_videoCodecCtx->pix_fmt = m_grabber->format(); 
    m_videoCodecCtx->width = m_grabber->width();
    m_videoCodecCtx->height = m_grabber->height();


    m_videoCodecCtx->bit_rate = calculateBitrate();
    //m_videoCodecCtx->rc_buffer_size = m_videoCodecCtx->bit_rate;
    //m_videoCodecCtx->rc_max_rate = m_videoCodecCtx->bit_rate;
    

    QString codec_prop = "";

    if (videoCodecName != "libx264")
    {
        m_videoCodecCtx->gop_size = 12;
        m_videoCodecCtx->has_b_frames = 0;
        m_videoCodecCtx->max_b_frames = 0;
        m_videoCodecCtx->me_threshold = 0;
        m_videoCodecCtx->intra_dc_precision = 0;
        m_videoCodecCtx->strict_std_compliance = 0;
        m_videoCodecCtx->me_method = ME_EPZS;
    }
    else {
        if (m_encodeQualuty <= 0.75)
            codec_prop = "refs=2;me_method=dia;subq=3;me_range=16;g=50;keyint_min=25;sc_threshold=40;i_qfactor=0.71;b_strategy=1;qcomp=0.6;qmin=10;qmax=51;qdiff=4;bf=3";
    }

    QStringList prop_list = codec_prop.split(";", QString::SkipEmptyParts);
    for (int i=0; i<prop_list.size();i++)    
    {
        QStringList param = prop_list.at(i).split("=", QString::SkipEmptyParts);
        if (param.size()==2) 
        {
            int res = av_set_string3(m_videoCodecCtx, param.at(0).trimmed().toAscii().data(), param.at(1).trimmed().toAscii().data(), 1, NULL);
            if (res != 0)
                cl_log.log("Wrong option for video codec:", param.at(0), cl_logWARNING);
        }
    }
    

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
    if (m_useAudio)
    {
        m_audioFormat = m_audioDevice.preferredFormat();
        m_audioFormat.setSampleRate(48000);
        m_audioFormat.setSampleSize(16);
        m_audioFormat.setChannels(2);
        m_audioFormat.setSampleType(QAudioFormat::SignedInt);

        if (!m_audioDevice.isFormatSupported(m_audioFormat))
        {
            m_audioFormat.setChannels(1);
            if (!m_audioDevice.isFormatSupported(m_audioFormat))
            {
                cl_log.log("Unsupported audio format specified for capturing!", cl_logERROR);
                return false;
            }
        }


        if (m_useSecondaryAudio)
        {
            m_audioFormat2 = m_audioDevice2.preferredFormat();
            m_audioFormat2.setSampleRate(48000);
            m_audioFormat2.setSampleSize(16);
            m_audioFormat2.setChannels(2);
            m_audioFormat2.setSampleType(QAudioFormat::SignedInt);
            if (!m_audioDevice2.isFormatSupported(m_audioFormat2))
            {
                m_audioFormat.setChannels(1);
                if (!m_audioDevice2.isFormatSupported(m_audioFormat2))
                {
                    cl_log.log("Unsupported audio format specified for capturing!", cl_logERROR);
                    return false;
                }
            }
        }

        m_encodedAudioBuf = (quint8*) av_malloc(FF_MIN_BUFFER_SIZE);
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
            cl_log.log("Can't find audio encoder", cl_logWARNING);
            return false;
        }
        m_outputCtx->audio_codec = audioCodec->id;

        m_audioCodecCtx = m_audioOutStream->codec;
        m_audioCodecCtx->codec_id = m_outputCtx->audio_codec;
        m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioCodecCtx->sample_fmt = audioFormatQtToFfmpeg(m_audioFormat);
        m_audioCodecCtx->channels = m_useSecondaryAudio ? 2 : m_audioFormat.channels();
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
        m_audioFrameDuration = m_audioCodecCtx->frame_size / (double) m_audioCodecCtx->sample_rate;
        m_audioFrameDuration *= m_audioOutStream->time_base.den / (double) m_audioOutStream->time_base.num;
        
        // 50 ms as max jitter
        // QT uses 25fps timer for audio grabbing, so jitter 40ms + 10ms reserved.
        m_maxAudioJitter = m_audioOutStream->time_base.den / m_audioOutStream->time_base.num / 20; 

        m_audioInput = new QAudioInput ( m_audioDevice, m_audioFormat);
        if (m_useSecondaryAudio)
            m_audioInput2 = new QAudioInput ( m_audioDevice2, m_audioFormat2);
        //m_audioInput->moveToThread(QApplication::instance()->thread());

        m_audioOStream = new CaptureAudioStream(this);
        m_audioOStream->open(QIODevice::WriteOnly | QIODevice::Truncate);

        m_audioOStream2 = new CaptureAudioStream(this);
        static_cast<CaptureAudioStream*>(m_audioOStream2)->setPrimary(false);
        m_audioOStream2->open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

    av_write_header(m_formatCtx);

    m_grabber->start(QThread::HighestPriority);
    if (m_audioInput)
        m_audioInput->start(m_audioOStream);
    if (m_audioInput2)
        m_audioInput2->start(m_audioOStream2);

    return true;
}

int DesktopFileEncoder::processData(bool flush)
{
    if (!flush)
    {
        if (m_encodedFrames == 0)
            m_encodedFrames = m_frame->pts;
        else {
            if (qAbs(m_frame->pts - m_encodedFrames) <= MAX_VIDEO_JITTER)
                m_frame->pts = m_encodedFrames;
            else
                m_encodedFrames = m_frame->pts;
        }
        m_encodedFrames++;
    }

    int out_size = avcodec_encode_video(m_videoCodecCtx, m_videoBuf, m_videoBufSize, flush ? 0 : m_frame);
    if (out_size < 1 && !flush)
        return out_size;

    if (out_size < 1)
    {
        int aSize = m_audioQueue.size();
        aSize = aSize;
    }

    AVPacket videoPkt;
    if (out_size > 0)
    {
        AVStream * stream = m_formatCtx->streams[m_videoStreamIndex];
        av_init_packet(&videoPkt);
        if (m_videoCodecCtx->coded_frame->pts != AV_NOPTS_VALUE)
            videoPkt.pts = av_rescale_q(m_videoCodecCtx->coded_frame->pts, m_videoCodecCtx->time_base, stream->time_base);

        if(m_videoCodecCtx->coded_frame->key_frame)
            videoPkt.flags |= AV_PKT_FLAG_KEY;

        videoPkt.stream_index= stream->index;
        videoPkt.data = m_videoBuf;
        videoPkt.size = out_size;
    }

    // write all audio frames
    while (m_audioQueue.size() > 0 && (!m_useSecondaryAudio || m_secondAudioQueue.size() > 0))
    {
        AVRational r;
        r.num = 1;
        r.den = 1000;
        CLAbstractMediaData* audioData = m_audioQueue.front();

        qint64 audioPts =  av_rescale_q(audioData->timestamp, r, m_audioOutStream->time_base);
        qint64 expectedAudioPts = m_storedAudioPts + m_audioFramesCount * m_audioFrameDuration; 
        int audioJitter = qAbs(audioPts - expectedAudioPts);

        if (audioJitter < m_maxAudioJitter)
        {
            audioPts = expectedAudioPts;
        }
        else {
            m_storedAudioPts = audioPts;
            m_audioFramesCount = 1;
        }

        if (out_size > 0 && audioPts > videoPkt.pts)
            break;

        m_audioFramesCount++;

        AVPacket audioPacket;
        m_audioQueue.pop(audioData);

        // todo: add audio resample here

        short* buffer1 = (short*) audioData->data.data();
        if (m_useSecondaryAudio)
        {
            CLAbstractMediaData* audioData2;
            m_secondAudioQueue.pop(audioData2);
            short* buffer2 = (short*) audioData2->data.data();

            int stereoPacketSize = m_audioCodecCtx->frame_size * 2 * m_audioFormat.sampleSize()/8;
            /*
            // first mono to left, second mono to right
            // may be it is mode usefull?
            if (m_audioFormat.channels() == 1 && m_audioFormat2.channels() == 1)
            {
                monoToStereo((qint16*) m_tmpAudioBuffer1.data.data(), buffer1, buffer2, stereoPacketSize/4);
                buffer1 = (qint16*) m_tmpAudioBuffer1.data.data();
                buffer2 = 0;
            }
            */
            if (m_audioFormat.channels() == 1)
            {
                monoToStereo((qint16*) m_tmpAudioBuffer1.data.data(), buffer1, stereoPacketSize/4);
                buffer1 = (qint16*) m_tmpAudioBuffer1.data.data();
            }
            if (m_audioFormat2.channels() == 1)
            {
                monoToStereo((qint16*) m_tmpAudioBuffer2.data.data(), buffer2, stereoPacketSize/4);
                buffer2 = (qint16*) m_tmpAudioBuffer2.data.data();
            }
            if (buffer2)
                stereoAudioMux(buffer1, buffer2, stereoPacketSize / 2);
            audioData2->releaseRef();
        }
        int aEncoded = avcodec_encode_audio(m_audioCodecCtx, m_encodedAudioBuf, FF_MIN_BUFFER_SIZE, buffer1);
        if (aEncoded > 0)
        {
            av_init_packet(&audioPacket);
            audioPacket.pts = audioPts - m_audioFrameDuration;
            audioPacket.data = m_encodedAudioBuf;
            audioPacket.size = aEncoded;
            audioPacket.stream_index = m_audioOutStream->index;
            if (av_write_frame(m_formatCtx,&audioPacket)<0)	
                cl_log.log("Audio packet write error", cl_logWARNING);
            //cl_log.log("audioPacket.pts=", audioPacket.pts, cl_logALWAYS);
            //cl_log.log("audio buffer=", m_audioQueue.size(), cl_logALWAYS);
        }
        audioData->releaseRef();


    }

    if (out_size > 0)
    {
        if (av_write_frame(m_formatCtx,&videoPkt)<0)	
            cl_log.log("Video packet write error", cl_logWARNING);
        //cl_log.log("videoPkt.pts=", videoPkt.pts, cl_logALWAYS);
    }
    return out_size;
}

void DesktopFileEncoder::run()
{
    while (!m_needStop)
    {
        CLScreenGrapper::CaptureInfo capturedData = m_grabber->getNextFrame();
        if (!capturedData.opaque)
            continue;
        m_grabber->capturedDataToFrame(capturedData, m_frame);

        AVRational r;
        r.num = 1;
        r.den = 1000;
        m_frame->pts = av_rescale_q(m_frame->pts, r, m_grabber->getFrameRate());

        if (processData(false) < 0)
        {
            cl_log.log("Video encoding error. Stop recording.", cl_logWARNING);
            break;
        }
    }
    stopCapturing();
    cl_log.log("flushing video buffer",cl_logALWAYS);
    do {
    } while (processData(true) > 0); // flush buffers

    closeStream();
}

void DesktopFileEncoder::openStream()
{
    if (init())
        m_initialized = true;
}

void DesktopFileEncoder::stopCapturing()
{
    delete m_grabber;
    m_grabber = 0;
    if (m_audioOStream)
        m_audioOStream->close();
    if (m_audioOStream2)
        m_audioOStream2->close();
}

void DesktopFileEncoder::closeStream()
{
    delete m_grabber;
    m_grabber = 0;
    if (m_audioInput)
        m_audioInput->deleteLater(); 
    m_audioInput = 0;
    if (m_audioInput2)
        m_audioInput2->deleteLater();
    m_audioInput2 = 0;
    if (m_audioOStream)
        m_audioOStream->deleteLater();
    m_audioOStream = 0;
    if (m_audioOStream2)
        m_audioOStream2->deleteLater();
    m_audioOStream2 = 0;

    if (m_formatCtx)
        av_write_trailer(m_formatCtx);

    if (m_videoCodecCtx)
        avcodec_close(m_videoCodecCtx);
    m_videoCodecCtx = 0;

    if (m_audioCodecCtx)
        avcodec_close(m_audioCodecCtx);
    m_audioCodecCtx = 0;

    if (m_formatCtx)
        avformat_free_context(m_formatCtx);
    m_formatCtx = 0;

    if (m_frame)
        av_free(m_frame);
    m_frame = 0;
    if (m_videoBuf)
        av_free(m_videoBuf);
    m_videoBuf = 0;

    av_free(av_alloc_put_byte);
    m_iocontext = 0;
    if (m_device)
    {
        m_device->close();
        delete m_device;
    }
    m_device = 0;

    if (m_encodedAudioBuf)
        av_free(m_encodedAudioBuf);
    m_encodedAudioBuf = 0;

    m_initialized = false;
}

qint64 DesktopFileEncoder::currentTime() const
{
    return m_grabber->currentTime();
}


int DesktopFileEncoder::audioPacketSize(bool isPrimary)
{
    return m_audioCodecCtx->frame_size * (isPrimary ? m_audioFormat.channels() : m_audioFormat2.channels()) * m_audioFormat.sampleSize()/8;
}
