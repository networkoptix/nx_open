#include "desktop_data_provider.h"

#ifdef Q_OS_WIN

#include <intrin.h>
#include <windows.h>
#include <mmsystem.h>


static const int DEFAULT_VIDEO_STREAM_ID = 0;
static const int DEFAULT_AUDIO_STREAM_ID = 1;
static const int AUDIO_QUEUE_MAX_SIZE = 256;
static const int AUDIO_CAUPTURE_FREQUENCY = 44100;
static const int AUDIO_CAUPTURE_ALT_FREQUENCY = 48000;
static const int BASE_BITRATE = 1000 * 1000 * 10; // bitrate for best quality for fullHD mode;

static const int MAX_VIDEO_JITTER = 2;

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>

    int av_set_string3(void *obj, const char *name, const char *val, int alloc, const AVOption **o_out);
}

#include "core/datapacket/media_data_packet.h"
#include "win_audio_device_info.h"
#include "decoders/audio/ffmpeg_audio.h"
#include "utils/common/synctime.h"
#include "utils/common/log.h"
#include "utils/media/ffmpeg_helper.h"

// mux audio 1 and audio 2 to audio1 buffer
// I have used intrisicts for SSE. It is portable for MSVC, GCC (mac, linux), Intel compiler
static void stereoAudioMux(qint16* a1, qint16* a2, int lenInShort)
{
    __m128i* audio1 = (__m128i*) a1;
    __m128i* audio2 = (__m128i*) a2;
    for (int i = 0; i < lenInShort/8; ++i)
    {
        //*audio1 = _mm_avg_epu16(*audio1, *audio2);
        *audio1 = _mm_add_epi16(*audio1, *audio2); /* SSE2. */
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
            *a1 = qMax(SHRT_MIN,qMin(SHRT_MAX, ((int)*a1 + (int)*a2)));
            a1++;
            a2++;
        }
    }
}

static void monoToStereo(qint16* dst, qint16* src, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src;
        *dst++ = *src++;
    }
}

static void monoToStereo(qint16* dst, qint16* src1, qint16* src2, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src1++;
        *dst++ = *src2++;
    }
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

QnDesktopDataProvider::EncodedAudioInfo::EncodedAudioInfo(QnDesktopDataProvider* owner):
    m_owner(owner),
    m_tmpAudioBuffer(CL_MEDIA_ALIGNMENT, FF_MIN_BUFFER_SIZE),
    m_speexPreprocess(0),
    m_terminated(false)
{

}

QnDesktopDataProvider::EncodedAudioInfo::~EncodedAudioInfo()
{
    stop();
    if (m_speexPreprocess)
        speex_preprocess_state_destroy(m_speexPreprocess);
    m_speexPreprocess = 0;
}

int QnDesktopDataProvider::EncodedAudioInfo::nameToWaveIndex()
{
    int iNumDevs = waveInGetNumDevs();
    QString name;
    int devNum = 1;
    m_audioDevice.splitFullName(name, devNum);
    for(int i = 0; i < iNumDevs; ++i)
    {
        WAVEINCAPS wic;
        if(waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR)
        {
            QString tmp = QString((const QChar *) wic.szPname);
            if (name.startsWith(tmp)) {
                if (--devNum == 0)
                    return i;
            }
        }
    }
    return WAVE_MAPPER;
}

static void QT_WIN_CALLBACK waveInProc(HWAVEIN /*hWaveIn*/,
                                UINT uMsg,
                                DWORD_PTR dwInstance,
                                DWORD_PTR /*dwParam1*/,
                                DWORD_PTR /*dwParam2*/)
{
    QnDesktopDataProvider::EncodedAudioInfo* audio = (QnDesktopDataProvider::EncodedAudioInfo*) dwInstance;
    switch(uMsg)
    {
        case WIM_OPEN:
            break;
        case WIM_DATA:
            audio->gotData();
            break;
        case WIM_CLOSE:
            break;
        default:
            return;
    }
}

