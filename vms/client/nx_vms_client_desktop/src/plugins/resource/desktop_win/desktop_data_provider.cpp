// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider.h"

#include <intrin.h>
#include <mmsystem.h>
#include <windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
} // extern "C"

#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <plugins/resource/desktop_win/audio_device_change_notifier.h>
#include <plugins/resource/desktop_win/win_audio_device_info.h>
#include <utils/common/synctime.h>
#include <utils/media/av_options.h>
#include <utils/media/ffmpeg_helper.h>

namespace {

static const int AUDIO_QUEUE_MAX_SIZE = 256;
static const int AUDIO_CAPTURE_FREQUENCY = 44100;
static const int BASE_BITRATE = 1000 * 1000 * 10; // bitrate for best quality for fullHD mode;
static const int MAX_VIDEO_JITTER = 2;

} // namespace

QnDesktopDataProvider::EncodedAudioInfo::EncodedAudioInfo(QnDesktopDataProvider* owner):
    m_tmpAudioBuffer(AV_INPUT_BUFFER_MIN_SIZE),
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
            // This may look like "Microphone (Realtec Hi".
            QString tmp = QString((const QChar *) wic.szPname);
            if (name.startsWith(tmp))
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

void QnDesktopDataProvider::EncodedAudioInfo::gotAudioPacket(const char* data, int packetSize)
{
    QnWritableCompressedAudioDataPtr outData(new QnWritableCompressedAudioData(packetSize));
    outData->m_data.write(data, packetSize);
    outData->timestamp = m_owner->currentTime(); // - m_startDelay;
    m_audioQueue.push(outData);
}

