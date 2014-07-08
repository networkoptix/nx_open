#ifndef QN_DESKTOP_FILE_ENCODER_H
#define QN_DESKTOP_FILE_ENCODER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtCore/QIODevice>
#include <QtMultimedia/QAudioInput>

#include <dsp_effects/speex/speex_preprocess.h>
#include "core/datapacket/audio_data_packet.h"
#include "utils/common/long_runnable.h"
#include "screen_grabber.h"
#include "buffered_screen_grabber.h"
#include "ui/screen_recording/qnaudio_device_info.h"
#include "ui/screen_recording/video_recorder_settings.h"

class CaptureAudioStream;

class QnDesktopFileEncoder: public QnLongRunnable {
    Q_OBJECT

public:
    QnDesktopFileEncoder( const QString& fileName,
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
    virtual ~QnDesktopFileEncoder();
    bool start();
    void stop();
    QString fileName() const { return m_fileName; }

    QString lastErrorStr() const { return m_lastErrorStr; }
protected:
    // QnLongRunnable runable
    virtual void run();
private:
    bool init();
private:
    friend class CaptureAudioStream;

    virtual void closeStream();

    int initIOContext();
    qint32 writePacketImpl(quint8* buf, qint32 bufSize);
    qint32 readPacketImpl(quint8* buf, quint32 bufSize);
    qint64 seekPacketImpl(qint64 offset, qint32 whence);
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
        EncodedAudioInfo(QnDesktopFileEncoder* owner);
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
        QnDesktopFileEncoder* m_owner;
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
    bool m_initialized;
    AVFrame* m_frame;
    int m_desktopNum;

    AVFormatContext* m_formatCtx;
    AVOutputFormat * m_outputCtx;

    AVStream * m_videoOutStream;

    int m_videoStreamIndex;

    QString m_fileName;

    QVector<quint8> m_buffer;
    AVIOContext* m_iocontext;
    QIODevice * m_device;

    // single audio objects
    quint8* m_encodedAudioBuf;
    AVCodecContext* m_audioCodecCtx;
    AVStream * m_audioOutStream;
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
    bool m_videoPacketWrited;
    QString m_lastErrorStr;
    bool m_capturingStopped;
    const QPixmap m_logo;
    friend void QT_WIN_CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,  DWORD_PTR dwParam1, DWORD_PTR dwParam2);
};

#endif // Q_OS_WIN

#endif //QN_DESKTOP_FILE_ENCODER_H
