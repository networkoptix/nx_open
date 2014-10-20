#ifndef dlink_stream_reader_h_0251
#define dlink_stream_reader_h_0251

#ifdef ENABLE_DLINK

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/multicodec_rtp_reader.h"

class PlDlinkStreamReader: public CLServerPushStreamReader
{
public:
    PlDlinkStreamReader(const QnResourcePtr& res);
    virtual ~PlDlinkStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual QString composeVideoProfile();


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
    virtual void pleaseStop() override;

private:

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QString getRTPurl(int profileId) const;
    QString getQualityString() const;
    bool isTextQualities(const QStringList& qualities) const;
private:
    QnMulticodecRtpReader m_rtpReader;
    CLSimpleHTTPClient* mHttpClient;

    bool m_h264;
    bool m_mpeg4;

};

#endif // ENABLE_DLINK
#endif //dlink_stream_reader_h_0251