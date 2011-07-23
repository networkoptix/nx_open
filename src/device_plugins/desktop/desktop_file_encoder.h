#ifndef __DESKTOP_H264_STREAM_READER_H
#define __DESKTOP_H264_STREAM_READER_H

#include <QIODevice>
#include <QAudioDeviceInfo>
#include <QAudioInput>

#include "device_plugins/desktop/buffered_screen_grabber.h"
#include "data/mediadata.h"


class CaptureAudioStream;

class DesktopFileEncoder: public CLLongRunnable
{
private:
    enum {BLOCK_SIZE = 1460};
public:
    DesktopFileEncoder( const QString& fileName, 
                        int desktopNum,           // = 0, 
                        const QAudioDeviceInfo audioDevice, //  = QAudioDeviceInfo::defaultInputDevice(),
                        CLScreenGrapper::CaptureMode mode,
                        bool captureCursor,
                        const QSize& captureResolution,
                        float encodeQualuty // in range 0.0 .. 1.0
                       );
    virtual ~DesktopFileEncoder();

    QString fileName() const { return m_fileName; }
protected:
    // CLLongRunnable runable
    virtual void run();
private:
    bool init();
private:
    friend class CaptureAudioStream;

    virtual void openStream();
    virtual void closeStream();
    int audioPacketSize();

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
    AVCodecContext* m_audioCodecCtx;
    bool m_initialized;
    AVFrame* m_frame;
    int m_desktopNum;

    AVFormatContext* m_formatCtx;
    AVOutputFormat * m_outputCtx;

    AVStream * m_videoOutStream;
    AVStream * m_audioOutStream;

    int m_videoStreamIndex;

    QString m_fileName;

    QVector<quint8> m_buffer;
    ByteIOContext* m_iocontext;
    QIODevice * m_device;

    QAudioDeviceInfo m_audioDevice;
    QAudioInput* m_audioInput;
    QIODevice* m_audioOStream;
    quint8* m_audioBuf;
    QAudioFormat m_audioFormat;

    CLThreadQueue<CLAbstractMediaData*>  m_audioQueue;

    int m_audioFramesCount;
    double m_audioFrameDuration;
    qint64 m_storedAudioPts;
    int m_maxAudioJitter;

    CLScreenGrapper::CaptureMode m_captureMode;
    bool m_captureCursor;
    QSize m_captureResolution;
   float m_encodeQualuty;

};

#endif //__DESKTOP_H264_STREAM_READER_H