void QnDesktopDataProvider::EncodedAudioInfo::clearBuffers()
{
    while (m_buffers.size() > 0)
    {
        WAVEHDR* data = m_buffers.dequeue();
        waveInUnprepareHeader(hWaveIn, data, sizeof(WAVEHDR));
        av_free(data->lpData);
        delete data;
    }
}

void QnDesktopDataProvider::EncodedAudioInfo::gotData()
{
    if (m_terminated)
        return;
    QMutexLocker lock(&m_mtx);
    if (m_buffers.isEmpty())
        return;
    WAVEHDR* data = m_buffers.front();
    if(data->dwBytesRecorded > 0 && data->dwFlags & WHDR_DONE)
    {
        // write data
        int packetSize = data->dwBytesRecorded;
        QnWritableCompressedAudioDataPtr outData(new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, packetSize));
        outData->m_data.write(data->lpData, data->dwBytesRecorded);
        outData->timestamp = m_owner->currentTime(); // - m_startDelay;
        //cl_log.log("got audio data. time=", outData->timestamp, cl_logALWAYS);
        m_audioQueue.push(outData);

        waveInUnprepareHeader(hWaveIn, data, sizeof(WAVEHDR));
        av_free(data->lpData);
        delete data;
        m_buffers.dequeue();
        addBuffer();
    }
}

bool QnDesktopDataProvider::EncodedAudioInfo::addBuffer()
{
    WAVEHDR* buffer = new WAVEHDR();
    HRESULT hr;
    memset(buffer, 0, sizeof(WAVEHDR));
    buffer->dwBufferLength = audioPacketSize();
    buffer->lpData = (LPSTR) av_malloc(audioPacketSize());

    m_buffers << buffer;

    hr = waveInPrepareHeader(hWaveIn, buffer, sizeof(WAVEHDR));
    if (hr != S_OK) return false;

    hr = waveInAddBuffer(hWaveIn, buffer, sizeof(WAVEHDR));
    if (hr != S_OK) return false;

    return true;
}

void QnDesktopDataProvider::EncodedAudioInfo::stop()
{
    m_terminated = true;
    QMutexLocker lock(&m_mtx);
    waveInReset(hWaveIn);
    waveInClose(hWaveIn);
    clearBuffers();
}

bool QnDesktopDataProvider::EncodedAudioInfo::start()
{
    return waveInStart(hWaveIn) == S_OK;
}

int QnDesktopDataProvider::EncodedAudioInfo::audioPacketSize()
{
    return m_owner->m_audioCodecCtx->frame_size * m_audioFormat.channelCount() * m_audioFormat.sampleSize()/8;
}

bool QnDesktopDataProvider::EncodedAudioInfo::setupFormat(QString& errMessage)
{
    m_audioFormat = m_audioDevice.preferredFormat();
    m_audioFormat.setSampleRate(AUDIO_CAUPTURE_FREQUENCY);
    m_audioFormat.setSampleSize(16);
    m_audioFormat.setChannelCount(2);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    if (!m_audioDevice.isFormatSupported(m_audioFormat))
    {
        m_audioFormat.setChannelCount(1);
        if (!m_audioDevice.isFormatSupported(m_audioFormat))
        {
            m_audioFormat.setSampleRate(AUDIO_CAUPTURE_ALT_FREQUENCY);
            if (!m_audioDevice.isFormatSupported(m_audioFormat)) {
                errMessage = tr("44.1Khz and 48Khz audio formats are not supported by audio capturing device! Please select other audio device or 'none' value in screen recording settings");
                return false;
            }
        }
    }
    m_audioQueue.setMaxSize(AUDIO_QUEUE_MAX_SIZE);
    return true;
}

