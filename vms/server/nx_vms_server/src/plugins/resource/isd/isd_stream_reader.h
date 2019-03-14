#pragma once

#ifdef ENABLE_ISD

#include "providers/spush_media_stream_provider.h"
#include "network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"


class QnISDStreamReader: public CLServerPushStreamReader
{
public:
    static const int ISD_HTTP_REQUEST_TIMEOUT_MS;

    QnISDStreamReader(const QnPlIsdResourcePtr& res);
    virtual ~QnISDStreamReader();

    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;

private:
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QString serializeStreamParams(
        const QnLiveStreamParams& params,
        int profileIndex) const;

private:
    QnMulticodecRtpReader m_rtpStreamParser;
    QnPlIsdResourcePtr m_isdCam;
};

#endif // #ifdef ENABLE_ISD
