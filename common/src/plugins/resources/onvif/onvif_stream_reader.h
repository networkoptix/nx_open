#ifndef onvif_stream_reader_h
#define onvif_stream_reader_h

#include "onvif_helper.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "soap_wrapper.h"

class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;

class QnOnvifStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
    static const int MAX_VIDEO_PARAMS_RESET_TRIES;

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

    bool isGotFrame(QnCompressedVideoDataPtr videoData);

    const QString updateCameraAndFetchStreamUrl() const;
    const QString updateCameraAndFetchStreamUrl(bool isPrimary) const;

    const QString getStreamUrl(MediaSoapWrapper& soapWrapper, const std::string& profileToken, bool isPrimary) const;

    std::string findAppropriateProfileToken(MediaSoapWrapper& soapWrapper, bool isPrimary) const;
    //Returned ptr can be used only when response is not destroyed!
    onvifXsd__Profile* findAppropriateProfile(const ProfilesResp& response, bool isPrimary) const;

    void setVideoEncoderParams(MediaSoapWrapper& soapWrapper, onvifXsd__Profile& profile, bool isPrimary) const;
    void updateVideoEncoderParams(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const;
    const QString normalizeStreamSrcUrl(const std::string& src) const;
    void printProfile(const ProfileResp& response, bool isPrimary) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;
};

#endif // onvif_stream_reader_h