bool QnDesktopDataProvider::EncodedAudioInfo::setupPostProcess()
{
    int devId = nameToWaveIndex();
    WAVEFORMATEX wfx;
    //HRESULT hr;
    wfx.nSamplesPerSec = m_audioFormat.sampleRate();
    wfx.wBitsPerSample = m_audioFormat.sampleSize();
    wfx.nChannels = m_audioFormat.channelCount();
    wfx.cbSize = 0;

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    if(waveInOpen(&hWaveIn, devId, &wfx,
        (DWORD_PTR)&waveInProc,
        (DWORD_PTR) this,
        CALLBACK_FUNCTION) != MMSYSERR_NOERROR != S_OK)
        return false;

    for (int i = 0; i < AUDIO_BUFFERS_COUNT; ++i)
    {
        if (!addBuffer())
            return false;
    }

    QnWinAudioDeviceInfo extInfo(m_audioDevice.deviceName());
    if (extInfo.isMicrophone())
    {
        m_speexPreprocess = speex_preprocess_state_init(m_owner->m_audioCodecCtx->frame_size * m_owner->m_audioCodecCtx->channels, m_owner->m_audioCodecCtx->sample_rate);
        int denoiseEnabled = 1;
        int agcEnabled = 1;
        float agcLevel = 16000;
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoiseEnabled);
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_AGC, &agcEnabled);
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_AGC_LEVEL, &agcLevel);
    }
    return true;
}

QnDesktopDataProvider::QnDesktopDataProvider (
                   QnResourcePtr res,
                   int desktopNum,
                   const QnAudioDeviceInfo* audioDevice,
                   const QnAudioDeviceInfo* audioDevice2,
                   Qn::CaptureMode captureMode,
                   bool captureCursor,
                   const QSize& captureResolution,
                   float encodeQualuty, // in range 0.0 .. 1.0
                   QWidget* glWidget,
                   const QPixmap& logo
                   ):
    QnAbstractMediaStreamDataProvider(res),
    m_videoBuf(0),
    m_videoBufSize(0),
    m_videoCodecCtx(0),
    m_audioCodecCtx(0),
    m_grabber(0),
    m_desktopNum(desktopNum),

    m_audioFramesCount(0),
    m_audioFrameDuration(0),
    m_storedAudioPts(0),
    m_maxAudioJitter(0),

    m_captureMode(captureMode),
    m_captureCursor(captureCursor),
    m_captureResolution(captureResolution),
    m_encodeQualuty(encodeQualuty),
    m_encodedFrames(0),
    m_widget(glWidget),
    m_encodedAudioBuf(0),
    m_capturingStopped(false),
    m_logo(logo),
    m_started(false),
    m_isInitialized(false)
{
    if (audioDevice || audioDevice2)
    {
        m_audioInfo << new EncodedAudioInfo(this);
        m_audioInfo[0]->m_audioDevice = audioDevice ? *audioDevice : *audioDevice2;
    }

    if (audioDevice && audioDevice2)
    {
        m_audioInfo << new EncodedAudioInfo(this); // second channel
        m_audioInfo[1]->m_audioDevice = *audioDevice2;
    }

    m_needStop = false;
}

QnDesktopDataProvider::~QnDesktopDataProvider()
{
    stop();
}

int QnDesktopDataProvider::calculateBitrate()
{
    double bitrate = BASE_BITRATE;

    bitrate /=  1920.0*1080.0 / m_grabber->width() / m_grabber->height();
    
    bitrate *= m_encodeQualuty;
    if (m_grabber->width() <= 320)
        bitrate *= 1.5;
    return bitrate;
}