void QnDesktopDataProvider::EncodedAudioInfo::gotData()
{
    NX_MUTEX_LOCKER lock( &m_mtx );
    if (m_terminated)
        return;

    if (m_buffers.isEmpty())
        return;
    WAVEHDR* data = m_buffers.front();
    if (data->dwBytesRecorded > 0 && data->dwFlags & WHDR_DONE)
    {
        // write data
        if (m_intermediateAudioBuffer.empty() && data->dwBytesRecorded == data->dwBufferLength)
        {
            gotAudioPacket(data->lpData, data->dwBufferLength);
        }
        else
        {
            std::copy(data->lpData, data->lpData + data->dwBytesRecorded, std::back_inserter(m_intermediateAudioBuffer));
            if (m_intermediateAudioBuffer.size() >= data->dwBufferLength)
            {
                gotAudioPacket(m_intermediateAudioBuffer.data(), data->dwBufferLength);
                m_intermediateAudioBuffer.erase(
                    m_intermediateAudioBuffer.begin(), m_intermediateAudioBuffer.begin() + data->dwBufferLength);
            }
        }
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
        NX_MUTEX_LOCKER lock( &m_mtx );
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
    NX_MUTEX_LOCKER lock(&m_mtx);
    if (m_terminated)
        return false;
    return waveInStart(hWaveIn) == S_OK;
}

int QnDesktopDataProvider::EncodedAudioInfo::audioPacketSize()
{
    return m_owner->m_frameSize * m_audioFormat.channelCount() * m_audioFormat.sampleSize()/8;
}

bool QnDesktopDataProvider::EncodedAudioInfo::setupFormat(QString& errMessage)
{
    m_audioFormat = m_audioDevice.preferredFormat();
    m_audioFormat.setSampleRate(AUDIO_CAPTURE_FREQUENCY);
    m_audioFormat.setSampleSize(16);
    m_audioFormat.setChannelCount(2);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    m_audioFormat = m_audioDevice.nearestFormat(m_audioFormat);
    if (!m_audioFormat.isValid())
    {
        errMessage = tr("The audio capturing device supports no suitable audio formats."
            "Please select another audio device or \"none\" in the Screen Recording settings.");

        return false;
    }

    m_audioQueue.setMaxSize(AUDIO_QUEUE_MAX_SIZE);
    return true;
}

bool QnDesktopDataProvider::EncodedAudioInfo::setupPostProcess(
    int frameSize,
    int channels,
    int sampleRate)
{
    NX_MUTEX_LOCKER lock(&m_mtx);
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
        m_speexPreprocess = speex_preprocess_state_init(frameSize * channels, sampleRate);
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
    m_outPacket(av_packet_alloc()),
    m_audioDeviceChangeNotifier(new nx::vms::client::desktop::AudioDeviceChangeNotifier())
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

    using AudioDeviceChangeNotifier = nx::vms::client::desktop::AudioDeviceChangeNotifier;
    const auto stopOnDeviceDisappeared =
        [this](const QString& deviceName)
        {
            const bool deviceInUse = std::any_of(std::cbegin(m_audioInfo), std::cend(m_audioInfo),
                [&deviceName](const auto& audioInfo)
                {
                    return audioInfo->m_audioDevice.name() == deviceName;}
                );

            if (deviceInUse)
                pleaseStop();
        };

    connect(m_audioDeviceChangeNotifier.get(), &AudioDeviceChangeNotifier::deviceUnplugged,
        this, stopOnDeviceDisappeared, Qt::DirectConnection);

    connect(m_audioDeviceChangeNotifier.get(), &AudioDeviceChangeNotifier::deviceNotPresent,
        this, stopOnDeviceDisappeared, Qt::DirectConnection);

    m_needStop = false;
    m_timer = std::make_unique<QElapsedTimer>();
}

QnDesktopDataProvider::~QnDesktopDataProvider()
{
    stop();
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

bool QnDesktopDataProvider::initVideoCapturing()
{
    m_grabber = new QnBufferedScreenGrabber(
        m_desktopNum,
        QnBufferedScreenGrabber::DEFAULT_QUEUE_SIZE,
        QnBufferedScreenGrabber::DEFAULT_FRAME_RATE,
        m_captureMode,
        m_captureCursor,
        m_captureResolution,
        m_widget);
    m_grabber->setLogo(m_logo);
    m_grabber->setTimer(m_timer.get());


    m_videoBufSize = av_image_get_buffer_size((AVPixelFormat) m_grabber->format(), m_grabber->width(), m_grabber->height(), /*align*/ 1);
    m_videoBuf = (quint8*) av_malloc(m_videoBufSize);

    m_frame = av_frame_alloc();
    int ret = av_image_alloc(m_frame->data, m_frame->linesize,
        m_grabber->width(), m_grabber->height(), m_grabber->format(), /*align*/ 1);
    if (ret < 0)
    {
        m_lastErrorStr = tr("Could not detect capturing resolution");
        return false;
    }

    QString videoCodecName;
    if (m_encodeQualuty <= 0.5)
        videoCodecName = getResource()->systemContext()->globalSettings()->lowQualityScreenVideoCodec();
    else
        videoCodecName = getResource()->systemContext()->globalSettings()->defaultExportVideoCodec();

    AVCodec* videoCodec = avcodec_find_encoder_by_name(videoCodecName.toLatin1().data());
    if (!videoCodec)
    {
        NX_WARNING(this, "Configured codec: %1 not found, h263p will used", videoCodecName);
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
    m_videoCodecCtx->bit_rate = calculateBitrate(videoCodec->name);

    QString codec_prop;

    if (m_encodeQualuty == 1)
    {
        m_videoCodecCtx->has_b_frames = 1;
        m_videoCodecCtx->level = 50;
        m_videoCodecCtx->gop_size = 25;
        m_videoCodecCtx->trellis = 2;
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_AC_PRED;
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_4MV;
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

    const auto codecParameters = new CodecParameters(m_videoCodecCtx);
    m_videoContext = CodecParametersConstPtr(codecParameters);

    AvOptions options;
    options.set("motion_est", "epzs", 0);
    if (avcodec_open2(m_videoCodecCtx, videoCodec, options) < 0)
    {
        m_lastErrorStr = tr("Could not initialize video encoder.");
        return false;
    }

    // CodecParameters was created before open() call to avoid encoder specific fields.
    // Transfer extradata manually.
    codecParameters->setExtradata(m_videoCodecCtx->extradata, m_videoCodecCtx->extradata_size);

    return true;
}

bool QnDesktopDataProvider::initAudioCapturing()
{
    m_initTime = AV_NOPTS_VALUE;

    // init audio capture
    if (!m_audioInfo.isEmpty())
    {
        foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        {
            if (!audioChannel->setupFormat(m_lastErrorStr))
                return false;
        }

        const auto& format = m_audioInfo.at(0)->m_audioFormat;
        int sampleRate = format.sampleRate();
        int channels = m_audioInfo.size() > 1 ? /*stereo*/ 2 : format.channelCount();

        if (!m_audioEncoder.initialize(AV_CODEC_ID_MP2,
            sampleRate,
            channels,
            fromQtAudioFormat(format),
            av_get_default_channel_layout(channels),
            /*bitrate*/ 64000 * channels))
        {
            m_lastErrorStr = tr("Could not initialize audio encoder.");
            return false;
        }

        m_frameSize = m_audioEncoder.codecParams()->getFrameSize();
        m_audioFrameDuration = m_frameSize / (double) sampleRate * 1000; // keep in ms

        m_soundAnalyzer = QnVoiceSpectrumAnalyzer::instance();
        m_soundAnalyzer->initialize(sampleRate, channels);

        foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        {
            if (!audioChannel->setupPostProcess(m_frameSize, channels, sampleRate))
            {
                m_lastErrorStr = tr("Could not initialize audio device \"%1\".").arg(audioChannel->m_audioDevice.fullName());
                return false;
            }
        }
    }
    // 50 ms as max jitter
    // Qt uses 25fps timer for audio grabbing, so jitter 40ms + 10ms reserved.
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

    return true;
}

int QnDesktopDataProvider::processData(bool flush)
{
    if (m_videoCodecCtx == 0)
        return -1;
    if (m_frame->width <= 0 || m_frame->height <= 0)
        return -1;

    m_outPacket->data = m_videoBuf;
    m_outPacket->size = m_videoBufSize;
    int got_packet = 0;
    int encodeResult = avcodec_encode_video2(m_videoCodecCtx, m_outPacket, flush ? 0 : m_frame, &got_packet);


    if (encodeResult < 0)
        return encodeResult; //< error

    AVRational timeBaseNative;
    timeBaseNative.num = 1;
    timeBaseNative.den = 1000000;

    if (m_initTime == AV_NOPTS_VALUE)
        m_initTime = qnSyncTime->currentUSecsSinceEpoch();

    if (got_packet > 0)
    {

        QnWritableCompressedVideoDataPtr video = QnWritableCompressedVideoDataPtr(
            new QnWritableCompressedVideoData(m_outPacket->size, m_videoContext));
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
            speex_preprocess(ai->m_speexPreprocess, buffer1, nullptr);

        if (ai2)
        {
            QnWritableCompressedAudioDataPtr audioData2;
            ai2->m_audioQueue.pop(audioData2);
            short* buffer2 = (short*) audioData2->m_data.data();
            if (ai2->m_speexPreprocess)
                speex_preprocess(ai2->m_speexPreprocess, buffer2, nullptr);

            int stereoPacketSize = m_frameSize * 2 * ai->m_audioFormat.sampleSize()/8;
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
        m_soundAnalyzer->processData(buffer1, m_frameSize);

        if (!m_audioEncoder.sendFrame((uint8_t*)buffer1, m_frameSize))
            return;

        QnWritableCompressedAudioDataPtr packet;
        while (!needToStop())
        {
            if (!m_audioEncoder.receivePacket(packet))
                return;

            if (!packet)
                break;

            if (m_initTime == AV_NOPTS_VALUE)
                m_initTime = qnSyncTime->currentUSecsSinceEpoch();

            packet->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            packet->dataProvider = this;
            packet->channelNumber = 1;

            AVRational timeBaseNative;
            timeBaseNative.num = 1;
            timeBaseNative.den = 1000000;

            AVRational timeBaseMs;
            timeBaseMs.num = 1;
            timeBaseMs.den = 1000;

            packet->timestamp = av_rescale_q(audioPts, timeBaseMs, timeBaseNative) + m_initTime;
            putData(packet);
        }
    }
}

void QnDesktopDataProvider::start(Priority priority)
{
    NX_MUTEX_LOCKER lock( &m_startMutex );
    if (m_started)
        return;
    m_started = true;

    m_isAudioInitialized = initAudioCapturing();
    if (!m_isAudioInitialized)
    {
        m_needStop = true;
        NX_WARNING(this, "Could not initialize audio capturing: %1", m_lastErrorStr);
    }

    QnLongRunnable::start(priority);
}

bool QnDesktopDataProvider::isInitialized() const
{
    return m_isAudioInitialized;
}

bool QnDesktopDataProvider::needVideoData() const
{
    const auto dataProcessors = m_dataprocessors.lock();
    for (const auto& consumer: *dataProcessors)
    {
        if (consumer->needConfigureProvider())
            return true;
    }
    return false;
}

void QnDesktopDataProvider::run()
{
    QThread::currentThread()->setPriority(QThread::HighPriority);
    m_timer->restart();

    while (!needToStop() || (m_grabber && m_grabber->dataExist()))
    {
        if (needVideoData())
        {
            if (!m_isVideoInitialized)
            {
                m_isVideoInitialized = initVideoCapturing();
                if (!m_isVideoInitialized)
                {
                    m_needStop = true;
                    break;
                }
            }

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
            if (m_grabber)
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
    if (m_grabber)
        m_grabber->pleaseStop();
}

void QnDesktopDataProvider::closeStream()
{
    delete m_grabber;
    m_grabber = 0;

    QnFfmpegHelper::deleteAvCodecContext(m_videoCodecCtx);
    m_videoCodecCtx = 0;

    if (m_frame) {
        avpicture_free((AVPicture*) m_frame);
        av_free(m_frame);
        m_frame = 0;
    }

    if (m_videoBuf)
        av_free(m_videoBuf);
    m_videoBuf = 0;

    foreach(EncodedAudioInfo* audioChannel, m_audioInfo)
        delete audioChannel;
    m_audioInfo.clear();
}

AudioLayoutConstPtr QnDesktopDataProvider::getAudioLayout()
{
    if (m_audioEncoder.codecParams() && !m_audioLayout)
        m_audioLayout.reset(new AudioLayout(m_audioEncoder.codecParams()));

    return m_audioLayout;
}

qint64 QnDesktopDataProvider::currentTime() const
{
    return m_timer->elapsed();
}

void QnDesktopDataProvider::beforeDestroyDataProvider(QnAbstractMediaDataReceptor* consumer)
{
    NX_MUTEX_LOCKER lock( &m_startMutex );
    removeDataProcessor(consumer);
    if (processorsCount() == 0)
        pleaseStop();
}

void QnDesktopDataProvider::addDataProcessor(QnAbstractMediaDataReceptor* consumer)
{
    NX_MUTEX_LOCKER lock( &m_startMutex );
    if (needToStop()) {
        wait(); // wait previous thread instance
        m_started = false; // now we ready to restart
    }
    QnAbstractMediaStreamDataProvider::addDataProcessor(consumer);
}

bool QnDesktopDataProvider::readyToStop() const
{
    NX_MUTEX_LOCKER lock( &m_startMutex );
    return processorsCount() == 0;
}

/*
void QnDesktopDataProvider::putData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media  = data.dynamicCast<QnAbstractMediaData>();
    if (!media)
        return;

    NX_MUTEX_LOCKER mutex( &m_mutex );
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
