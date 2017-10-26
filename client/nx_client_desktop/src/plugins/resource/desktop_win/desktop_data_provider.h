#pragma once

#include <QtCore/QIODevice>
#include <QtMultimedia/QAudioInput>

#include <nx/utils/thread/long_runnable.h>

#include <nx/streaming/audio_data_packet.h>

#include <ui/screen_recording/qnaudio_device_info.h>

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
    int calculateBitrate();
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
        SpeexPreprocessState* m_speexPreprocess;

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
        QnDesktopDataProvider* m_owner;
        HWAVEIN hWaveIn;
        QQueue<WAVEHDR*> m_buffers;
        QnMutex m_mtx;
        std::atomic<bool> m_terminated;
        bool m_waveInOpened;
    };

    int m_encodedFrames;
    QnBufferedScreenGrabber* m_grabber;
    quint8* m_videoBuf;
    int m_videoBufSize;
    AVCodecContext* m_videoCodecCtx;
    QnConstMediaContextPtr m_videoContext;
    AVFrame* m_frame;
    int m_desktopNum;

    QVector<quint8> m_buffer;

    // single audio objects
    quint8* m_encodedAudioBuf;
    AVCodecContext* m_audioCodecCtx;
    QnConstMediaContextPtr m_audioContext;
    int m_audioFramesCount;
    double m_audioFrameDuration;
    qint64 m_storedAudioPts;
    int m_maxAudioJitter;
    QVector <EncodedAudioInfo*> m_audioInfo;

    Qn::CaptureMode m_captureMode;
    bool m_captureCursor;
    QSize m_captureResolution;
    float m_encodeQualuty;
    QWidget* m_widget;

    bool m_capturingStopped;
    const QPixmap m_logo;
    qint64 m_initTime;
    mutable QnMutex m_startMutex;
    bool m_started;
    bool m_isInitialized;

    QPointer<QnVoiceSpectrumAnalyzer> m_soundAnalyzer;
    AVFrame* m_inputAudioFrame;
    AVPacket* m_outPacket;

    friend void QT_WIN_CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,  DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};
