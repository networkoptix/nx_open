#ifndef isd_stream_reader_h_2015
#define isd_stream_reader_h_2015

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "../onvif/dataprovider/onvif_h264.h"


class PlISDStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    PlISDStreamReader(QnResourcePtr res);
    virtual ~PlISDStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    void updateStreamParamsBasedOnQuality() override {};
        
    void updateStreamParamsBasedOnFps() override {};
private:

    RTPH264StreamreaderDelegate mRTP264;


};

#endif //isd_stream_reader_h_2015