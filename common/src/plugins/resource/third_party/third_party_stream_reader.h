/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_STREAM_READER_H
#define THIRD_PARTY_STREAM_READER_H

#ifdef ENABLE_THIRD_PARTY

#include "core/dataprovider/abstract_media_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "core/resource/resource_media_layout.h"
#include "third_party_resource.h"


//!Stream reader for resource, implemented in external plugin
class ThirdPartyStreamReader
:
    public CLServerPushStreamReader
{
    typedef CLServerPushStreamReader base_type;

public:
    ThirdPartyStreamReader(
        QnResourcePtr res,
        nxcip::BaseCameraManager* camManager );
    virtual ~ThirdPartyStreamReader();

    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

    static CodecID toFFmpegCodecID( nxcip::CompressionType compressionType );
    static QnAbstractMediaDataPtr readStreamReader( nxcip::StreamReader* streamReader );

    //!Overrides QnLiveStreamProvider::onGotVideoFrame()
    virtual void onGotVideoFrame(const QnCompressedVideoDataPtr& videoData) override;
    //!Overrides QnLiveStreamProvider::updateSoftwareMotion()
    virtual void updateSoftwareMotion() override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual int getLastResponseCode() const override;

    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

    virtual void pleaseStop() override;
    virtual void beforeRun() override;
    virtual void afterRun() override;

    //!Overrides QnLiveStreamProvider::roleForMotionEstimation()
    virtual Qn::ConnectionRole roleForMotionEstimation() override;
    //!Overrides QnLiveStreamProvider::onStreamResolutionChanged()
    virtual void onStreamResolutionChanged( int channelNumber, const QSize& picSize );

private:
    //virtual bool needMetaData() const override;
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

private:
    QnMetaDataV1Ptr m_lastMetadata;
    std::unique_ptr<QnAbstractMediaStreamProvider> m_builtinStreamReader;
    QnThirdPartyResourcePtr m_thirdPartyRes;
    nxcip_qt::BaseCameraManager m_camManager;
    nxcip::StreamReader* m_liveStreamReader;
    QnAbstractMediaDataPtr m_savedMediaPacket;
    QSize m_videoResolution;
    QnMediaContextPtr m_audioContext;
    nxcip::CameraMediaEncoder2* m_mediaEncoder2Ref;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    unsigned int m_cameraCapabilities;

    QnAbstractMediaDataPtr readLiveStreamReader();
    void initializeAudioContext( const nxcip::AudioFormat& audioFormat );
};

#endif // ENABLE_THIRD_PARTY

#endif // THIRD_PARTY_STREAM_READER_H
