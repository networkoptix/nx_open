#pragma once

#include <QtCore/QIODevice>
#include <QtMultimedia/QAudioInput>

#include <nx/utils/thread/long_runnable.h>

#include <nx/streaming/audio_data_packet.h>

#include <dsp_effects/speex/speex_preprocess.h>

#include "screen_grabber.h"
#include "buffered_screen_grabber.h"
#include <utils/media/voice_spectrum_analyzer.h>
#include <plugins/resource/desktop_camera/desktop_data_provider_base.h>

class CaptureAudioStream;
class QnAbstractDataConsumer;

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

    virtual void beforeDestroyDataProvider(QnAbstractDataConsumer* dataProviderWrapper) override;
    void addDataProcessor(QnAbstractDataConsumer* consumer);

    virtual bool isInitialized() const override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    bool readyToStop() const;

protected:
    virtual void run() override;
private:
    friend class CaptureAudioStream;

    bool init();
    virtual void closeStream();

    qint64 currentTime() const;
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
        QnAudioFormat m_audioFormat;
        QnSafeQueue<QnWritableCompressedAudioDataPtr>  m_audioQueue;
        QnWritableCompressedAudioData m_tmpAudioBuffer;
        SpeexPreprocessState* m_speexPreprocess = nullptr;

        int audioPacketSize();
        bool setupFormat(QString& errMessage);
        bool setupPostProcess();
        qint64 currentTime() const { return m_owner->m_grabber->currentTime(); }
        bool start();
        void stop();
        int nameToWaveIndex();
        bool addBuffer();
        void gotData();
        void clearBuffers();
    private:
        QnDesktopDataProvider* const m_owner;
        HWAVEIN hWaveIn = 0;
        QQueue<WAVEHDR*> m_buffers;
        QnMutex m_mtx;
        std::atomic<bool> m_terminated = false;
        bool m_waveInOpened = false;
    };

    int m_encodedFrames = 0;
    QnBufferedScreenGrabber* m_grabber = nullptr;
    quint8* m_videoBuf = nullptr;
    int m_videoBufSize = 0;
    AVCodecContext* m_videoCodecCtx = nullptr;
    QnConstMediaContextPtr m_videoContext;
    AVFrame* m_frame = nullptr;
    const int m_desktopNum;

    QVector<quint8> m_buffer;

    // single audio objects
    quint8* m_encodedAudioBuf = nullptr;
    AVCodecContext* m_audioCodecCtx = nullptr;
    QnConstMediaContextPtr m_audioContext;
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
    mutable QnMutex m_startMutex;
    bool m_started = false;
    bool m_isInitialized = false;

    QPointer<QnVoiceSpectrumAnalyzer> m_soundAnalyzer;
    AVFrame* m_inputAudioFrame = nullptr;
    AVPacket* m_outPacket = nullptr;

    friend void QT_WIN_CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,
        DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};
