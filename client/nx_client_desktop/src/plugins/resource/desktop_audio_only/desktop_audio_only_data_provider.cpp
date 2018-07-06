#include "desktop_audio_only_data_provider.h"

#include <dsp_effects/speex/speex_preprocess.h>
#include <utils/media/ffmpeg_helper.h>
#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/streaming/media_context.h>
#include <nx/streaming/config.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace
{
    const auto kStereoChannelCount = 2;
    const auto kBitsInByte = 8;
    const auto kDefaultSampleRate = 44100;
    const auto kDefaultSampleSize = 16;
    const auto kDefaultChannelCount = 1;
    const auto kDefaultCodec = lit("audio/pcm");
    const auto kEncoderCodecName = lit("mp2"); // libmp3lame
    const auto kSingleChannelBitrate = 64000;
}

QnDesktopAudioOnlyDataProvider::QnDesktopAudioOnlyDataProvider(QnResourcePtr ptr) :
    QnDesktopDataProviderBase(ptr),
    m_initialized(false),
    m_stopping(false),
    m_encoderBuffer(nullptr),
    m_inputFrame(av_frame_alloc())
{
}

QnDesktopAudioOnlyDataProvider::~QnDesktopAudioOnlyDataProvider()
{
    directDisconnectAll();
    stop();

    if (m_encoderBuffer)
        av_free(m_encoderBuffer);

    av_frame_free(&m_inputFrame);
}

void QnDesktopAudioOnlyDataProvider::pleaseStop()
{
    m_stopping = true;
    quit();
}

quint32 QnDesktopAudioOnlyDataProvider::getFrameSize()
{
    return m_encoderCtxPtr->getFrameSize();
}

quint32 QnDesktopAudioOnlyDataProvider::getSampleRate()
{
    return m_encoderCtxPtr->getSampleRate();
}

void QnDesktopAudioOnlyDataProvider::resizeBuffers()
{
    for (auto& audioInput: m_audioSourcesInfo)
    {
        auto size =
            getFrameSize() *
            audioInput->format.sampleSize() / kBitsInByte *
            kStereoChannelCount;

        audioInput->frameBuffer =
            static_cast<char*>(qMallocAligned(size, CL_MEDIA_ALIGNMENT));
    }
}

void QnDesktopAudioOnlyDataProvider::startInputs()
{
    for(auto& info: m_audioSourcesInfo)
    {
        info->ioDevice =
            info->input->start();
    }

    auto primaryIODevice =
        m_audioSourcesInfo.at(0)->ioDevice;

    Qn::directConnect(
        primaryIODevice,
        &QIODevice::readyRead,
        this,
        &QnDesktopAudioOnlyDataProvider::processData);
}

void QnDesktopAudioOnlyDataProvider::run()
{

    if (!initInputDevices())
        return;

    if (!initAudioEncoder())
        return;

    initSpeex();

    resizeBuffers();

    m_soundAnalyzer = QnVoiceSpectrumAnalyzer::instance();
    m_soundAnalyzer->initialize(
        getSampleRate(),
        m_encoderCtxPtr->getChannels());

    startInputs();
    m_initialized = true;

    exec();

    directDisconnectAll();
    m_audioSourcesInfo.clear();
}


bool QnDesktopAudioOnlyDataProvider::initSpeex()
{
    for (auto& info: m_audioSourcesInfo)
    {
        auto speexPreprocessState = speex_preprocess_state_init(
            getFrameSize(),
            m_encoderCtxPtr->getSampleRate());

        int denoiseEnabled = 1;
        int agcEnabled = 1;
        float agcLevel = 16000;

        speex_preprocess_ctl(speexPreprocessState, SPEEX_PREPROCESS_SET_DENOISE, &denoiseEnabled);
        speex_preprocess_ctl(speexPreprocessState, SPEEX_PREPROCESS_SET_AGC, &agcEnabled);
        speex_preprocess_ctl(speexPreprocessState, SPEEX_PREPROCESS_SET_AGC_LEVEL, &agcLevel);

        info->speexPreprocessState = speexPreprocessState;
    }

    return true;
}

