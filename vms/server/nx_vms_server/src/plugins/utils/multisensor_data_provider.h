#pragma once

#ifdef ENABLE_ONVIF

#include <providers/stream_mixer.h>
#include <providers/spush_media_stream_provider.h>
#include <core/resource/resource_fwd.h>

class QnOnvifStreamReader;

namespace nx {
namespace plugins {
namespace utils {

class MultisensorDataProvider : public CLServerPushStreamReader
{
public:
    MultisensorDataProvider(const nx::vms::server::resource::CameraPtr& res);
    virtual ~MultisensorDataProvider();

protected:

    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

    virtual bool isStreamOpened() const override;

    virtual QnAbstractMediaDataPtr getNextData() override;

    virtual void closeStream() override;

    virtual void pleaseStop() override;

    // Supress processing metada second time by	MultisensorDataProvider.
    virtual void processAvMetadata(const QnAbstractMediaDataPtr& data) override {}
private:
    nx::vms::server::resource::CameraPtr m_cameraResource;
    QnStreamMixer m_dataSource;
private:

    QnSecurityCamResourcePtr initSubChannelResource(quint32 channelNumber);
    QList<QnResourceChannelMapping> getVideoChannelMapping();
};


} // namespace utils
} // namespace plugins
} // namespace nx


typedef std::shared_ptr<nx::plugins::utils::MultisensorDataProvider> MultisensorDataProviderPtr;

#endif // #ifdef ENABLE_ONVIF

