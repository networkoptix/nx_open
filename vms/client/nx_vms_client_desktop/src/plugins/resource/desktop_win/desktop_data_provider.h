// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIODevice>
#include <QtMultimedia/QAudioInput>
#include <QtMultimedia/QAudioFormat>

#include <nx/utils/thread/long_runnable.h>

#include <nx/media/ffmpeg/audio_encoder.h>
#include <nx/streaming/audio_data_packet.h>

#include <speex/speex_preprocess.h>

#include <utils/media/voice_spectrum_analyzer.h>
#include <plugins/resource/desktop_camera/desktop_data_provider_base.h>

#include "screen_grabber.h"
#include "buffered_screen_grabber.h"

class CaptureAudioStream;
class QnAbstractDataConsumer;
namespace nx::vms::client::desktop { class AudioDeviceChangeNotifier; }

class QnDesktopDataProvider: public QnDesktopDataProviderBase
{
    Q_OBJECT

public:
    QnDesktopDataProvider(QnResourcePtr res,
                        int desktopNum,           // = 0,
                        const QnAudioDeviceInfo* audioDevice,
                        const QnAudioDeviceInfo* audioDevice2,
                        Qn::CaptureMode mode,
                        bool captureCursor,
                        const QSize& captureResolution,
                        float encodeQualuty, // in range 0.0 .. 1.0
                        QWidget* glWidget,  // used in application capture mode only
                        const QPixmap& logo       // logo over video
                       );
    virtual ~QnDesktopDataProvider();

    virtual void start(Priority priority = InheritPriority) override;

    virtual void beforeDestroyDataProvider(QnAbstractMediaDataReceptor* dataProviderWrapper) override;
    void addDataProcessor(QnAbstractMediaDataReceptor* consumer);

    virtual bool isInitialized() const override;

    virtual AudioLayoutConstPtr getAudioLayout() override;

    bool readyToStop() const;

    qint64 currentTime() const;
protected:
    virtual void run() override;
private:
    friend class CaptureAudioStream;

    bool initVideoCapturing();
    bool initAudioCapturing();
    virtual void closeStream();

    int calculateBitrate(const char* codecName = nullptr);
    int processData(bool flush);
    void putAudioData();
    void stopCapturing();
    SpeexPreprocessState* createSpeexPreprocess();
    bool needVideoData() const;
private:
    class EncodedAudioInfo
    {
    public:
        static const int AUDIO_BUFFERS_COUNT = 2;
        EncodedAudioInfo(QnDesktopDataProvider* owner);
        ~EncodedAudioInfo();
        // doubled audio objects
        QnAudioDeviceInfo m_audioDevice;
        //QString m_audioDeviceName;
        QAudioFormat m_audioFormat;
        QnSafeQueue<QnWritableCompressedAudioDataPtr>  m_audioQueue;
        QnWritableCompressedAudioData m_tmpAudioBuffer;
        SpeexPreprocessState* m_speexPreprocess = nullptr;

        int audioPacketSize();
        bool setupFormat(QString& errMessage);
        bool setupPostProcess(int frameSize, int channels, int sampleRate);
        qint64 currentTime() const { return m_owner->currentTime(); }
        bool start();
        void stop();
        int nameToWaveIndex();
        bool addBuffer();
        void gotAudioPacket(const char* data, int packetSize);
        void gotData();
        void clearBuffers();
    private:
        QnDesktopDataProvider* const m_owner;
        HWAVEIN hWaveIn = 0;
        QQueue<WAVEHDR*> m_buffers;
        nx::Mutex m_mtx;
        std::atomic<bool> m_terminated = false;
        bool m_waveInOpened = false;
        std::vector<char> m_intermediateAudioBuffer;
    };

    int m_encodedFrames = 0;
    QnBufferedScreenGrabber* m_grabber = nullptr;
    quint8* m_videoBuf = nullptr;
    int m_videoBufSize = 0;
    AVCodecContext* m_videoCodecCtx = nullptr;
    CodecParametersConstPtr m_videoContext;
    AVFrame* m_frame = nullptr;
    const int m_desktopNum;

    QVector<quint8> m_buffer;

    // single audio objects
    int m_audioFramesCount = 0;
    double m_audioFrameDuration = 0.0;
    qint64 m_storedAudioPts = 0;
    int m_maxAudioJitter = 0;
    QVector<EncodedAudioInfo*> m_audioInfo;

    const Qn::CaptureMode m_captureMode;
    const bool m_captureCursor;
    const QSize m_captureResolution;
    const float m_encodeQualuty;
    QWidget* const m_widget;

    bool m_capturingStopped = false;
    const QPixmap m_logo;
    qint64 m_initTime;
    mutable nx::Mutex m_startMutex;
    bool m_started = false;
    bool m_isAudioInitialized = false;
    bool m_isVideoInitialized = false;

    QPointer<QnVoiceSpectrumAnalyzer> m_soundAnalyzer;
    AVPacket* m_outPacket = nullptr;
    nx::media::ffmpeg::AudioEncoder m_audioEncoder;
    int m_frameSize = 0;
    std::unique_ptr<QElapsedTimer> m_timer;
    std::unique_ptr<nx::vms::client::desktop::AudioDeviceChangeNotifier> m_audioDeviceChangeNotifier;

    friend void QT_WIN_CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,
        DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};
