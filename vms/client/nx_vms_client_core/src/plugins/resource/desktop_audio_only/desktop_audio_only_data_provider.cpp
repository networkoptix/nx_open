// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_data_provider.h"

#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/build_info.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <speex/speex_preprocess.h>
#include <ui/screen_recording/audio_recorder_settings.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_helper.h>

using namespace nx::vms::client::core;

namespace
{
    const auto kSingleChannelBitrate = 64000;
    const auto kStereoChannelCount = 2;
    const auto kBitsInByte = 8;
    const auto kDefaultSampleRate = 44100;
    const auto kDefaultSampleSize = 16;
    const auto kDefaultChannelCount = 1;
    const auto kDefaultCodec = "audio/pcm";
}

struct QnDesktopAudioOnlyDataProvider::AudioSourceInfo
{
    ~AudioSourceInfo();

    QAudioDeviceInfo deviceInfo;
    QAudioFormat format;
    QIODevice* ioDevice = nullptr;
    std::unique_ptr<QAudioInput> input;
    QByteArray buffer;
    char* frameBuffer = nullptr;
    SpeexPreprocessState* speexPreprocessState = nullptr;
};

QnDesktopAudioOnlyDataProvider::AudioSourceInfo::~AudioSourceInfo()
{
    if (input)
        input->stop();
    if (speexPreprocessState)
    {
        speex_preprocess_state_destroy(speexPreprocessState);
        speexPreprocessState = 0;
    }

    if (frameBuffer)
        qFreeAligned(frameBuffer);
}

QnDesktopAudioOnlyDataProvider::QnDesktopAudioOnlyDataProvider(QnResourcePtr ptr) :
    QnDesktopDataProviderBase(ptr),
    m_initialized(false),
    m_stopping(false)
{
}

QnDesktopAudioOnlyDataProvider::~QnDesktopAudioOnlyDataProvider()
{
    directDisconnectAll();
    stop();

}

void QnDesktopAudioOnlyDataProvider::pleaseStop()
{
    m_stopping = true;
    quit();
}

void QnDesktopAudioOnlyDataProvider::resizeBuffers(int frameSize)
{
    for (auto& audioInput: m_audioSourcesInfo)
    {
        auto size =
            frameSize *
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

    const auto& format = m_audioSourcesInfo.at(0)->format;
    int sampleRate = format.sampleRate();
    int channels = m_audioSourcesInfo.size() > 1 ? kStereoChannelCount : format.channelCount();

    if (!m_audioEncoder.initialize(AV_CODEC_ID_MP2,
        sampleRate,
        channels,
        fromQtAudioFormat(format),
        av_get_default_channel_layout(channels),
        kSingleChannelBitrate * channels))
    {
        return;
    }

    m_frameSize = m_audioEncoder.codecParams()->getFrameSize();
    initSpeex(m_frameSize, sampleRate);
    resizeBuffers(m_frameSize);

    m_soundAnalyzer = appContext()->voiceSpectrumAnalyzer();
    m_soundAnalyzer->initialize(sampleRate, channels);

    startInputs();
    m_initialized = true;

    exec();

    directDisconnectAll();
    m_audioSourcesInfo.clear();
}

bool QnDesktopAudioOnlyDataProvider::initSpeex(int frameSize, int sampleRate)
{
    for (auto& info: m_audioSourcesInfo)
    {
        auto speexPreprocessState = speex_preprocess_state_init(frameSize, sampleRate);
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

QAudioFormat QnDesktopAudioOnlyDataProvider::getAppropriateAudioFormat(
    const QAudioDeviceInfo& deviceInfo,
    QString* errorString)
{
    QAudioFormat result;
    result.setSampleRate(kDefaultSampleRate);
    result.setChannelCount(kDefaultChannelCount);
    result.setSampleSize(kDefaultSampleSize);
    result.setCodec(kDefaultCodec);
    result.setByteOrder(QAudioFormat::LittleEndian);
    result.setSampleType(QAudioFormat::SignedInt);

    if (!deviceInfo.isFormatSupported(result))
    {
        qDebug() << "Default format is not supported, trying to use nearest";
        result = deviceInfo.nearestFormat(result);
    }

    if (result.sampleSize() != kDefaultSampleSize
        || result.sampleType() != QAudioFormat::SignedInt)
    {
        if (errorString)
        {
            *errorString = tr("Sample format of input device %1 is not supported.")
                .arg(deviceInfo.deviceName());
        }
        result = QAudioFormat();
    }
    return result;
}

bool QnDesktopAudioOnlyDataProvider::initInputDevices()
{
    QnAudioRecorderSettings settings;
    auto primaryAudioDevice = settings.primaryAudioDevice();
    auto secondaryAudioDevice = nx::build_info::isMobile()
        ? QnAudioDeviceInfo() // It is not allowed to run several devices at mobile platform.
        : settings.secondaryAudioDevice();

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
    for (const auto& source: m_audioSourcesInfo)
    {
        const auto format = getAppropriateAudioFormat(source->deviceInfo, &m_lastErrorStr);
        if (!format.isValid())
            return false;

        source->format = format;
        source->input.reset(new QAudioInput(source->deviceInfo, source->format));
    }

    return true;
}

void QnDesktopAudioOnlyDataProvider::processData()
{
    const int kFrameSizeInBytes = m_frameSize * 2;

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

        m_soundAnalyzer->processData((qint16*)firstBuffer, m_frameSize);

        if (!m_audioEncoder.sendFrame((uint8_t*)firstBuffer, m_frameSize))
            return;

        QnWritableCompressedAudioDataPtr packet;
        while (!needToStop())
        {
            if (!m_audioEncoder.receivePacket(packet))
                return;

            if (!packet)
                break;

            packet->timestamp = qnSyncTime->currentUSecsSinceEpoch(); //< TODO: #lbusygin set an appropriate timestamp.
            packet->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            packet->dataProvider = this;
            packet->channelNumber = 1;
            if (dataCanBeAccepted())
                putData(packet);
        }
    }
}

void QnDesktopAudioOnlyDataProvider::preprocessAudioBuffers(
    std::vector<AudioSourceInfoPtr>& preprocessList)
{
    NX_ASSERT(!preprocessList.empty());

    const auto kSampleSizeInBytes =
        preprocessList.at(0)->format.sampleSize() / kBitsInByte;

    const auto kFrameSizeInBytes = m_frameSize * kSampleSizeInBytes;

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
                nullptr);
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

bool QnDesktopAudioOnlyDataProvider::isInitialized() const
{
    return m_initialized;
}

AudioLayoutConstPtr QnDesktopAudioOnlyDataProvider::getAudioLayout()
{
    if (!m_audioLayout)
        m_audioLayout.reset(new AudioLayout(m_audioEncoder.codecParams()));

    return m_audioLayout;
}

void QnDesktopAudioOnlyDataProvider::beforeDestroyDataProvider(
    QnAbstractMediaDataReceptor* /*dataProviderWrapper*/)
{
}

bool QnDesktopAudioOnlyDataProvider::readyToStop() const
{
    return true;
}
