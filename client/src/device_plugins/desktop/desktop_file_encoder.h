#ifndef __DESKTOP_H264_STREAM_READER_H
#define __DESKTOP_H264_STREAM_READER_H

#include <QIODevice>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <windows.h>
#include <mmsystem.h>


#include <dsp_effects/speex/speex_preprocess.h>
#include "core/datapacket/mediadatapacket.h"
#include "utils/common/longrunnable.h"
#include "screen_grabber.h"
#include "buffered_screen_grabber.h"

class CaptureAudioStream;

class DesktopFileEncoder: public CLLongRunnable
{
    Q_OBJECT

private:
    enum {BLOCK_SIZE = 1460};
public:
    DesktopFileEncoder( const QString& fileName,
                        int desktopNum,           // = 0,
                        const QAudioDeviceInfo* audioDevice,
                        const QAudioDeviceInfo* audioDevice2,
                        CLScreenGrabber::CaptureMode mode,
                        bool captureCursor,
                        const QSize& captureResolution,
                        float encodeQualuty, // in range 0.0 .. 1.0
                        QWidget* glWidget,  // used in application capture mode only
                        const QPixmap& logo       // logo over video
                       );
    virtual ~DesktopFileEncoder();
    bool start();
    void stop();
    QString fileName() const { return m_fileName; }

    QString lastErrorStr() const { return m_lastErrorStr; }
protected:
    // CLLongRunnable runable
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
        EncodedAudioInfo(DesktopFileEncoder* owner);
        ~EncodedAudioInfo();
        // doubled audio objects
        QAudioDeviceInfo m_audioDevice;
        QnAudioFormat m_audioFormat;
        CLThreadQueue<QnAbstractMediaDataPtr>  m_audioQueue;
        QnAbstractMediaData m_tmpAudioBuffer;
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
        DesktopFileEncoder* m_owner;
        HWAVEIN hWaveIn;
        QQueue<WAVEHDR*> m_buffers;
        QMutex m_mtx;
        bool m_terminated;
    };

    int m_encodedFrames;
    CLBufferedScreenGrabber* m_grabber;
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

    CLScreenGrabber::CaptureMode m_captureMode;
    bool m_captureCursor;
    QSize m_captureResolution;
    float m_encodeQualuty;
    QWidget* m_widget;
    bool m_videoPacketWrited;
    QString m_lastErrorStr;
    bool m_capturingStopped;
    const QPixmap m_logo;
    friend void QT_WIN_CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD dwInstance,  DWORD dwParam1, DWORD dwParam2);
};

#endif //__DESKTOP_H264_STREAM_READER_H
