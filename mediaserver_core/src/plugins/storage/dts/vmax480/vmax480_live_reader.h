#pragma once

#ifdef ENABLE_VMAX

#include "providers/spush_media_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "vmax480_stream_fetcher.h"

#include <QtCore/QElapsedTimer>


class QnVMax480ConnectionProcessor;
class VMaxStreamFetcher;

class QnVMax480LiveProvider: public CLServerPushStreamReader, public QnVmax480DataConsumer
{
public:
    QnVMax480LiveProvider(
        const QnPlVmax480ResourcePtr& dev);
    virtual ~QnVMax480LiveProvider();

    virtual int getChannel() const override;
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void beforeRun() override;
    virtual void afterRun() override;
    virtual bool reinitResourceOnStreamError() const { return false; }

private:
    QnPlVmax480ResourcePtr m_networkRes;

    VMaxStreamFetcher* m_maxStream = nullptr;
    bool m_opened = false;
    QElapsedTimer m_lastMediaTimer;
    //QnVMax480ConnectionProcessor* m_processor;
};

#endif // #ifdef ENABLE_VMAX
