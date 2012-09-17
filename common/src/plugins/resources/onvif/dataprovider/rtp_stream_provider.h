#ifndef h264_stream_provider_h_2015
#define h264_stream_provider_h_2015

#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"


class QnRtpStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnRtpStreamReader(QnResourcePtr res, const QString& request = QString());
    virtual ~QnRtpStreamReader();

    void setRequest(const QString& request);
    const QnResourceAudioLayout* getDPAudioLayout() const;
protected:
    

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    void updateStreamParamsBasedOnQuality() override {}

    void updateStreamParamsBasedOnFps() override {}
private:
    QnMulticodecRtpReader m_rtpReader;
    QString m_request;


};

#endif //h264_stream_provider_h_2015
