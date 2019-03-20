#include "desktop_data_provider.h"

#include <intrin.h>
#include <windows.h>
#include <mmsystem.h>

#include <common/common_module.h>
#include <api/global_settings.h>


static const int DEFAULT_VIDEO_STREAM_ID = 0;
static const int DEFAULT_AUDIO_STREAM_ID = 1;
static const int AUDIO_QUEUE_MAX_SIZE = 256;
static const int AUDIO_CAUPTURE_FREQUENCY = 44100;
static const int AUDIO_CAUPTURE_ALT_FREQUENCY = 48000;
static const int BASE_BITRATE = 1000 * 1000 * 10; // bitrate for best quality for fullHD mode;

static const int MAX_VIDEO_JITTER = 2;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
} // extern "C"

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/config.h>
#include <plugins/resource/desktop_win/win_audio_device_info.h>
#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>
#include <utils/media/ffmpeg_helper.h>
#include <nx/streaming/video_data_packet.h>

namespace {

    struct FffmpegLog
    {
        static void av_log_default_callback_impl(void* ptr, int level, const char* fmt, va_list vl)
        {
            Q_UNUSED(level)
                Q_UNUSED(ptr)
                NX_ASSERT(fmt && "NULL Pointer");

            if (!fmt) {
                return;
            }
            static char strText[1024 + 1];
            vsnprintf(&strText[0], sizeof(strText) - 1, fmt, vl);
            va_end(vl);

            qDebug() << "ffmpeg library: " << strText;
        }
    };

} // namespace

QnDesktopDataProvider::EncodedAudioInfo::EncodedAudioInfo(QnDesktopDataProvider* owner):
    m_tmpAudioBuffer(CL_MEDIA_ALIGNMENT, FF_MIN_BUFFER_SIZE),
    m_owner(owner)
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
    QString name = m_audioDevice.name();
    int devNum = m_audioDevice.deviceNumber();
    for (int i = 0; i < iNumDevs; ++i)
    {
        WAVEINCAPS wic;
        if(waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR)
        {
            QString tmp = QString((const QChar *) wic.szPname);
            if (name == tmp)
            {
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
    QnMutexLocker lock( &m_mtx );
    if (m_terminated)
        return;

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
    {
        QnMutexLocker lock( &m_mtx );
        m_terminated = true;
        if (!m_waveInOpened)
            return;
        m_waveInOpened = false;
    }
    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    waveInClose(hWaveIn);
    clearBuffers();
}

bool QnDesktopDataProvider::EncodedAudioInfo::start()
{
    QnMutexLocker lock(&m_mtx);
    if (m_terminated)
        return false;
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
            if (!m_audioDevice.isFormatSupported(m_audioFormat))
            {
                errMessage = tr("44.1 kHz and 48 kHz audio formats are not supported by the audio "
                    "capturing device. Please select another audio device or \"none\" in the "
                    "Screen Recording settings.");
                return false;
            }
        }
    }
    m_audioQueue.setMaxSize(AUDIO_QUEUE_MAX_SIZE);
    return true;
}

bool QnDesktopDataProvider::EncodedAudioInfo::setupPostProcess()
{
    QnMutexLocker lock(&m_mtx);
    if (m_terminated)
        return false;

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
    m_waveInOpened = true;
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
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoiseEnabled);

        int gainLevel = 16;
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_AGC_INCREMENT, &gainLevel);
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &gainLevel);
    }
    return true;
}

QnDesktopDataProvider::QnDesktopDataProvider(
    QnResourcePtr res,
    int desktopNum,
    const QnAudioDeviceInfo* audioDevice,
    const QnAudioDeviceInfo* audioDevice2,
    Qn::CaptureMode captureMode,
    bool captureCursor,
    const QSize& captureResolution,
    float encodeQualuty, //< In range [0.0, 1.0].
    QWidget* glWidget,
    const QPixmap& logo)
    :
    QnDesktopDataProviderBase(res),
    m_desktopNum(desktopNum),
    m_captureMode(captureMode),
    m_captureCursor(captureCursor),
    m_captureResolution(captureResolution),
    m_encodeQualuty(encodeQualuty),
    m_widget(glWidget),
    m_logo(logo),
    m_inputAudioFrame(av_frame_alloc()),
    m_outPacket(av_packet_alloc())
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
    av_frame_free(&m_inputAudioFrame);
    av_packet_free(&m_outPacket);
}

