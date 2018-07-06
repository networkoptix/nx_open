#pragma once

#if defined(ENABLE_ONVIF)

#include <QtCore/QFlags>

#include <boost/optional/optional.hpp>

#include <plugins/resource/onvif/onvif_resource.h>

#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class VivotekResource: public QnPlOnvifResource
{
    using base_type = QnPlOnvifResource;

public:
    enum class StreamCodecCapability
    {
        mpeg4 = 0b0001,
        mjpeg = 0b0010,
        h264 = 0b0100,
        svc = 0b1000
    };

    Q_DECLARE_FLAGS(StreamCodecCapabilities, StreamCodecCapability);

    virtual CameraDiagnostics::Result initializeMedia(
        const CapabilitiesResp& onvifCapabilities) override;

    virtual CameraDiagnostics::Result customStreamConfiguration(Qn::ConnectionRole role) override;

private:
    bool fetchHevcSupport();

    boost::optional<bool> hasHevcSupport() const;
    bool streamSupportsHevc(Qn::ConnectionRole role) const;
    bool setHevcForStream(Qn::ConnectionRole role);

    bool parseStreamCodecCapabilities(
        const QString& codecCapabilitiesString,
        std::vector<StreamCodecCapabilities>* outCapabilities) const;

    void tuneHttpClient(nx::network::http::HttpClient& httpClient) const;

    bool parseResponse(
        const nx::Buffer& response,
        QString* outName,
        QString* outValue) const;

    bool doVivotekRequest(const nx::utils::Url& url, QString* outParameterName, QString* outParameterValue) const;
    boost::optional<QString> getVivotekParameter(const QString& param) const;
    bool setVivotekParameter(const QString& parameterName, const QString& parameterValue) const;

private:
    boost::optional<bool> m_hasHevcSupport;
    std::vector<StreamCodecCapabilities> m_streamCodecCapabilities;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VivotekResource::StreamCodecCapabilities);

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // ENABLE_ONVIF
