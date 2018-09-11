#pragma once

#ifdef ENABLE_DLINK

#include <providers/spush_media_stream_provider.h>
#include <nx/network/deprecated/simple_http_client.h>
#include "network/multicodec_rtp_reader.h"
#include "dlink_resource.h"

class PlDlinkStreamReader: public CLServerPushStreamReader
{
public:
    PlDlinkStreamReader(const QnPlDlinkResourcePtr& res);
    virtual ~PlDlinkStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual QString composeVideoProfile(bool isCameraControlRequired, const QnLiveStreamParams& params);
    virtual void pleaseStop() override;

private:

    QnAbstractMediaDataPtr getNextDataMPEG(AVCodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QString getRTPurl(int profileId) const;
    QByteArray getQualityString(const QnLiveStreamParams& params) const;
    bool isTextQualities(const QList<QByteArray>& qualities) const;
private:
    QnMulticodecRtpReader m_rtpReader;
    std::unique_ptr<CLSimpleHTTPClient> m_HttpClient;
    QSize m_resolution;
    QnDlink_ProfileInfo m_profile;
    QnPlDlinkResourcePtr m_dlinkRes;
};

#endif // ENABLE_DLINK