int QnDesktopDataProvider::calculateBitrate(const char* codecName)
{
    double bitrate = BASE_BITRATE;

    bitrate /=  1920.0*1080.0 / m_grabber->width() / m_grabber->height();

    bitrate *= m_encodeQualuty;
    if (m_grabber->width() <= 320)
        bitrate *= 1.5;
    if (strcmp(codecName, "libopenh264") == 0)
    {
        // Increase bitrate due to bad quality of libopenh264 coding.
        bitrate *= 4;
    }
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

    m_videoBufSize = av_image_get_buffer_size((AVPixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height(), /*align*/ 1);
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = av_frame_alloc();
    int ret = av_image_alloc(m_frame->data, m_frame->linesize,
        m_grabber->width(), m_grabber->height(), m_grabber->format(), /*align*/ 1);
    if (ret < 0)
        memset((AVPicture*)m_frame, 0, sizeof(AVPicture));

    QString videoCodecName;
    if (m_encodeQualuty <= 0.5)
        videoCodecName = getResource()->commonModule()->globalSettings()->lowQualityScreenVideoCodec();
    else
        videoCodecName = getResource()->commonModule()->globalSettings()->defaultExportVideoCodec();
    AVCodec* videoCodec = avcodec_find_encoder_by_name(videoCodecName.toLatin1().data());
    if (!videoCodec)
    {
        NX_WARNING(this, tr("Configured codec: %1 not found, h263p will used").arg(videoCodecName));
        videoCodec = avcodec_find_encoder(AV_CODEC_ID_H263P);
    }
    if(videoCodec == 0)
    {
        m_lastErrorStr = tr("Could not find video encoder %1.").arg(videoCodecName);
        return false;
    }

    if (m_grabber->width() % 8 != 0) {
        m_lastErrorStr = tr("Screen width must be a multiple of 8.");
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

    m_videoCodecCtx->bit_rate = calculateBitrate(videoCodec->name);
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

    /*
    QStringList prop_list = codec_prop.split(QLatin1Char(';'), QString::SkipEmptyParts);
    for (int i=0; i <prop_list.size();i++)
    {
        QStringList param = prop_list.at(i).split(QLatin1Char('='), QString::SkipEmptyParts);
        if (param.size()==2)
        {
            int res = av_set_string3(m_videoCodecCtx, param.at(0).trimmed().toLatin1().data(), param.at(1).trimmed().toLatin1().data(), 1, NULL);
        }
    }
    */

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

    const auto videoContext = new QnAvCodecMediaContext(m_videoCodecCtx);
    m_videoContext = QnConstMediaContextPtr(videoContext);

    if (avcodec_open2(m_videoCodecCtx, videoCodec, nullptr) < 0)
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

        QString audioCodecName = QLatin1String("mp2"); //< "libmp3lame"
        AVCodec* audioCodec = avcodec_find_encoder_by_name(audioCodecName.toLatin1().constData());
        if(audioCodec == 0)
        {
            m_lastErrorStr = tr("Could not find audio encoder \"%1\".").arg(audioCodecName);
            return false;
        }

        m_audioCodecCtx = avcodec_alloc_context3(audioCodec);
        m_audioCodecCtx->codec_id = audioCodec->id;
        m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
        m_audioCodecCtx->sample_fmt = QnFfmpegHelper::fromQtAudioFormatToFfmpegSampleType(m_audioInfo[0]->m_audioFormat);
        m_audioCodecCtx->channels = m_audioInfo.size() > 1 ? 2 : m_audioInfo[0]->m_audioFormat.channelCount();
        m_audioCodecCtx->sample_rate = m_audioInfo[0]->m_audioFormat.sampleRate();
        AVRational audioRational = {1, m_audioCodecCtx->sample_rate};
        m_audioCodecCtx->time_base = audioRational;
        m_audioCodecCtx->bit_rate = 64000 * m_audioCodecCtx->channels;
        //m_audioCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        const auto audioContext = new QnAvCodecMediaContext(m_audioCodecCtx);
        m_audioContext = QnConstMediaContextPtr(audioContext);
        const auto res = avcodec_open2(m_audioCodecCtx, audioCodec, nullptr);
        if (res < 0)
        {
            m_lastErrorStr = tr("Could not initialize audio encoder.");
            return false;
        }

        foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        {
            if (!audioChannel->setupPostProcess())
            {
                m_lastErrorStr = tr("Could not initialize audio device \"%1\".").arg(audioChannel->m_audioDevice.fullName());
                return false;
            }
        }

        // QnMediaContext was created before open() call to avoid encoder specific fields.
        // Transfer extradata manually.
        audioContext->setExtradata(m_audioCodecCtx->extradata, m_audioCodecCtx->extradata_size);
    }

    if (m_audioCodecCtx)
        m_audioFrameDuration = m_audioCodecCtx->frame_size / (double) m_audioCodecCtx->sample_rate * 1000; // keep in ms
    //m_audioFrameDuration *= m_audioOutStream->time_base.den / (double) m_audioOutStream->time_base.num;

    // 50 ms as max jitter
    // QT uses 25fps timer for audio grabbing, so jitter 40ms + 10ms reserved.
    m_maxAudioJitter = 1000 / 20; // keep in ms
    //m_maxAudioJitter = m_audioOutStream->time_base.den / (double) m_audioOutStream->time_base.num / 20;


    foreach (EncodedAudioInfo* info, m_audioInfo)
    {
        if (!info->start())
        {
            m_lastErrorStr = tr("Could not start primary audio device.");
            return false;
        }
    }

    // QnMediaContext was created before open() call to avoid encoder specific fields.
    // Transfer extradata manually.
    videoContext->setExtradata(m_videoCodecCtx->extradata, m_videoCodecCtx->extradata_size);

    //m_videoContext = QnMediaContextPtr(new QnAvCodecMediaContext(m_videoCodecCtx));
    //m_videoContext->ctx()->stats_out = 0;
    //m_videoContext->ctx()->coded_frame = 0;

    return true;
}

int QnDesktopDataProvider::processData(bool flush)
{
    if (m_videoCodecCtx == 0)
        return -1;
    //int out_size = avcodec_encode_video(m_videoCodecCtx, m_videoBuf, m_videoBufSize, flush ? 0 : m_frame);

    m_outPacket->data = m_videoBuf;
    m_outPacket->size = m_videoBufSize;
    int got_packet = 0;
    int encodeResult = avcodec_encode_video2(m_videoCodecCtx, m_outPacket, flush ? 0 : m_frame, &got_packet);


    if (encodeResult < 0)
        return encodeResult; //< error

    AVRational timeBaseMs;
    timeBaseMs.num = 1;
    timeBaseMs.den = 1000;

    AVRational timeBaseNative;
    timeBaseNative.num = 1;
    timeBaseNative.den = 1000000;

    if (m_initTime == AV_NOPTS_VALUE)
        m_initTime = qnSyncTime->currentUSecsSinceEpoch();

    if (got_packet > 0)
    {

        QnWritableCompressedVideoDataPtr video = QnWritableCompressedVideoDataPtr(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, m_outPacket->size, m_videoContext));
        video->m_data.write((const char*) m_videoBuf, m_outPacket->size);
        video->compressionType = m_videoCodecCtx->codec_id;
        video->timestamp = av_rescale_q(m_videoCodecCtx->coded_frame->pts, m_videoCodecCtx->time_base, timeBaseNative) + m_initTime;

        if(m_videoCodecCtx->coded_frame->key_frame)
            video->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        video->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        video->dataProvider = this;
        putData(video);
    }

    putAudioData();

    return m_outPacket->size;
}

