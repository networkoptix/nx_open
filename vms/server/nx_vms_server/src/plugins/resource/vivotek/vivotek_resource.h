#pragma once

#if defined(ENABLE_ONVIF)

#include <QtCore/QFlags>

#include <boost/optional/optional.hpp>

#include <plugins/resource/onvif/onvif_resource.h>

#include <nx/network/http/http_client.h>

namespace nx {
namespace vms::server {
namespace plugins {

class VivotekResource: public QnPlOnvifResource
{
    using base_type = QnPlOnvifResource;

public:

    VivotekResource(QnMediaServerModule* serverModule);

    enum class StreamCodecCapability
    {
        mpeg4 = 0b0001,
        mjpeg = 0b0010,
        h264 = 0b0100,
        svc = 0b1000
    };

    Q_DECLARE_FLAGS(StreamCodecCapabilities, StreamCodecCapability);

    virtual QString defaultCodec() const override;

    virtual CameraDiagnostics::Result initializeMedia(
        const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities) override;

    virtual CameraDiagnostics::Result customStreamConfiguration(
        Qn::ConnectionRole role,
        const QnLiveStreamParams& params) override;

    CameraDiagnostics::Result setVivotekParameter(
        const QString& parameterName, const QString& parameterValue, bool isPrimary) const;
    boost::optional<QString> getVivotekParameter(const QString& param) const;
protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
        StreamIndex streamIndex) override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

private:
    bool fetchHevcSupport();

    boost::optional<bool> hasHevcSupport() const;
    bool streamSupportsHevc(Qn::ConnectionRole role) const;
    CameraDiagnostics::Result setHevcForStream(Qn::ConnectionRole role);

    bool parseStreamCodecCapabilities(
        const QString& codecCapabilitiesString,
        std::vector<StreamCodecCapabilities>* outCapabilities) const;

    void tuneHttpClient(nx::network::http::HttpClient& httpClient) const;

    bool parseResponse(
        const nx::Buffer& response,
        QString* outName,
        QString* outValue) const;

    bool doVivotekRequest(const nx::utils::Url& url, QString* outParameterName, QString* outParameterValue) const;

private:
    boost::optional<bool> m_hasHevcSupport;
    std::vector<StreamCodecCapabilities> m_streamCodecCapabilities;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VivotekResource::StreamCodecCapabilities);

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif // ENABLE_ONVIF