bool QnDesktopDataProvider::init()
{
    m_initTime = AV_NOPTS_VALUE;
    m_grabber = new QnBufferedScreenGrabber(
            m_desktopNum,
            QnBufferedScreenGrabber::DEFAULT_QUEUE_SIZE,
            QnBufferedScreenGrabber::DEFAULT_FRAME_RATE,
            m_captureMode,
            m_captureCursor,
            m_captureResolution,
            m_widget);
    m_grabber->setLogo(m_logo);

    //av_log_set_callback(FffmpegLog::av_log_default_callback_impl);


    m_videoBufSize = avpicture_get_size((PixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height());
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = avcodec_alloc_frame();
    avpicture_alloc((AVPicture*) m_frame, m_grabber->format(), m_grabber->width(), m_grabber->height() );

    QString videoCodecName = QLatin1String("mpeg4"); // "libx264"
    if (m_encodeQualuty <= 0.5)
        videoCodecName = QLatin1String("mpeg2video");
    AVCodec* videoCodec = avcodec_find_encoder_by_name(videoCodecName.toLatin1().constData());
    if(videoCodec == 0)
    {
        m_lastErrorStr = tr("Could not find video encoder %1.").arg(videoCodecName);
        return false;
    }

    if (m_grabber->width() % 8 != 0) {
        m_lastErrorStr = tr("Screen width must be a multiplier of 8.");
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(videoCodec);
    m_videoCodecCtx->codec_id = videoCodec->id;
    m_videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO ;
    m_videoCodecCtx->thread_count = QThread::idealThreadCount();


    m_videoCodecCtx->time_base = m_grabber->getFrameRate();

    m_videoCodecCtx->pix_fmt = m_grabber->format();
    m_videoCodecCtx->coded_width = m_videoCodecCtx->width = m_grabber->width();
    m_videoCodecCtx->coded_height = m_videoCodecCtx->height = m_grabber->height();
    //m_videoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;


    m_videoCodecCtx->bit_rate = calculateBitrate();
    //m_videoCodecCtx->rc_buffer_size = m_videoCodecCtx->bit_rate;
    //m_videoCodecCtx->rc_max_rate = m_videoCodecCtx->bit_rate;
    
    QString codec_prop;

    //if (videoCodecName != QLatin1String("libx264"))
    if (m_encodeQualuty == 1)
    {
        m_videoCodecCtx->has_b_frames = 1;
        m_videoCodecCtx->level = 50;
        //m_videoCodecCtx->me_threshold = 0;
        //m_videoCodecCtx->intra_dc_precision = 0;
        //m_videoCodecCtx->strict_std_compliance = 0;
        m_videoCodecCtx->me_method = ME_EPZS;
        m_videoCodecCtx->gop_size = 25;
        m_videoCodecCtx->trellis = 2;
        m_videoCodecCtx->flags |= CODEC_FLAG_AC_PRED;
        m_videoCodecCtx->flags |= CODEC_FLAG_4MV;
        //"mv4=1:aic=1";
    }
    /*
    else {
        if (m_encodeQualuty <= 0.75)
            codec_prop = QLatin1String("refs=2;me_method=dia;subq=3;me_range=16;g=50;keyint_min=25;sc_threshold=40;i_qfactor=0.71;b_strategy=1;qcomp=0.6;qmin=10;qmax=51;qdiff=4;bf=3");
        m_videoCodecCtx->gop_size = 50;
    }
    */

    QStringList prop_list = codec_prop.split(QLatin1Char(';'), QString::SkipEmptyParts);
    for (int i=0; i<prop_list.size();i++)
    {
        QStringList param = prop_list.at(i).split(QLatin1Char('='), QString::SkipEmptyParts);
        if (param.size()==2)
        {
            int res = av_set_string3(m_videoCodecCtx, param.at(0).trimmed().toLatin1().data(), param.at(1).trimmed().toLatin1().data(), 1, NULL);
            if (res != 0)
                cl_log.log(QLatin1String("Wrong option for video codec:"), param.at(0), cl_logWARNING);
        }
    }


    if (m_captureResolution.width() > 0) 
    {
        double srcAspect = m_grabber->screenWidth() / (double) m_grabber->screenHeight();
        double dstAspect = m_captureResolution.width() / (double) m_captureResolution.height();
        double sar = srcAspect / dstAspect;
        m_videoCodecCtx->sample_aspect_ratio = av_d2q(sar, 255);
    }
    else {
        m_videoCodecCtx->sample_aspect_ratio.num = 1;
        m_videoCodecCtx->sample_aspect_ratio.den = 1;
    }

    m_videoCodecCtxPtr = QnMediaContextPtr(new QnMediaContext(m_videoCodecCtx));


    if (avcodec_open(m_videoCodecCtx, videoCodec) < 0)
    {
        m_lastErrorStr = tr("Could not initialize video encoder.");
        return false;
    }

    // init audio capture
    if (!m_audioInfo.isEmpty())
    {
        foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        {
            if (!audioChannel->setupFormat(m_lastErrorStr))
                return false;
        }

        m_encodedAudioBuf = (quint8*) av_malloc(FF_MIN_BUFFER_SIZE);

        QString audioCodecName = QLatin1String("libmp3lame"); // ""aac
        AVCodec* audioCodec = avcodec_find_encoder_by_name(audioCodecName.toLatin1().constData());
        if(audioCodec == 0)
        {
            m_lastErrorStr = tr("Could not find audio encoder '%1'.").arg(audioCodecName);
            return false;
        }

        m_audioCodecCtx = avcodec_alloc_context3(audioCodec);
        m_audioCodecCtx->codec_id = audioCodec->id;
        m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioCodecCtx->sample_fmt = CLFFmpegAudioDecoder::audioFormatQtToFfmpeg(m_audioInfo[0]->m_audioFormat);
        m_audioCodecCtx->channels = m_audioInfo.size() > 1 ? 2 : m_audioInfo[0]->m_audioFormat.channelCount();
        m_audioCodecCtx->sample_rate = m_audioInfo[0]->m_audioFormat.sampleRate();
        AVRational audioRational = {1, m_audioCodecCtx->sample_rate};
        m_audioCodecCtx->time_base         = audioRational;
        m_audioCodecCtx->bit_rate = 64000 * m_audioCodecCtx->channels;
        //m_audioCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        m_audioCodecCtxPtr = QnMediaContextPtr(new QnMediaContext(m_audioCodecCtx));

        if (avcodec_open(m_audioCodecCtx, audioCodec) < 0)
        {
            m_lastErrorStr = tr("Could not initialize audio encoder.");
            return false;
        }

        foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        {
            if (!audioChannel->setupPostProcess())
            {
                m_lastErrorStr = tr("Could not initialize audio device '%1'.").arg(audioChannel->m_audioDevice.fullName());
                return false;
            }
        }
    }

    if (m_audioCodecCtx)
        m_audioFrameDuration = m_audioCodecCtx->frame_size / (double) m_audioCodecCtx->sample_rate * 1000; // keep in ms
    //m_audioFrameDuration *= m_audioOutStream->time_base.den / (double) m_audioOutStream->time_base.num;

    // 50 ms as max jitter
    // QT uses 25fps timer for audio grabbing, so jitter 40ms + 10ms reserved.
    m_maxAudioJitter = 1000 / 20; // keep in ms
    //m_maxAudioJitter = m_audioOutStream->time_base.den / (double) m_audioOutStream->time_base.num / 20;


    m_grabber->start(QThread::HighestPriority);
    foreach(EncodedAudioInfo* info, m_audioInfo)
    {
        if (!info->start())
        {
            m_lastErrorStr = tr("Could not start primary audio device.");
            return false;
        }
    }

    // codecCtxptr was created before open() call to avoid encoder specific fields.
    // transfer extra_data field manually

    if (m_videoCodecCtx->extradata_size)
    {
        m_videoCodecCtxPtr->ctx()->extradata_size = m_videoCodecCtx->extradata_size;
        m_videoCodecCtxPtr->ctx()->extradata = (uint8_t*) av_malloc(m_videoCodecCtx->extradata_size);
        memcpy(m_videoCodecCtxPtr->ctx()->extradata, m_videoCodecCtx->extradata, m_videoCodecCtx->extradata_size);
    }

    if (m_audioCodecCtx && m_audioCodecCtx->extradata_size)
    {
        m_audioCodecCtxPtr->ctx()->extradata_size = m_audioCodecCtx->extradata_size;
        m_audioCodecCtxPtr->ctx()->extradata = (uint8_t*) av_malloc(m_audioCodecCtx->extradata_size);
        memcpy(m_audioCodecCtxPtr->ctx()->extradata, m_audioCodecCtx->extradata, m_audioCodecCtx->extradata_size);
    }

    //m_videoCodecCtxPtr = QnMediaContextPtr(new QnMediaContext(m_videoCodecCtx));
    //m_videoCodecCtxPtr->ctx()->stats_out = 0;
    //m_videoCodecCtxPtr->ctx()->coded_frame = 0;


    return true;
}

int QnDesktopDataProvider::processData(bool flush)
{
    if (m_videoCodecCtx == 0)
        return -1;
    int out_size = avcodec_encode_video(m_videoCodecCtx, m_videoBuf, m_videoBufSize, flush ? 0 : m_frame);

    if (out_size < 1 && !flush)
        return out_size;

    AVRational timeBaseMs;
    timeBaseMs.num = 1;
    timeBaseMs.den = 1000;

    AVRational timeBaseNative;
    timeBaseNative.num = 1;
    timeBaseNative.den = 1000000;

    if (m_initTime == AV_NOPTS_VALUE)
        m_initTime = qnSyncTime->currentUSecsSinceEpoch();

    if (out_size > 0)
    {

        QnWritableCompressedVideoDataPtr video = QnWritableCompressedVideoDataPtr(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, out_size, m_videoCodecCtxPtr));
        video->m_data.write((const char*) m_videoBuf, out_size);
        video->compressionType = m_videoCodecCtx->codec_id;
        video->timestamp = av_rescale_q(m_videoCodecCtx->coded_frame->pts, m_videoCodecCtx->time_base, timeBaseNative) + m_initTime;

        if(m_videoCodecCtx->coded_frame->key_frame)
            video->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        video->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        video->dataProvider = this;
        putData(video);
    }

    // write all audio frames
    EncodedAudioInfo* ai = m_audioInfo.size() > 0 ? m_audioInfo[0] : 0;
    EncodedAudioInfo* ai2 = m_audioInfo.size() > 1 ? m_audioInfo[1] : 0;
    while (ai && ai->m_audioQueue.size() > 0 && (ai2 == 0 || ai2->m_audioQueue.size() > 0))
    {
        QnWritableCompressedAudioDataPtr audioData = ai->m_audioQueue.front().staticCast<QnWritableCompressedAudioData>();

        qint64 audioPts = audioData->timestamp - m_audioFrameDuration;
        qint64 expectedAudioPts = m_storedAudioPts + m_audioFramesCount * m_audioFrameDuration;
        int audioJitter = qAbs(audioPts - expectedAudioPts);

        //cl_log.log("audio jitter=", audioJitter, cl_logALWAYS); // all in ms

        if (audioJitter < m_maxAudioJitter)
        {
            audioPts = expectedAudioPts;
        }
        else {
            m_storedAudioPts = audioPts;
            m_audioFramesCount = 0;
        }


        m_audioFramesCount++;

        QnWritableCompressedAudioDataPtr mediaData;
        ai->m_audioQueue.pop(mediaData);

        // todo: add audio resample here

        short* buffer1 = (short*) audioData->m_data.data();
        if (ai->m_speexPreprocess)
            speex_preprocess(ai->m_speexPreprocess, buffer1, NULL);

        if (ai2)
        {
            QnWritableCompressedAudioDataPtr audioData2;
            ai2->m_audioQueue.pop(audioData2);
            short* buffer2 = (short*) audioData2->m_data.data();
            if (ai2->m_speexPreprocess)
                speex_preprocess(ai2->m_speexPreprocess, buffer2, NULL);

            int stereoPacketSize = m_audioCodecCtx->frame_size * 2 * ai->m_audioFormat.sampleSize()/8;
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
            if (ai->m_audioFormat.channelCount() == 1)
            {
                monoToStereo((qint16*) ai->m_tmpAudioBuffer.m_data.data(), buffer1, stereoPacketSize/4);
                buffer1 = (qint16*) ai->m_tmpAudioBuffer.m_data.data();
            }
            if (ai2->m_audioFormat.channelCount() == 1)
            {
                monoToStereo((qint16*) ai2->m_tmpAudioBuffer.m_data.data(), buffer2, stereoPacketSize/4);
                buffer2 = (qint16*) ai2->m_tmpAudioBuffer.m_data.data();
            }
            if (buffer2)
                stereoAudioMux(buffer1, buffer2, stereoPacketSize / 2);
        }
        int aEncoded = avcodec_encode_audio(m_audioCodecCtx, m_encodedAudioBuf, FF_MIN_BUFFER_SIZE, buffer1);
        if (aEncoded > 0)
        {
            QnWritableCompressedAudioDataPtr audio = QnWritableCompressedAudioDataPtr(new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, aEncoded, m_audioCodecCtxPtr));
            audio->m_data.write((const char*) m_encodedAudioBuf, aEncoded);
            audio->compressionType = m_audioCodecCtx->codec_id;
            audio->timestamp = av_rescale_q(audioPts, timeBaseMs, timeBaseNative) + m_initTime;
            //audio->timestamp = av_rescale_q(m_audioCodecCtx->coded_frame->pts, m_audioCodecCtx->time_base, timeBaseNative) + m_initTime;
            audio->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            audio->dataProvider = this;
            audio->channelNumber = 1;
            putData(audio);
        }
    }

    return out_size;
}