void QnDesktopDataProvider::putAudioData()
{
    // write all audio frames
    EncodedAudioInfo* ai = m_audioInfo.size() > 0 ? m_audioInfo[0] : 0;
    EncodedAudioInfo* ai2 = m_audioInfo.size() > 1 ? m_audioInfo[1] : 0;
    while (ai && ai->m_audioQueue.size() > 0 && (ai2 == 0 || ai2->m_audioQueue.size() > 0))
    {
        QnWritableCompressedAudioDataPtr audioData;
        ai->m_audioQueue.pop(audioData);

        qint64 audioPts = audioData->timestamp - m_audioFrameDuration;
        qint64 expectedAudioPts = m_storedAudioPts + m_audioFramesCount * m_audioFrameDuration;
        int audioJitter = qAbs(audioPts - expectedAudioPts);

        if (audioJitter < m_maxAudioJitter)
        {
            audioPts = expectedAudioPts;
        }
        else {
            m_storedAudioPts = audioPts;
            m_audioFramesCount = 0;
        }


        m_audioFramesCount++;

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

        if (!m_soundAnalyzer)
        {
            m_soundAnalyzer = QnVoiceSpectrumAnalyzer::instance();
            m_soundAnalyzer->initialize(m_audioCodecCtx->sample_rate, m_audioCodecCtx->channels);
        }
        m_soundAnalyzer->processData(buffer1, m_audioCodecCtx->frame_size);

        //int aEncoded = avcodec_encode_audio(m_audioCodecCtx, m_encodedAudioBuf, FF_MIN_BUFFER_SIZE, buffer1);
        //if (aEncoded > 0)

        m_outPacket->data = m_encodedAudioBuf;
        m_outPacket->size = FF_MIN_BUFFER_SIZE;

        m_inputAudioFrame->data[0] = (quint8*) buffer1;
        m_inputAudioFrame->nb_samples = m_audioCodecCtx->frame_size;

        int got_packet = 0;

        if (avcodec_encode_audio2(m_audioCodecCtx, m_outPacket, m_inputAudioFrame, &got_packet) < 0)
            continue;
        if (got_packet)
        {
            if (m_initTime == AV_NOPTS_VALUE)
                m_initTime = qnSyncTime->currentUSecsSinceEpoch();

            AVRational timeBaseNative;
            timeBaseNative.num = 1;
            timeBaseNative.den = 1000000;

            AVRational timeBaseMs;
            timeBaseMs.num = 1;
            timeBaseMs.den = 1000;

            QnWritableCompressedAudioDataPtr audio = QnWritableCompressedAudioDataPtr(new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, m_outPacket->size, m_audioContext));
            audio->m_data.write((const char*) m_encodedAudioBuf, m_outPacket->size);
            audio->compressionType = m_audioCodecCtx->codec_id;
            audio->timestamp = av_rescale_q(audioPts, timeBaseMs, timeBaseNative) + m_initTime;
            //audio->timestamp = av_rescale_q(m_audioCodecCtx->coded_frame->pts, m_audioCodecCtx->time_base, timeBaseNative) + m_initTime;
            audio->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            audio->dataProvider = this;
            audio->channelNumber = 1;
            putData(audio);
        }
    }
}

