#pragma once

#ifdef ENABLE_ONVIF

#include <boost/optional/optional.hpp>

#include <plugins/resource/hikvision/hikvision_parsing_utils.h>
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
    virtual QnAudioTransmitterPtr getAudioTransmitter() override;

    boost::optional<hikvision::ChannelCapabilities>
    channelCapabilities(Qn::ConnectionRole role);

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual CameraDiagnostics::Result initializeMedia(
        const CapabilitiesResp& onvifCapabilities) override;
private:
    bool fetchChannelCapabilities(
        Qn::ConnectionRole role,
        hikvision::ChannelCapabilities* outCapabilities);
    CameraDiagnostics::Result initialize2WayAudio();
    std::unique_ptr<nx_http::HttpClient> getHttpClient();

private:
    QnAudioTransmitterPtr m_audioTransmitter;
    std::map<Qn::ConnectionRole, hikvision::ChannelCapabilities> m_channelCapabilitiesByRole;
    bool m_hevcSupported = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF
