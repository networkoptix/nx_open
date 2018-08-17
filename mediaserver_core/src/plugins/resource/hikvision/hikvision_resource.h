#pragma once

#ifdef ENABLE_ONVIF

#include <boost/optional/optional.hpp>

#include <plugins/resource/hikvision/hikvision_utils.h>
#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HikvisionResource: public QnPlOnvifResource
{
    Q_OBJECT

    using base_type = QnPlOnvifResource;
public:
    HikvisionResource();
    virtual ~HikvisionResource() override;

    virtual QString bestCodec() const override;

    boost::optional<hikvision::ChannelCapabilities>
    channelCapabilities(Qn::ConnectionRole role);
    bool findDefaultPtzProfileToken();

    static bool tryToEnableIntegrationProtocols(const QUrl& url, const QAuthenticator& authenticator);

protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initializeMedia(
        const CapabilitiesResp& onvifCapabilities) override;

    virtual CameraDiagnostics::Result fetchChannelCount(bool limitedByEncoders = true) override;
private:
    CameraDiagnostics::Result fetchChannelCapabilities(
        Qn::ConnectionRole role,
        hikvision::ChannelCapabilities* outCapabilities);

    virtual QnAudioTransmitterPtr initializeTwoWayAudio() override;
    std::unique_ptr<nx_http::HttpClient> getHttpClient();
    void setResolutionList(
        const hikvision::ChannelCapabilities& channelCapabilities,
        Qn::ConnectionRole role);
private:
    std::map<Qn::ConnectionRole, hikvision::ChannelCapabilities> m_channelCapabilitiesByRole;
    bool m_hevcSupported = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF
