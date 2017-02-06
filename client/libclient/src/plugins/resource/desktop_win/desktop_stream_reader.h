#pragma once

#include "core/dataprovider/spush_media_stream_provider.h"
#include "buffered_screen_grabber.h"

class QnDesktopStreamreader: public CLServerPushStreamReader
{
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
