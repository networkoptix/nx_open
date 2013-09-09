/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_STREAM_READER_H
#define THIRD_PARTY_STREAM_READER_H

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"
#include "third_party_resource.h"


//!Stream reader for resource, implemented in external plugin
class ThirdPartyStreamReader
:
    public CLServerPushStreamReader
{
public:
    ThirdPartyStreamReader(
        QnResourcePtr res,
        nxcip::BaseCameraManager* camManager );
    virtual ~ThirdPartyStreamReader();

    const QnResourceAudioLayout* getDPAudioLayout() const;

    static CodecID toFFmpegCodecID( nxcip::CompressionType compressionType );
    static QnAbstractMediaDataPtr readStreamReader( nxcip::StreamReader* streamReader );

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual int getLastResponseCode() const override;

    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

    virtual void pleaseStop() override;

private:
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_rtpStreamParser;
    QnThirdPartyResourcePtr m_thirdPartyRes;
    nxcip_qt::BaseCameraManager m_camManager;
    nxcip::StreamReader* m_liveStreamReader;

    nxcip::Resolution getMaxResolution( int encoderNumber ) const;
    //!Returns resolution with pixel count equal or less than \a desiredResolution
    nxcip::Resolution getNearestResolution( int encoderNumber, const nxcip::Resolution& desiredResolution ) const;
    void readMotionInfo();
    QnAbstractMediaDataPtr readLiveStreamReader();
};

#endif // THIRD_PARTY_STREAM_READER_H
