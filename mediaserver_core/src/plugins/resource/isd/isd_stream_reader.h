#ifndef isd_STREAM_REDER_H__1914
#define isd_STREAM_REDER_H__1914

#ifdef ENABLE_ISD

#include "core/dataprovider/spush_media_stream_provider.h"
#include "network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"


class QnISDStreamReader: public CLServerPushStreamReader
{
public:
    static const int ISD_HTTP_REQUEST_TIMEOUT_MS;

    QnISDStreamReader(const QnResourcePtr& res);
    virtual ~QnISDStreamReader();

    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;

private:
    QnMulticodecRtpReader m_rtpStreamParser;

    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QString serializeStreamParams(
        const QnLiveStreamParams& params,
        int profileIndex) const;
};

#endif // #ifdef ENABLE_ISD
#endif // isd_STREAM_REDER_H__1914
