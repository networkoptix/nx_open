#ifndef __DESKTOP_H264_STREAM_READER_H
#define __DESKTOP_H264_STREAM_READER_H
#ifdef Q_OS_WIN


#include "../buffered_screen_grabber.h"
#include "core/dataprovider/spush_media_stream_provider.h"


class CLDesktopStreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
    CLDesktopStreamreader(QnResourcePtr dev);
    virtual ~CLDesktopStreamreader();
protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    bool init();
private:
    QnBufferedScreenGrabber* m_grabber;
    quint8* m_videoBuf;
    int m_videoBufSize;
    AVCodecContext* m_videoCodecCtx;
    char* m_encoderCodecName;
    bool m_initialized;
    AVFrame* m_frame;
};

#endif // Q_OS_WIN

#endif //__DESKTOP_H264_STREAM_READER_H