void QnDesktopDataProvider::start(Priority priority)
{
    QnMutexLocker lock( &m_startMutex );
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

bool QnDesktopDataProvider::needVideoData() const
{
    QnMutexLocker mutex( &m_mutex );
    for (const auto& consumer: m_dataprocessors)
    {
        if (consumer->needConfigureProvider())
            return true;
    }
    return false;
}

void QnDesktopDataProvider::run()
{
    while (!needToStop() || m_grabber->dataExist())
    {
        if (needVideoData())
        {
            m_grabber->start(QThread::HighestPriority);

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

            bool firstStep = true;
            while (firstStep || capturedPts - m_encodedFrames >= MAX_VIDEO_JITTER)
            {
                m_frame->pts = m_encodedFrames;
                if (processData(false) < 0)
                {
                    NX_WARNING(this, "Video encoding error. Stop recording.");
                    break;
                }

                m_encodedFrames++;
                firstStep = false;
            }
        }
        else
        {
            m_grabber->pleaseStop();
            putAudioData();
        }
        if (needToStop())
            break;
    }
    if (!m_capturingStopped)
        stopCapturing();

    NX_VERBOSE(this, "flushing video buffer");
    if (needVideoData())
    {
        do {
        } while (processData(true) > 0); // flush buffers
    }

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

    QnFfmpegHelper::deleteAvCodecContext(m_videoCodecCtx);
    m_videoCodecCtx = 0;
    QnFfmpegHelper::deleteAvCodecContext(m_audioCodecCtx);
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

    foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        delete audioChannel;
    m_audioInfo.clear();
}

QnConstResourceAudioLayoutPtr QnDesktopDataProvider::getAudioLayout()
{
    if (!m_audioLayout)
        m_audioLayout.reset(new QnDesktopAudioLayout() );
    if (m_audioCodecCtx && m_audioLayout->channelCount() == 0)
    {
        QnResourceAudioLayout::AudioTrack track;
        track.codecContext = QnConstMediaContextPtr(new QnAvCodecMediaContext(m_audioCodecCtx));
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
    QnMutexLocker lock( &m_startMutex );
    removeDataProcessor(consumer);
    if (processorsCount() == 0)
        pleaseStop();
}

void QnDesktopDataProvider::addDataProcessor(QnAbstractDataConsumer* consumer)
{
    QnMutexLocker lock( &m_startMutex );
    if (needToStop()) {
        wait(); // wait previous thread instance
        m_started = false; // now we ready to restart
    }
    QnAbstractMediaStreamDataProvider::addDataProcessor(consumer);
}

bool QnDesktopDataProvider::readyToStop() const
{
    QnMutexLocker lock( &m_startMutex );
    return processorsCount() == 0;
}

/*
void QnDesktopDataProvider::putData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media  = data.dynamicCast<QnAbstractMediaData>();
    if (!media)
        return;

    QnMutexLocker mutex( &m_mutex );
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
