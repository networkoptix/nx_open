#ifndef droid_stream_reader_h_1756
#define droid_stream_reader_h_1756

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "core/dataprovider/live_stream_provider.h"


class PlDroidStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    PlDroidStreamReader(QnResourcePtr res);
    virtual ~PlDroidStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

private:


private:

    TCPSocket* m_sock;
};

#endif //dlink_stream_reader_h_0251