#ifndef __DESKTOP_H264_STREAM_READER_H
#define __DESKTOP_H264_STREAM_READER_H

#include <QIODevice>
#include <QAudioDeviceInfo>
#include <QAudioInput>

#include "device_plugins/desktop/buffered_screen_grabber.h"
#include "data/mediadata.h"
#include <dsp_effects/speex/speex_preprocess.h>

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
                        CLScreenGrapper::CaptureMode mode,
                        bool captureCursor,
                        const QSize& captureResolution,
                        float encodeQualuty, // in range 0.0 .. 1.0
                        QWidget* glWidget  // used in application capture mode only
                       );
    virtual ~DesktopFileEncoder();
    void stop();
    bool start();
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
    struct EncodedAudioInfo
    {
        EncodedAudioInfo();
        ~EncodedAudioInfo();
        // doubled audio objects
        QIODevice* m_audioOStream;
        QAudioDeviceInfo m_audioDevice;
        QAudioInput* m_audioInput;
        QAudioFormat m_audioFormat;
        CLThreadQueue<CLAbstractMediaData*>  m_audioQueue;
        CLAbstractMediaData m_tmpAudioBuffer;
        SpeexPreprocessState* m_speexPreprocess;

        int audioPacketSize();
        bool setupFormat(DesktopFileEncoder* owner, QString& errMessage);
        void setupPostProcess();
        qint64 currentTime() const { return m_owner->m_grabber->currentTime(); }
    private:
        DesktopFileEncoder* m_owner;
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
    ByteIOContext* m_iocontext;
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

    CLScreenGrapper::CaptureMode m_captureMode;
    bool m_captureCursor;
    QSize m_captureResolution;
    float m_encodeQualuty;
    QWidget* m_widget;
    bool m_videoPacketWrited;
    QString m_lastErrorStr;
};

#endif //__DESKTOP_H264_STREAM_READER_H
