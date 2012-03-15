#ifndef h264_stream_provider_h_2015
#define h264_stream_provider_h_2015

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "utils/network/h264_rtp_reader.h"


class RTP264StreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    RTP264StreamReader(QnResourcePtr res, const QString& request = "");
    virtual ~RTP264StreamReader();

    void setRequest(const QString& request);
protected:
    

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    void updateStreamParamsBasedOnQuality() override {};

    void updateStreamParamsBasedOnFps() override {};
private:

    RTPH264StreamreaderDelegate mRTP264;
    QString m_request;


};

#endif //h264_stream_provider_h_2015