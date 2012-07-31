#ifndef QN_DESKTOP_FILE_ENCODER_H
#define QN_DESKTOP_FILE_ENCODER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include "../buffered_screen_grabber.h"
#include "core/dataprovider/spush_media_stream_provider.h"


class QnDesktopStreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};

public:
    QnDesktopStreamreader(QnResourcePtr dev);
    virtual ~QnDesktopStreamreader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void openStream();
    virtual void closeStream();
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

#endif //QN_DESKTOP_FILE_ENCODER_H
