// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QQueue>

#include <nx/audio/sound.h>
#include <nx/media/audio/format.h>
#include <nx/media/audio_data_packet.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/desktop/camera/audio_decode_mode.h>

class QnVoiceSpectrumAnalyzer;
class QnAbstractAudioDecoder;
class QnCompressedAudioData;

/**
 * On source data end, playCurrentBuffer method MUST be called to avoid media data loss.
 */
class QnAudioStreamDisplay: public QObject
{
    Q_OBJECT

public:
    QnAudioStreamDisplay(
        int bufferMs,
        int prebufferMs,
        nx::vms::client::desktop::AudioDecodeMode decodeMode);
    ~QnAudioStreamDisplay();

    bool putData(QnCompressedAudioDataPtr data, qint64 minTime = 0);
    void enqueueData(QnCompressedAudioDataPtr data, qint64 minTime = 0);
    void suspend();

    void resume();

    /** Removes all data from audio buffers. */
    void clearAudioBuffer();

    /** Clears only device buff, not packets queue. */
    void clearDeviceBuffer();

    /** How many ms is buffered in audio buffers at this moment. (!) */
    int msInBuffer() const;
    qint64 getCurrentTime() const;
    void blockTimeValue(qint64 value);
    void unblockTimeValue();

    /** @return start buffering time or AV_NOPTS_VALUE if audio is not buffering. */
    qint64 startBufferingTime() const;

    /** @return False if format is not supported. */
    bool isFormatSupported() const;

    /** Forcing downmixing, even if output device supports multichannel output. */
    void setForceDownmix(bool value);

    bool isDownmixForced() const { return m_forceDownmix; }

    /** Play current buffer to the end. */
    void playCurrentBuffer();

    int getAudioBufferSize() const;
    bool isPlaying() const;

    QnVoiceSpectrumAnalyzer* analyzer() const;
    nx::vms::client::desktop::AudioDecodeMode decodeMode() const;

private:
    int msInQueue() const;

    bool initFormatConvertRule(nx::media::audio::Format format);
    bool createDecoder(const QnCompressedAudioDataPtr& data);
private:
    enum class SampleConvertMethod { none, float2Int32, float2Int16, int32ToInt16 };

    nx::Mutex m_guiSync;
    QMap<AVCodecID, QnAbstractAudioDecoder*> m_decoders;

    int m_bufferMs;
    int m_prebufferMs;
    bool m_tooFewDataDetected;
    bool m_isFormatSupported;
    std::unique_ptr<nx::audio::Sound> m_sound;
    std::unique_ptr<QnVoiceSpectrumAnalyzer> m_analyzer;
    nx::vms::client::desktop::AudioDecodeMode m_decodeMode =
        nx::vms::client::desktop::AudioDecodeMode::normal;

    bool m_downmixing; //< Do downmixing.
    bool m_forceDownmix; //< Force downmixing, even if output device supports multichannel.
    SampleConvertMethod m_sampleConvertMethod;
    bool m_isConvertMethodInitialized;

    QQueue<QnCompressedAudioDataPtr> m_audioQueue;
    nx::utils::ByteArray m_decodedAudioBuffer;
    qint64 m_startBufferingTime;
    qint64 m_lastAudioTime;
    mutable nx::Mutex m_audioQueueMutex;
    qint64 m_blockedTimeValue;
    bool m_suspended = false;
};
