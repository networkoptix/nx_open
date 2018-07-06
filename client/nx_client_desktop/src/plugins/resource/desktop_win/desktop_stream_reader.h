#pragma once

#include "buffered_screen_grabber.h"
#include <core/dataprovider/abstract_media_stream_provider.h>
#include <core/dataprovider/live_stream_params.h>
#include <nx/streaming/abstract_stream_data_provider.h>

class QnDesktopStreamreader:
    public QnAbstractStreamDataProvider,
    public QnAbstractMediaStreamProvider
{
public:
    QnDesktopStreamreader(const QnResourcePtr &dev);
    virtual ~QnDesktopStreamreader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;

    CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params);
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    //virtual void pleaseReopenStream()  override {}

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
