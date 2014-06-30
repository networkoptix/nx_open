#ifndef vmax480_live_h_1740
#define vmax480_live_h_1740

#ifdef ENABLE_VMAX

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "vmax480_stream_fetcher.h"

#include <QtCore/QElapsedTimer>


class QnVMax480ConnectionProcessor;
class VMaxStreamFetcher;

class QnVMax480LiveProvider: public CLServerPushStreamReader, public QnVmax480DataConsumer
{
public:
    QnVMax480LiveProvider(const QnResourcePtr& dev );
    virtual ~QnVMax480LiveProvider();

    virtual int getChannel() const override;
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
    virtual void beforeRun() override;
    virtual void afterRun() override;

    virtual bool canChangeStatus() const override;

private:
    QnNetworkResourcePtr m_networkRes;

    VMaxStreamFetcher* m_maxStream;
    bool m_opened;
    QElapsedTimer m_lastMediaTimer;
    //QnVMax480ConnectionProcessor* m_processor;
};

#endif // #ifdef ENABLE_VMAX
#endif //vmax480_live_h_1740
