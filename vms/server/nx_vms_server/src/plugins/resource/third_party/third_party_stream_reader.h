/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#pragma once

#ifdef ENABLE_THIRD_PARTY

#include <core/dataprovider/abstract_media_stream_provider.h>
#include <providers/spush_media_stream_provider.h>
#include <core/resource/resource_media_layout.h>

#include "third_party_resource.h"
#include <nx/utils/time_helper.h>

//!Stream reader for resource, implemented in external plugin
class ThirdPartyStreamReader: public CLServerPushStreamReader
{
    typedef CLServerPushStreamReader base_type;

    struct Extras
    {
        QByteArray extradataBlob;
    };

public:
    ThirdPartyStreamReader(
        QnThirdPartyResourcePtr res,
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

    void setNeedCorrectTime(bool value);
    virtual CameraDiagnostics::Result lastOpenStreamResult() const override;
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

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
    nx::utils::TimeHelper* timeHelper(const QnAbstractMediaDataPtr& data);
private:
    QnMetaDataV1Ptr m_lastMetadata;
    std::unique_ptr<QnAbstractMediaStreamProvider> m_builtinStreamReader;
    mutable QnMutex m_streamReaderMutex;
    QnThirdPartyResourcePtr m_thirdPartyRes;
    nxcip_qt::BaseCameraManager m_camManager;
    // TODO: Migrate to nx::sdk::Ptr.
    std::shared_ptr<nxcip::StreamReader> m_liveStreamReader;
    QnAbstractMediaDataPtr m_savedMediaPacket;
    QSize m_videoResolution;
    QnConstMediaContextPtr m_audioContext;
    std::shared_ptr<nxcip::CameraMediaEncoder2> m_mediaEncoder2;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    unsigned int m_cameraCapabilities = 0;

    bool m_needCorrectTime = false;
    using TimeHelperPtr = std::unique_ptr<nx::utils::TimeHelper>;
    std::vector<TimeHelperPtr> m_videoTimeHelpers;
    std::vector<TimeHelperPtr> m_audioTimeHelpers;
    std::atomic_flag m_isMediaUrlValid;

    void initializeAudioContext( const nxcip::AudioFormat& audioFormat, const Extras& extras );
};

#endif // ENABLE_THIRD_PARTY
