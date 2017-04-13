#ifndef h264_stream_provider_h_2015
#define h264_stream_provider_h_2015

#ifdef ENABLE_DATA_PROVIDERS

#include "core/dataprovider/spush_media_stream_provider.h"
#include "network/multicodec_rtp_reader.h"

class QnRtpStreamReader: public CLServerPushStreamReader
{
public:
    QnRtpStreamReader(const QnResourcePtr& res, const QString& request = QString());
    virtual ~QnRtpStreamReader();

    void setRtpTransport(const RtpTransport::Value& transport);
    void setRequest(const QString& request);
    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    virtual void pleaseStop() override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    //virtual void pleaseReopenStream() override {}

private:
    QnMulticodecRtpReader m_rtpReader;
    QString m_request;
    RtpTransport::Value m_rtpTransport;

};

#endif // ENABLE_DATA_PROVIDERS

#endif //h264_stream_provider_h_2015

