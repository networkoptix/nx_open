#ifndef QN_DESKTOP_STREAM_READER_H
#define QN_DESKTOP_STREAM_READER_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include "core/dataprovider/spush_media_stream_provider.h"

#include "buffered_screen_grabber.h"


class QnDesktopStreamreader: public CLServerPushStreamReader {
    Q_OBJECT

public:
    QnDesktopStreamreader(const QnResourcePtr &dev);
    virtual ~QnDesktopStreamreader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseReopenStream() override {}

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
    AVPacket* m_outPacket;
};

#endif // Q_OS_WIN

#endif //QN_DESKTOP_STREAM_READER_H
