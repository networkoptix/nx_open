#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include "core/dataprovider/spush_media_stream_provider.h"
#include "network/multicodec_rtp_reader.h"

class QnRtpStreamReader: public CLServerPushStreamReader
{
public:
    QnRtpStreamReader(const QnResourcePtr& res, const QString& request = QString());
    virtual ~QnRtpStreamReader();

    void setRtpTransport(const RtpTransport::Value& transport);
    void setRequest(const QString& request);
    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const override;
    virtual void pleaseStop() override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

protected:
    QnMulticodecRtpReader m_rtpReader;

private:
    QString m_request;
    RtpTransport::Value m_rtpTransport;
    size_t m_dataPassed = 0;
};

#endif // defined(ENABLE_DATA_PROVIDERS)