void QnDesktopDataProvider::start(Priority priority)
{
    QMutexLocker lock(&m_startMutex);
    if (m_started)
        return;
    m_started = true;

    m_isInitialized = init();
    if (!m_isInitialized) {
        m_needStop = true;
        return;
    }
    QnLongRunnable::start(priority);
}

bool QnDesktopDataProvider::isInitialized() const
{
    return m_isInitialized;
}

void QnDesktopDataProvider::run()
{
    while (!needToStop() || m_grabber->dataExist())
    {
        if (needToStop() && !m_capturingStopped)
        {
            stopCapturing();
            m_capturingStopped = true;
        }

        CaptureInfoPtr capturedData = m_grabber->getNextFrame();
        if (!capturedData || !capturedData->opaque || capturedData->width == 0 || capturedData->height == 0)
            continue;
        m_grabber->capturedDataToFrame(capturedData, m_frame);

        AVRational r;
        r.num = 1;
        r.den = 1000;
        qint64 capturedPts = av_rescale_q(m_frame->pts, r, m_grabber->getFrameRate());


        if (m_encodedFrames == 0)
            m_encodedFrames = capturedPts;

        //cl_log.log("captured pts=", capturedPts, cl_logALWAYS);

        bool firstStep = true;
        while (firstStep || capturedPts - m_encodedFrames >= MAX_VIDEO_JITTER)
        {
            m_frame->pts = m_encodedFrames;
            if (processData(false) < 0)
            {
                cl_log.log(QLatin1String("Video encoding error. Stop recording."), cl_logWARNING);
                break;
            }
            //cl_log.log(QLatin1String("encode pts="), m_encodedFrames, cl_logALWAYS);

            m_encodedFrames++;
            firstStep = false;
        }
    }
    if (!m_capturingStopped)
        stopCapturing();

    cl_log.log(QLatin1String("flushing video buffer"), cl_logALWAYS);
    do {
    } while (processData(true) > 0); // flush buffers

    closeStream();
}

