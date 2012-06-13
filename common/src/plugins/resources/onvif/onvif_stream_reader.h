#ifndef onvif_stream_reader_h
#define onvif_stream_reader_h

#include "onvif_helper.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"

class _onvifMedia__GetProfilesResponse;
class _onvifMedia__GetProfileResponse;
class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;

class QnOnvifStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnOnvifStreamReader(QnResourcePtr res);
    virtual ~QnOnvifStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
private:

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    void processTriggerData(const quint8* payload, int len);

    void fillMotionInfo(const QRect& rect);
    bool isGotFrame(QnCompressedVideoDataPtr videoData);

    const QString updateCameraAndfetchStreamUrl() const;
    const QString updateCameraAndfetchStreamUrl(bool isPrimary) const;

    //Returned ptr can be used only when response is not destroyed!
    onvifXsd__Profile* findAppropriateProfile(const _onvifMedia__GetProfilesResponse& response, bool isPrimary) const;

    void setRequiredValues(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const;
    const QString normalizeStreamSrcUrl(const std::string& src) const;
    void printProfile(const _onvifMedia__GetProfileResponse& response, bool isPrimary) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;
};

#endif // onvif_stream_reader_h
