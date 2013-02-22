#ifndef vmax480_live_h_1740
#define vmax480_live_h_1740

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/resource/resource_fwd.h"

class QnVMax480ConnectionProcessor;

class QnVMax480LiveProvider: public CLServerPushStreamreader
{
public:
    QnVMax480LiveProvider(QnResourcePtr dev );
    virtual ~QnVMax480LiveProvider();

    void onGotData(QnAbstractMediaDataPtr mediaData);
    void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime);
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
    bool m_connected;
    CLDataQueue m_internalQueue;
    QProcess* m_vMaxProxy;
    QString m_tcpID;
    //QnVMax480ConnectionProcessor* m_processor;
};

#endif //vmax480_live_h_1740
