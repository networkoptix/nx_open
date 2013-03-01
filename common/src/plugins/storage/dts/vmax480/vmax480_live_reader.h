#ifndef vmax480_live_h_1740
#define vmax480_live_h_1740

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "vmax480_stream_fetcher.h"

class QnVMax480ConnectionProcessor;

class QnVMax480LiveProvider: public CLServerPushStreamreader, public VMaxStreamFetcher
{
public:
    QnVMax480LiveProvider(QnResourcePtr dev );
    virtual ~QnVMax480LiveProvider();

    virtual void onGotData(QnAbstractMediaDataPtr mediaData) override;
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
    virtual void beforeRun() override;
    virtual void afterRun() override;

private:
    QnNetworkResourcePtr m_networkRes;
    CLDataQueue m_internalQueue;
    //QnVMax480ConnectionProcessor* m_processor;
};

#endif //vmax480_live_h_1740
