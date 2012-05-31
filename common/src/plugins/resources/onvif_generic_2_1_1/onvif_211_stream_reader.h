#ifndef ONVIF_2_1_1_STREAM_REDER_H__
#define ONVIF_2_1_1_STREAM_REDER_H__

#include "onvif_211_helper.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/h264_rtp_reader.h"

class _onvifMedia__GetProfilesResponse;
class _onvifMedia__GetProfileResponse;
class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;

const ResolutionPair SECONDARY_STREAM_DEFAULT_RESOLUTION(320, 240);

class QnOnvifGeneric211StreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnOnvifGeneric211StreamReader(QnResourcePtr res);
    virtual ~QnOnvifGeneric211StreamReader();

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
    RTPH264StreamreaderDelegate m_RTP264;
    QnPlOnvifGeneric211ResourcePtr m_onvifRes;
};

#endif // ONVIF_2_1_1_STREAM_REDER_H__