bool QnDesktopAudioOnlyDataProvider::initAudioEncoder()
{
    NX_ASSERT(!m_audioSourcesInfo.empty());

    m_encoderBuffer = (uint8_t*) av_malloc(FF_MIN_BUFFER_SIZE);

    AVCodec* audioCodec = avcodec_find_encoder_by_name(
        kEncoderCodecName.toLatin1().constData());

    if(audioCodec == 0)
    {
        m_lastErrorStr = tr("Could not find audio encoder \"%1\".")
            .arg(kEncoderCodecName);
        return false;
    }

    auto encoderCtx = avcodec_alloc_context3(audioCodec);
    const auto format = m_audioSourcesInfo.at(0)->format;

    encoderCtx->codec_id = audioCodec->id;
    encoderCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    encoderCtx->sample_fmt =
        QnFfmpegHelper::fromQtAudioFormatToFfmpegSampleType(
            fromQtAudioFormat(format));

    encoderCtx->channels = m_audioSourcesInfo.size() > 1 ?
        kStereoChannelCount : format.channelCount();

    encoderCtx->sample_rate = format.sampleRate();

    AVRational timeBase;
        timeBase.num = 1;
        timeBase.den = encoderCtx->sample_rate;

    encoderCtx->time_base = timeBase;
    encoderCtx->bit_rate = kSingleChannelBitrate * encoderCtx->channels;

    m_encoderCtxPtr.reset(new QnAvCodecMediaContext(encoderCtx));
    if (avcodec_open2(m_encoderCtxPtr->getAvCodecContext(), audioCodec, nullptr) < 0)
    {
        m_lastErrorStr = tr("Could not initialize audio encoder.");
        return false;
    }

    return true;
}

bool QnDesktopAudioOnlyDataProvider::initInputDevices()
{
    QnVideoRecorderSettings settings;
    auto primaryAudioDevice = settings.primaryAudioDevice();
    auto secondaryAudioDevice = settings.secondaryAudioDevice();

    if (primaryAudioDevice.isNull())
    {
        if (secondaryAudioDevice.isNull())
        {
            m_lastErrorStr = tr("Primary audio device is not selected.");
            return false;
        }

        primaryAudioDevice = secondaryAudioDevice;
        secondaryAudioDevice = QnAudioDeviceInfo();
    }

    AudioSourceInfoPtr sourceInfo(new AudioSourceInfo());

    sourceInfo->deviceInfo = static_cast<QAudioDeviceInfo>(primaryAudioDevice);
    m_audioSourcesInfo.push_back(sourceInfo);

    if (!secondaryAudioDevice.isNull())
    {
        sourceInfo.reset(new AudioSourceInfo());
        sourceInfo->deviceInfo = static_cast<QAudioDeviceInfo>(secondaryAudioDevice);
        m_audioSourcesInfo.push_back(sourceInfo);
    }



    for (auto& source: m_audioSourcesInfo)
    {
        QAudioFormat format;

        format.setSampleRate(kDefaultSampleRate);
        format.setChannelCount(kDefaultChannelCount);
        format.setSampleSize(kDefaultSampleSize);
        format.setCodec(kDefaultCodec);
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        if (!source->deviceInfo.isFormatSupported(format))
        {
            qDebug() << "Default format is not supported, trying to use nearest";
            format = source->deviceInfo.nearestFormat(format);
        }

        if (format.sampleSize() != kDefaultSampleSize ||
            format.sampleType() != QAudioFormat::SignedInt)
        {
            qDebug() << "Wrong sample format";
            m_lastErrorStr = tr("Sample format of input device %1 is not supported.")
                .arg(source->deviceInfo.deviceName());
            return false;
        }

        source->format = format;
        source->input.reset(new QAudioInput(
            source->deviceInfo,
            source->format));
    }

    return true;
}


void QnDesktopAudioOnlyDataProvider::processData()
{
    const size_t kFrameSizeInBytes = getFrameSize() * 2;

    for (auto& info: m_audioSourcesInfo)
    {
        auto samples = info->ioDevice->readAll();
        info->buffer.append(samples);
    }

    while (!m_stopping)
    {
        for(auto& info: m_audioSourcesInfo)
        {
            if (info->buffer.size() < kFrameSizeInBytes)
                return;
        }

        preprocessAudioBuffers(m_audioSourcesInfo);
        auto firstBuffer = m_audioSourcesInfo.at(0)->frameBuffer;
        analyzeSpectrum(firstBuffer);
        auto packet = encodePacket(firstBuffer, static_cast<int>(kFrameSizeInBytes));
        if(packet && dataCanBeAccepted())
            putData(packet);
    }
}

void QnDesktopAudioOnlyDataProvider::preprocessAudioBuffers(
    std::vector<AudioSourceInfoPtr>& preprocessList)
{
    NX_ASSERT(!preprocessList.empty());

    const auto kSampleSizeInBytes =
        preprocessList.at(0)->format.sampleSize() / kBitsInByte;

    const auto kFrameSizeInBytes =
        getFrameSize() * kSampleSizeInBytes;

    const qint16 kStereoPacketSizeInBytes =
        kFrameSizeInBytes * kStereoChannelCount;

    m_internalBuffer.resize(kStereoPacketSizeInBytes);

    for( auto& preprocessItem: preprocessList)
    {
        NX_ASSERT(preprocessItem->buffer.size() >= static_cast<int>(kFrameSizeInBytes));

        auto frameBufferPtr = preprocessItem->frameBuffer;
        memcpy(
            frameBufferPtr,
            preprocessItem->buffer.constData(),
            kFrameSizeInBytes);

        preprocessItem->buffer.remove(0, kFrameSizeInBytes);

        if (preprocessItem->speexPreprocessState)
        {
            speex_preprocess(
                preprocessItem->speexPreprocessState,
                (short*) frameBufferPtr,
                NULL);
        }
    }

    if (preprocessList.size() == 2)
    {
        for (auto& preprocessItem: preprocessList)
        {
            auto frameBufferPtr = preprocessItem->frameBuffer;

            if(preprocessItem->format.channelCount() == 1)
            {
                monoToStereo(
                    (qint16*) m_internalBuffer.data(),
                    (qint16*) frameBufferPtr,
                    kFrameSizeInBytes / kSampleSizeInBytes );

                memcpy(
                    frameBufferPtr,
                    m_internalBuffer.constData(),
                    kStereoPacketSizeInBytes);
            }
        }

        stereoAudioMux(
            (qint16*)preprocessList.at(0)->frameBuffer,
            (qint16*)preprocessList.at(1)->frameBuffer,
            kStereoPacketSizeInBytes / kSampleSizeInBytes);
    }

}