void QnDesktopDataProvider::stopCapturing()
{
    foreach(EncodedAudioInfo* info, m_audioInfo)
        info->stop();
    m_grabber->pleaseStop();
}

void QnDesktopDataProvider::closeStream()
{
    delete m_grabber;
    m_grabber = 0;

    QnFfmpegHelper::deleteCodecContext(m_videoCodecCtx);
    m_videoCodecCtx = 0;
    QnFfmpegHelper::deleteCodecContext(m_audioCodecCtx);
    m_audioCodecCtx = 0;

    if (m_frame) {
        avpicture_free((AVPicture*) m_frame);
        av_free(m_frame);
        m_frame = 0;
    }

    if (m_videoBuf)
        av_free(m_videoBuf);
    m_videoBuf = 0;

    if (m_encodedAudioBuf)
        av_free(m_encodedAudioBuf);
    m_encodedAudioBuf = 0;

    m_audioInfo.clear();

    foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        delete audioChannel;
    m_audioInfo.clear();
}

class QnDesktopDataProvider::QnDesktopAudioLayout: public QnResourceAudioLayout
{
public:
    QnDesktopAudioLayout(): QnResourceAudioLayout(), m_channels(0) {}
    virtual int channelCount() const override { return m_channels; }
    virtual AudioTrack getAudioTrackInfo(int /*index*/) const override { return m_audioTrack; }
    void setAudioTrackInfo(const AudioTrack& info) { m_audioTrack = info; m_channels = 1;}
private:
    AudioTrack m_audioTrack;
    int m_channels;
};

