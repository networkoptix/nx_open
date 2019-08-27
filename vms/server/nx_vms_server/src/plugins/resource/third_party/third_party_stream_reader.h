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
#include <nx/sdk/ptr.h>

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
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void pleaseStop() override;
    virtual void beforeRun() override;
    virtual void afterRun() override;

private:
    //virtual bool needMetaData() const override;
    virtual QnMetaDataV1Ptr getCameraMetadata() override;
    nx::utils::TimeHelper* timeHelper(const QnAbstractMediaDataPtr& data);
    void updateSourceUrl(const QString& urlString);

private:
    QnMetaDataV1Ptr m_lastMetadata;
    std::unique_ptr<QnAbstractMediaStreamProvider> m_builtinStreamReader;
    mutable QnMutex m_streamReaderMutex;
    QnThirdPartyResourcePtr m_thirdPartyRes;
    nxcip_qt::BaseCameraManager m_camManager;
    nx::sdk::Ptr<nxcip::StreamReader> m_liveStreamReader;
    QnAbstractMediaDataPtr m_savedMediaPacket;
    QSize m_videoResolution;
    QnConstMediaContextPtr m_audioContext;
    nx::sdk::Ptr<nxcip::CameraMediaEncoder2> m_mediaEncoder2;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    unsigned int m_cameraCapabilities = 0;

    bool m_needCorrectTime = false;
    using TimeHelperPtr = std::unique_ptr<nx::utils::TimeHelper>;
    std::vector<TimeHelperPtr> m_videoTimeHelpers;
    std::vector<TimeHelperPtr> m_audioTimeHelpers;
    std::atomic_flag m_isMediaUrlValid;
    bool m_serverSideUpdate = false;

    void initializeAudioContext( const nxcip::AudioFormat& audioFormat, const Extras& extras );
};

#endif // ENABLE_THIRD_PARTY
