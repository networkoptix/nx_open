#pragma once

#ifdef ENABLE_ONVIF

#include <boost/optional/optional.hpp>

#include "hikvision_request_helper.h"

#include <plugins/resource/hikvision/hikvision_utils.h>
#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HikvisionResource: public QnPlOnvifResource
{
    Q_OBJECT

    using base_type = QnPlOnvifResource;
public:
    HikvisionResource(QnMediaServerModule* serverModule);
    virtual ~HikvisionResource() override;

    virtual QString defaultCodec() const override;

    boost::optional<hikvision::ChannelCapabilities>
    channelCapabilities(Qn::ConnectionRole role);
    bool findDefaultPtzProfileToken();

    static hikvision::ProtocolStates tryToEnableIntegrationProtocols(
        const nx::utils::Url& url,
        const QAuthenticator& authenticator,
        bool isAdditionalSupportCheckNeeded = false);

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initializeMedia(
        const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities) override;
    virtual QnAbstractPtzController* createPtzControllerInternal() const override;

    virtual CameraDiagnostics::Result fetchChannelCount(bool limitedByEncoders = true) override;

private:
    CameraDiagnostics::Result fetchChannelCapabilities(
        Qn::ConnectionRole role,
        hikvision::ChannelCapabilities* outCapabilities);

    virtual QnAudioTransmitterPtr initializeTwoWayAudio() override;
    std::unique_ptr<nx::network::http::HttpClient> getHttpClient();
    void setResolutionList(
        const hikvision::ChannelCapabilities& channelCapabilities,
        Qn::ConnectionRole role);

private:
    std::map<Qn::ConnectionRole, hikvision::ChannelCapabilities> m_channelCapabilitiesByRole;
    hikvision::ProtocolStates m_integrationProtocols;
    bool m_hevcSupported = false;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif  //ENABLE_ONVIF