QnConstResourceAudioLayoutPtr QnDesktopDataProvider::getAudioLayout()
{
    if (!m_audioLayout)
        m_audioLayout.reset(new QnDesktopAudioLayout() );
    if (m_audioCodecCtx && m_audioLayout->channelCount() == 0)
    {
        QnResourceAudioLayout::AudioTrack track;
        track.codecContext = QnAbstractMediaContextPtr(new QnMediaContext(m_audioCodecCtx));
        m_audioLayout->setAudioTrackInfo(track);
    }
    return m_audioLayout;
}

qint64 QnDesktopDataProvider::currentTime() const
{
    return m_grabber->currentTime();
}

void QnDesktopDataProvider::beforeDestroyDataProvider(QnAbstractDataConsumer* consumer)
{
    QMutexLocker lock(&m_startMutex);
    removeDataProcessor(consumer);
    if (processorsCount() == 0) 
        pleaseStop();
}

void QnDesktopDataProvider::addDataProcessor(QnAbstractDataConsumer* consumer)
{
    QMutexLocker lock(&m_startMutex);
    if (needToStop())
        wait(); // wait previous thread instance
    QnAbstractMediaStreamDataProvider::addDataProcessor(consumer);
    m_started = false; // now we ready to restart
}

bool QnDesktopDataProvider::readyToStop() const
{
    QMutexLocker lock(&m_startMutex);
    return processorsCount() == 0;
}

/*
void QnDesktopDataProvider::putData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media  = data.dynamicCast<QnAbstractMediaData>();
    if (!media)
        return;

    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
        if (dp->canAcceptData()) 
        {
            if (media->dataType == QnAbstractMediaData::VIDEO)
            {
                QSet<void*>::iterator itr = m_needKeyData.find(dp);
                if (itr != m_needKeyData.end())
                {
                    if (media->flags | AV_PKT_FLAG_KEY)
                        m_needKeyData.erase(itr);
                    else
                        continue; // skip data
                }
            }
            dp->putData(data);
        }
        else {
            m_needKeyData << dp;
        }
    }
}
*/

#endif // Q_OS_WIN
