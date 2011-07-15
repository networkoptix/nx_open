#ifndef __DESKTOP_H264_STREAM_READER_H
#define __DESKTOP_H264_STREAM_READER_H

#include "streamreader/spush_streamreader.h"
#include "device_plugins/desktop/buffered_screen_grabber.h"

class CLDesktopStreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
    CLDesktopStreamreader(CLDevice* dev);
    virtual ~CLDesktopStreamreader();
protected:
    virtual CLAbstractMediaData* getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    bool init();
private:
    CLBufferedScreenGrabber* m_grabber;
    quint8* m_videoBuf;
    int m_videoBufSize;
    AVCodecContext* m_videoCodecCtx;
    char* m_encoderCodecName;
    bool m_initialized;
    AVFrame* m_frame;
};

#endif //__DESKTOP_H264_STREAM_READER_H
