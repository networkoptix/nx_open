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
    int audioPacketSize(bool isPrimary);

    int initIOContext();
    qint32 writePacketImpl(quint8* buf, qint32 bufSize);
    qint32 readPacketImpl(quint8* buf, quint32 bufSize);
    qint64 seekPacketImpl(qint64 offset, qint32 whence);
    qint64 currentTime() const;
    int calculateBitrate();
    int processData(bool flush);
    void stopCapturing();
private:
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
    bool m_useSecondaryAudio;
    bool m_useAudio;
    // doubled audio objects
    QIODevice* m_audioOStream;
    QIODevice* m_audioOStream2;
    QAudioDeviceInfo m_audioDevice;
    QAudioDeviceInfo m_audioDevice2;
    QAudioInput* m_audioInput;
    QAudioInput* m_audioInput2;
    QAudioFormat m_audioFormat;
    QAudioFormat m_audioFormat2;
    CLThreadQueue<CLAbstractMediaData*>  m_audioQueue;
    CLThreadQueue<CLAbstractMediaData*>  m_secondAudioQueue;
    CLAbstractMediaData m_tmpAudioBuffer1;
    CLAbstractMediaData m_tmpAudioBuffer2;

    CLScreenGrapper::CaptureMode m_captureMode;
    bool m_captureCursor;
    QSize m_captureResolution;
    float m_encodeQualuty;
    QWidget* m_widget;
    bool m_videoPacketWrited;
    QString m_lastErrorStr;
    SpeexPreprocessState* m_speexPreprocess;
};

#endif //__DESKTOP_H264_STREAM_READER_H
