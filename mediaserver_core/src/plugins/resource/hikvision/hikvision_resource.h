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

    boost::optional<hikvision::ChannelCapabilities>
    channelCapabilities(Qn::ConnectionRole role);
    bool findDefaultPtzProfileToken();

    static bool tryToEnableOnvifSupport(const QUrl& url, const QAuthenticator& authenticator);

protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        bool primaryStream) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initializeMedia(
        const CapabilitiesResp& onvifCapabilities) override;

    virtual CameraDiagnostics::Result fetchChannelCount(bool limitedByEncoders = true) override;
private:
    CameraDiagnostics::Result fetchChannelCapabilities(
        Qn::ConnectionRole role,
        hikvision::ChannelCapabilities* outCapabilities);

    CameraDiagnostics::Result initialize2WayAudio();
    std::unique_ptr<nx_http::HttpClient> getHttpClient();

private:
    std::map<Qn::ConnectionRole, hikvision::ChannelCapabilities> m_channelCapabilitiesByRole;
    bool m_hevcSupported = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF
