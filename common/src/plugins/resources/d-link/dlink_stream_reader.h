#ifndef dlink_stream_reader_h_0251
#define dlink_stream_reader_h_0251

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "core/dataprovider/live_stream_provider.h"


class PlDlinkStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
protected:
    struct DlinkStreamParams
    {
        enum Codec{mjpeg, h264, mpeg4};
        int profileNum;
        Codec codec;
        QSize resolution;
        int frameRate;
        int qulity;
    };

public:
    PlDlinkStreamReader(QnResourcePtr res);
    virtual ~PlDlinkStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual QString composeVideoProfile(const DlinkStreamParams& streamParams) const;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;


private:
    CLSimpleHTTPClient* mHttpClient;

};

#endif //dlink_stream_reader_h_0251