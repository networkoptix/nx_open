// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_data_provider.h"

#include <algorithm>

#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSource>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <speex/speex_preprocess.h>

#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/audio/audio.h>
#include <nx/build_info.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/audio_recording_settings.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

namespace {

const auto kSingleChannelBitrate = 64000;
const auto kStereoChannelCount = 2;
const auto kDefaultSampleRate = 44100;
const auto kDefaultChannelCount = 1;

} // namespace

struct DesktopAudioOnlyDataProvider::AudioSourceInfo
{
    ~AudioSourceInfo();

    QAudioDevice deviceInfo;
    QAudioFormat format;
    QIODevice* ioDevice = nullptr;
    std::unique_ptr<QAudioSource> input;
    QByteArray buffer;
    char* frameBuffer = nullptr;
    SpeexPreprocessState* speexPreprocessState = nullptr;
};

DesktopAudioOnlyDataProvider::AudioSourceInfo::~AudioSourceInfo()
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

DesktopAudioOnlyDataProvider::DesktopAudioOnlyDataProvider(QnResourcePtr ptr):
    DesktopDataProviderBase(ptr)
{
}

DesktopAudioOnlyDataProvider::~DesktopAudioOnlyDataProvider()
{
    directDisconnectAll();
    stop();

}

void DesktopAudioOnlyDataProvider::pleaseStop()
{
    m_stopping = true;
    quit();
}

void DesktopAudioOnlyDataProvider::resizeBuffers(int frameSize)
{
    for (auto& audioInput: m_audioSourcesInfo)
    {
        auto size =
            frameSize *
            audioInput->format.bytesPerSample() *
            kStereoChannelCount;

        audioInput->frameBuffer =
            static_cast<char*>(qMallocAligned(size, CL_MEDIA_ALIGNMENT));
    }
}

void DesktopAudioOnlyDataProvider::startInputs()
{
    for(auto& info: m_audioSourcesInfo)
        info->ioDevice = info->input->start();

    auto primaryIODevice = m_audioSourcesInfo.at(0)->ioDevice;

    Qn::directConnect(
        primaryIODevice,
        &QIODevice::readyRead,
        this,
        &DesktopAudioOnlyDataProvider::processData);
}

void DesktopAudioOnlyDataProvider::run()
{
    if (!initInputDevices())
        return;

    const auto& format = m_audioSourcesInfo.at(0)->format;
    int sampleRate = format.sampleRate();
    int channels = m_audioSourcesInfo.size() > 1 ? kStereoChannelCount : format.channelCount();

    AVChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    if (!m_audioEncoder.initialize(AV_CODEC_ID_MP2,
        sampleRate,
        fromQtAudioFormat(format),
        layout,
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

    // Re-setup audio session since QAudioInput initialization may cause invalid changes.
    nx::audio::setupAudio();

    exec();

    directDisconnectAll();
    m_audioSourcesInfo.clear();
}

bool DesktopAudioOnlyDataProvider::initSpeex(int frameSize, int sampleRate)
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

QAudioFormat DesktopAudioOnlyDataProvider::getAppropriateAudioFormat(
    const QAudioDevice& deviceInfo,
    QString* errorString)
{
    QAudioFormat result;
    result.setSampleRate(kDefaultSampleRate);
    result.setChannelCount(kDefaultChannelCount);
    result.setSampleFormat(QAudioFormat::Int16);

    if (!deviceInfo.isFormatSupported(result))
    {
        qDebug() << "Default format is not supported, trying to use nearest";

        if (!deviceInfo.supportedSampleFormats().contains(result.sampleFormat()))
        {
            if (errorString)
            {
                *errorString = tr("Sample format of input device %1 is not supported.")
                    .arg(deviceInfo.description());
            }

            return {};
        }

        // Set the nearest supported values.

        result.setChannelCount(std::clamp(
            result.channelCount(),
            deviceInfo.minimumChannelCount(),
            deviceInfo.maximumChannelCount()));

        result.setSampleRate(std::clamp(
            result.sampleRate(),
            deviceInfo.minimumSampleRate(),
            deviceInfo.maximumSampleRate()));
    }

    return result;
}

bool DesktopAudioOnlyDataProvider::initInputDevices()
{
    AudioRecordingSettings settings;
    auto primaryAudioDevice = settings.primaryAudioDevice();
    auto secondaryAudioDevice = nx::build_info::isMobile()
        ? AudioDeviceInfo() // It is not allowed to run several devices at mobile platform.
        : settings.secondaryAudioDevice();

    if (primaryAudioDevice.isNull())
    {
        if (secondaryAudioDevice.isNull())
        {
            m_lastErrorStr = tr("Primary audio device is not selected.");
            return false;
        }

        primaryAudioDevice = secondaryAudioDevice;
        secondaryAudioDevice = AudioDeviceInfo();
    }

    AudioSourceInfoPtr sourceInfo(new AudioSourceInfo());

    sourceInfo->deviceInfo = static_cast<QAudioDevice>(primaryAudioDevice);
    m_audioSourcesInfo.push_back(sourceInfo);

    if (!secondaryAudioDevice.isNull())
    {
        sourceInfo.reset(new AudioSourceInfo());
        sourceInfo->deviceInfo = static_cast<QAudioDevice>(secondaryAudioDevice);
        m_audioSourcesInfo.push_back(sourceInfo);
    }
    for (const auto& source: m_audioSourcesInfo)
    {
        const auto format = getAppropriateAudioFormat(source->deviceInfo, &m_lastErrorStr);
        if (!format.isValid())
            return false;

        source->format = format;
        source->input.reset(new QAudioSource(source->deviceInfo, source->format));
    }

    return true;
}

void DesktopAudioOnlyDataProvider::processData()
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

void DesktopAudioOnlyDataProvider::preprocessAudioBuffers(
    std::vector<AudioSourceInfoPtr>& preprocessList)
{
    NX_ASSERT(!preprocessList.empty());

    const auto kSampleSizeInBytes =
        preprocessList.at(0)->format.bytesPerSample();

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

bool DesktopAudioOnlyDataProvider::isInitialized() const
{
    return m_initialized;
}

AudioLayoutConstPtr DesktopAudioOnlyDataProvider::getAudioLayout()
{
    if (!m_audioLayout)
        m_audioLayout.reset(new AudioLayout(m_audioEncoder.codecParams()));

    return m_audioLayout;
}

void DesktopAudioOnlyDataProvider::beforeDestroyDataProvider(
    QnAbstractMediaDataReceptor* /*dataProviderWrapper*/)
{
}

bool DesktopAudioOnlyDataProvider::readyToStop() const
{
    return true;
}

} // namespace nx::vms::client::core
