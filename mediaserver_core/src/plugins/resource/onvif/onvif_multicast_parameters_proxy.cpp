#include "onvif_multicast_parameters_proxy.h"

#include <optional>
#include <type_traits>

#include <plugins/resource/onvif/onvif_resource.h>
#include <core/resource/camera_advanced_param.h>

namespace nx {
namespace vms {
namespace server {
namespace resource {

namespace {

bool getVideoEncoderConfigurationViaMedia1(
    std::string videoEncoderConfigurationToken,
    MediaSoapWrapper* soapWrapper,
    _onvifMedia__GetVideoEncoderConfigurationResponse* outConfigurationResponse)
{
    _onvifMedia__GetVideoEncoderConfiguration videoEncoderConfigurationRequest;
    videoEncoderConfigurationRequest.ConfigurationToken = videoEncoderConfigurationToken;

    return soapWrapper->getVideoEncoderConfiguration(
        videoEncoderConfigurationRequest,
        *outConfigurationResponse) == SOAP_OK;
}

template<typename Configuration>
MulticastParameters multicastParameters(const Configuration* videoEncoderConfiguration)
{
    if (!videoEncoderConfiguration)
    {
        NX_ASSERT(false, "Video encoder configuration can't be null");
        return {};
    }

    const auto multicastConfiguration = videoEncoderConfiguration->Multicast;
    if (!multicastConfiguration)
        return {};

    if (!multicastConfiguration->Address)
        return {};

    if (!multicastConfiguration->Address->IPv4Address)
        return {};

    MulticastParameters parameters;

    parameters.address = *multicastConfiguration->Address->IPv4Address;
    parameters.port = multicastConfiguration->Port;
    parameters.ttl = multicastConfiguration->TTL;

    return parameters;
}

template<typename Configuration>
void updateConfigurationWithMulticastParameters(
    Configuration* inOutVideoEncoderConfiguration,
    MulticastParameters& multicastParameters)
{
    const auto multicastConfiguration = inOutVideoEncoderConfiguration->Multicast;

    if (multicastParameters.address)
        multicastConfiguration->Address->IPv4Address = &*multicastParameters.address;

    if (multicastParameters.port)
        multicastConfiguration->Port = *multicastParameters.port;

    if (multicastParameters.ttl)
        multicastConfiguration->TTL = *multicastParameters.ttl;
}

MulticastParameters getMulticastParametersViaMedia1(
    std::string videoEncoderConfigurationToken,
    QnPlOnvifResource* resource)
{
    const auto auth = resource->getAuth();
    MediaSoapWrapper media(
        resource->getMediaUrl().toStdString().c_str(),
        auth.user(),
        auth.password(),
        resource->getTimeDrift());

    _onvifMedia__GetVideoEncoderConfigurationResponse configurationResponse;
    const bool result = getVideoEncoderConfigurationViaMedia1(
        videoEncoderConfigurationToken,
        &media,
        &configurationResponse);

    if (!result)
        return {};

    return multicastParameters(configurationResponse.Configuration);
}

bool setVideoEncoderConfigurationViaMedia1(
    onvifXsd__VideoEncoderConfiguration* configuration,
    MediaSoapWrapper* soapWrapper,
    QnPlOnvifResource* resource)
{
    const auto auth = resource->getAuth();
    MediaSoapWrapper media(
        resource->getMediaUrl().toStdString().c_str(),
        auth.user(),
        auth.password(),
        resource->getTimeDrift());

    _onvifMedia__SetVideoEncoderConfiguration request;
    _onvifMedia__SetVideoEncoderConfigurationResponse response;
    request.Configuration = configuration;
    const auto result = media.setVideoEncoderConfiguration(request, response);
    return result == SOAP_OK;
}

} // namespace

OnvifMulticastParametersProxy::OnvifMulticastParametersProxy(
    QnPlOnvifResource* onvifResource,
    Qn::StreamIndex streamIndex)
    :
    m_resource(onvifResource),
    m_streamIndex(streamIndex)
{
}

MulticastParameters OnvifMulticastParametersProxy::multicastParameters()
{
    std::string token = m_resource->videoEncoderConfigurationToken(m_streamIndex);
    if (token.empty())
        return MulticastParameters();

    return getMulticastParametersViaMedia1(token, m_resource);
}

bool OnvifMulticastParametersProxy::setMulticastParameters(MulticastParameters parameters)
{
    std::string token = m_resource->videoEncoderConfigurationToken(m_streamIndex);
    if (token.empty())
        return false;

    const auto auth = m_resource->getAuth();
    MediaSoapWrapper media(
        m_resource->getMediaUrl().toStdString().c_str(),
        auth.user(),
        auth.password(),
        m_resource->getTimeDrift());

    _onvifMedia__GetVideoEncoderConfigurationResponse configurationResponse;
    const auto result = getVideoEncoderConfigurationViaMedia1(
        token,
        &media,
        &configurationResponse);

    if (!result)
        return false;

    auto configuration = configurationResponse.Configuration;
    updateConfigurationWithMulticastParameters(configuration, parameters);
    return setVideoEncoderConfigurationViaMedia1(configuration, &media, m_resource);
}

} // namespace resource
} // namespace server
} // namespace vms
} // namespace nx
