/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_STREAM_READER_H
#define THIRD_PARTY_STREAM_READER_H

#ifdef ENABLE_THIRD_PARTY

#include <core/dataprovider/abstract_media_stream_provider.h>
#include <providers/spush_media_stream_provider.h>
#include <core/resource/resource_media_layout.h>

#include "third_party_resource.h"

//!Stream reader for resource, implemented in external plugin
class ThirdPartyStreamReader
:
    public CLServerPushStreamReader
{
    typedef CLServerPushStreamReader base_type;

    struct Extras
    {
        QByteArray extradataBlob;
    };

public:
    ThirdPartyStreamReader(
        QnResourcePtr res,
        nxcip::BaseCameraManager* camManager );
    virtual ~ThirdPartyStreamReader();

    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

    static AVCodecID toFFmpegCodecID( nxcip::CompressionType compressionType );
    static QnAbstractMediaDataPtr readStreamReader(
        nxcip::StreamReader* streamReader,
        int* errorCode = nullptr,
        Extras* outExtras = nullptr);

    virtual void updateSoftwareMotion() override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual int getLastResponseCode() const override;

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
    mutable QnMutex m_streamReaderMutex;
    QnThirdPartyResourcePtr m_thirdPartyRes;
    nxcip_qt::BaseCameraManager m_camManager;
    std::shared_ptr<nxcip::StreamReader> m_liveStreamReader;
    QnAbstractMediaDataPtr m_savedMediaPacket;
    QSize m_videoResolution;
    QnConstMediaContextPtr m_audioContext;
    std::shared_ptr<nxcip::CameraMediaEncoder2> m_mediaEncoder2;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    unsigned int m_cameraCapabilities;

    void initializeAudioContext( const nxcip::AudioFormat& audioFormat, const Extras& extras );
};

#endif // ENABLE_THIRD_PARTY

#endif // THIRD_PARTY_STREAM_READER_H