void QnDesktopAudioOnlyDataProvider::analyzeSpectrum(char *buffer)
{
    m_soundAnalyzer->processData((qint16*)buffer, getFrameSize());
}

QnWritableCompressedAudioDataPtr QnDesktopAudioOnlyDataProvider::encodePacket(char *buffer, int inputFrameSize)
{
    Q_UNUSED(inputFrameSize);
    QnWritableCompressedAudioDataPtr audio(nullptr);
    auto ctx = m_encoderCtxPtr->getAvCodecContext();

    /*
    auto encoderBuffer = m_encoderBuffer;
    auto bytesEncoded = avcodec_encode_audio(
        ctx,
        encoderBuffer,
        FF_MIN_BUFFER_SIZE,
        (const short*) buffer);
    */

    m_inputFrame->data[0] = (quint8*)buffer;
    m_inputFrame->nb_samples = ctx->frame_size;

    QnFfmpegAvPacket outputPacket(m_encoderBuffer, FF_MIN_BUFFER_SIZE);
    int got_packet = 0;
    if (avcodec_encode_audio2(ctx, &outputPacket, m_inputFrame, &got_packet) < 0)
        return audio;

    if (got_packet)
    {
        AVRational timeBaseNative;
        timeBaseNative.num = 1;
        timeBaseNative.den = 1000000;

        AVRational timeBaseMs;
        timeBaseMs.num = 1;
        timeBaseMs.den = 1000;

        audio.reset(new QnWritableCompressedAudioData(
            CL_MEDIA_ALIGNMENT,
            outputPacket.size,
            m_encoderCtxPtr));

        audio->m_data.write((char*) m_encoderBuffer, outputPacket.size);
        audio->compressionType = ctx->codec_id;
        audio->timestamp = QDateTime::currentMSecsSinceEpoch();

        audio->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        audio->dataProvider = this;
        audio->channelNumber = 1;
    }

    return audio;
}

QnAudioFormat QnDesktopAudioOnlyDataProvider::fromQtAudioFormat(const QAudioFormat &format) const
{
    QnAudioFormat wrapper;
    wrapper.setSampleRate(format.sampleRate());
    wrapper.setChannelCount(format.channelCount());
    wrapper.setSampleSize(format.sampleSize());
    wrapper.setCodec(format.codec());

    auto endianess = format.byteOrder() == QAudioFormat::BigEndian ?
        QnAudioFormat::BigEndian : QnAudioFormat::LittleEndian;
    wrapper.setByteOrder(endianess);

    auto origSampleType = format.sampleType();
    auto sampleType =
        origSampleType == QAudioFormat::SignedInt ?
            QnAudioFormat::SignedInt :
        origSampleType == QAudioFormat::UnSignedInt ?
            QnAudioFormat::UnSignedInt :
        origSampleType == QAudioFormat::Float ?
            QnAudioFormat::Float :
            QnAudioFormat::Unknown;

    wrapper.setSampleType(sampleType);

    return wrapper;
}

bool QnDesktopAudioOnlyDataProvider::isInitialized() const
{
    return m_initialized;
}

QnConstResourceAudioLayoutPtr QnDesktopAudioOnlyDataProvider::getAudioLayout()
{
    auto ctx = m_encoderCtxPtr->getAvCodecContext();

    if (!m_audioLayout)
        m_audioLayout.reset(new QnDesktopAudioLayout() );
    if (ctx && m_audioLayout->channelCount() == 0)
    {
        QnResourceAudioLayout::AudioTrack track;
        track.codecContext.reset(new QnAvCodecMediaContext(ctx));
        m_audioLayout->setAudioTrackInfo(track);
    }
    return m_audioLayout;
}

void QnDesktopAudioOnlyDataProvider::beforeDestroyDataProvider(
    QnAbstractDataConsumer* /*dataProviderWrapper*/)
{
}

bool QnDesktopAudioOnlyDataProvider::readyToStop() const
{
    return true;
}
