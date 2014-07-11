#ifndef h264_stream_provider_h_2015
#define h264_stream_provider_h_2015

#ifdef ENABLE_DATA_PROVIDERS

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"


class QnRtpStreamReader: public CLServerPushStreamReader
{
public:
    QnRtpStreamReader(const QnResourcePtr& res, const QString& request = QString());
    virtual ~QnRtpStreamReader();

    void setRequest(const QString& request);
    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
protected:
    

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    void updateStreamParamsBasedOnQuality() override {}

    void updateStreamParamsBasedOnFps() override {}
private:
    QnMulticodecRtpReader m_rtpReader;
    QString m_request;


};

#endif // ENABLE_DATA_PROVIDERS

#endif //h264_stream_provider_h_2015

