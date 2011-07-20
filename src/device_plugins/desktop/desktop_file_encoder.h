#ifndef __DESKTOP_H264_STREAM_READER_H
#define __DESKTOP_H264_STREAM_READER_H

#ifdef Q_OS_WIN

#include "device_plugins/desktop/buffered_screen_grabber.h"

class DesktopFileEncoder: public CLLongRunnable
{
private:
    enum {BLOCK_SIZE = 1460};
public:
    DesktopFileEncoder(const QString& fileName, int desktopNum = 0); // 0 - default
    virtual ~DesktopFileEncoder();

    QString fileName() const { return m_fileName; }
protected:
    virtual void run();
private:
    bool init();
private:
    virtual void openStream();
    virtual void closeStream();

    int initIOContext();
    qint32 writePacketImpl(quint8* buf, qint32 bufSize);
    qint32 readPacketImpl(quint8* buf, quint32 bufSize);
    qint64 seekPacketImpl(qint64 offset, qint32 whence);

    CLBufferedScreenGrabber* m_grabber;
    quint8* m_videoBuf;
    int m_videoBufSize;
    AVCodecContext* m_videoCodecCtx;
    AVCodecContext* m_audioCodecCtx;
    char* m_encoderCodecName;
    bool m_initialized;
    AVFrame* m_frame;
    int m_desktopNum;

    AVFormatContext* m_formatCtx;
    AVOutputFormat * m_outputCtx;
    AVStream * m_outStream;
    AVStream * m_audioStream;
    int m_videoStream;
    QString m_fileName;

    QVector<quint8> m_buffer;
    ByteIOContext* m_iocontext;
    QIODevice * m_device;
};

#endif // Q_OS_WIN

#endif //__DESKTOP_H264_STREAM_READER_H
