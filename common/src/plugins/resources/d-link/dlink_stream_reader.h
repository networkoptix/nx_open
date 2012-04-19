#ifndef dlink_stream_reader_h_0251
#define dlink_stream_reader_h_0251

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "core/dataprovider/live_stream_provider.h"
#include "utils/network/h264_rtp_reader.h"



class PlDlinkStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    PlDlinkStreamReader(QnResourcePtr res);
    virtual ~PlDlinkStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual QString composeVideoProfile();


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;


private:

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;

private:

    RTPH264StreamreaderDelegate mRTP264;
    CLSimpleHTTPClient* mHttpClient;

    bool m_h264;
    bool m_mpeg4;

};

#endif //dlink_stream_reader_h_0251