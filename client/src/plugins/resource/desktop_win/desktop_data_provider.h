#ifndef QN_DESKTOP_DATA_PROVIDER_H
#define QN_DESKTOP_DATA_PROVIDER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtCore/QIODevice>
#include <QtMultimedia/QAudioInput>

#include <utils/common/long_runnable.h>

#include <core/datapacket/audio_data_packet.h>
#include <core/dataprovider/live_stream_provider.h>

#include <ui/screen_recording/qnaudio_device_info.h>

#include <dsp_effects/speex/speex_preprocess.h>

#include "screen_grabber.h"
#include "buffered_screen_grabber.h"

class CaptureAudioStream;
class QnAbstractDataConsumer;

class QnDesktopDataProvider: public QnAbstractMediaStreamDataProvider
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
    QString lastErrorStr() const { return m_lastErrorStr; }

    virtual void start(Priority priority = InheritPriority) override;

    void beforeDestroyDataProvider(QnAbstractDataConsumer* dataProviderWrapper);

    bool isInitialized() const;

    QnConstResourceAudioLayoutPtr getAudioLayout();

    bool readyToStop() const;

protected:
    virtual void run() override;

private:
    bool init();

private:
    friend class CaptureAudioStream;

    virtual void closeStream();

    qint64 currentTime() const;
    int calculateBitrate();
    int processData(bool flush);
    void stopCapturing();
    SpeexPreprocessState* createSpeexPreprocess();

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
        CLThreadQueue<QnWritableCompressedAudioDataPtr>  m_audioQueue;
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
        QMutex m_mtx;
        bool m_terminated;
    };

    int m_encodedFrames;
    QnBufferedScreenGrabber* m_grabber;
    quint8* m_videoBuf;
    int m_videoBufSize;
    AVCodecContext* m_videoCodecCtx;
    QnMediaContextPtr m_videoCodecCtxPtr;
    AVFrame* m_frame;
    int m_desktopNum;

    QVector<quint8> m_buffer;

    // single audio objects
    quint8* m_encodedAudioBuf;
    AVCodecContext* m_audioCodecCtx;
    QnMediaContextPtr m_audioCodecCtxPtr;
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
    QString m_lastErrorStr;
    bool m_capturingStopped;
    const QPixmap m_logo;
    //QSet<void*> m_needKeyData;
    qint64 m_initTime;
    mutable QMutex m_startMutex;
    bool m_started;
    bool m_isInitialized;

    class QnDesktopAudioLayout;
    QSharedPointer<QnDesktopAudioLayout> m_audioLayout;

    friend void QT_WIN_CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,  DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};

#endif // Q_OS_WIN

#endif //QN_DESKTOP_DATA_PROVIDER_H
