#include "onvif_multicast_parameters_proxy.h"

#include <optional>
#include <type_traits>

#include <plugins/resource/onvif/onvif_resource.h>
#include <core/resource/camera_advanced_param.h>

namespace nx::vms::server::resource {

namespace {

onvifXsd__VideoEncoderConfiguration* getVideoEncoderConfigurationViaMedia1(
    std::string videoEncoderConfigurationToken,
    Media::VideoEncoderConfiguration* request)
{
    _onvifMedia__GetVideoEncoderConfiguration videoEncoderConfigurationRequest;
    videoEncoderConfigurationRequest.ConfigurationToken = videoEncoderConfigurationToken;

    if (!request->receiveBySoap(videoEncoderConfigurationRequest))
        return nullptr;

    return request->get()->Configuration;
}

onvifXsd__VideoEncoder2Configuration* getVideoEncoderConfigurationViaMedia2(
    std::string videoEncoderConfigurationToken,
    Media2::VideoEncoderConfigurations* request)
{
    onvifMedia2__GetConfiguration videoEncoderConfigurationRequest2;
    videoEncoderConfigurationRequest2.ConfigurationToken = &videoEncoderConfigurationToken;

    if (!request->receiveBySoap(videoEncoderConfigurationRequest2))
        return nullptr;

    const auto response = request->get();
    if (response->Configurations.empty())
        return nullptr;

    return response->Configurations[0];
}

template<typename Configuration>
MulticastParameters multicastParameters(
    const Configuration* videoEncoderConfiguration)
{
    if (!NX_ASSERT(videoEncoderConfiguration))
        return {};

    const auto multicastConfiguration = videoEncoderConfiguration->Multicast;
    if (!multicastConfiguration)
        return {};

    if (!multicastConfiguration->Address)
        return {};

    if (!multicastConfiguration->Address->IPv4Address)
        return {};

    MulticastParameters parameters;

    if constexpr (std::is_pointer<decltype(multicastConfiguration->Address->IPv4Address)>::value)
        parameters.address = *multicastConfiguration->Address->IPv4Address;
    else
        parameters.address = multicastConfiguration->Address->IPv4Address;

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
    {
        if constexpr (std::is_pointer<decltype(multicastConfiguration->Address->IPv4Address)>::value)
            multicastConfiguration->Address->IPv4Address = &*multicastParameters.address;
        else
            multicastConfiguration->Address->IPv4Address = multicastParameters.address;
    }

    if (multicastParameters.port)
        multicastConfiguration->Port = *multicastParameters.port;

    if (multicastParameters.ttl)
        multicastConfiguration->TTL = *multicastParameters.ttl;
}

MulticastParameters getMulticastParametersViaMedia1(
    std::string videoEncoderConfigurationToken,
    QnPlOnvifResource* resource)
{
    Media::VideoEncoderConfiguration media(resource);
    const auto configuration = getVideoEncoderConfigurationViaMedia1(
        videoEncoderConfigurationToken, &media);

    if (!configuration)
        return {};

    return multicastParameters(configuration);
}

MulticastParameters getMulticastParametersViaMedia2(
    std::string videoEncoderConfigurationToken,
    QnPlOnvifResource* resource)
{
    Media2::VideoEncoderConfigurations media2(resource);
    const auto configuration = getVideoEncoderConfigurationViaMedia2(
        videoEncoderConfigurationToken, &media2);

    if (!configuration)
        return {};

    return multicastParameters(configuration);
}

bool setVideoEncoderConfigurationViaMedia1(
    onvifXsd__VideoEncoderConfiguration* configuration,
    QnPlOnvifResource* resource)
{
    Media::VideoEncoderConfigurationSetter media(resource);
    _onvifMedia__SetVideoEncoderConfiguration request;
    request.Configuration = configuration;
    return media.performRequest(request);
}

bool setVideoEncoderConfigurationViaMedia2(
    onvifXsd__VideoEncoder2Configuration* configuration,
    QnPlOnvifResource* resource)
{
    Media2::VideoEncoderConfigurationSetter media2(resource);
    _onvifMedia2__SetVideoEncoderConfiguration request;
    request.Configuration = configuration;
    return media2.performRequest(request);
}

} // namespace

OnvifMulticastParametersProxy::OnvifMulticastParametersProxy(
    QnPlOnvifResource* onvifResource,
    nx::vms::api::StreamIndex streamIndex)
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

    if (m_resource->getMedia2Url().isEmpty())
        return getMulticastParametersViaMedia1(token, m_resource);

    return getMulticastParametersViaMedia2(token, m_resource);
}

bool OnvifMulticastParametersProxy::setMulticastParameters(MulticastParameters parameters)
{
    std::string token = m_resource->videoEncoderConfigurationToken(m_streamIndex);
    if (token.empty())
        return false;

    if (m_resource->getMedia2Url().isEmpty())
    {
        Media::VideoEncoderConfiguration media(m_resource);
        const auto configuration = getVideoEncoderConfigurationViaMedia1(token, &media);
        if (!configuration)
            return false;

        updateConfigurationWithMulticastParameters(configuration, parameters);
        return setVideoEncoderConfigurationViaMedia1(configuration, m_resource);
    }

    Media2::VideoEncoderConfigurations media2(m_resource);
    const auto configuration = getVideoEncoderConfigurationViaMedia2(token, &media2);
    if (!configuration)
        return false;

    updateConfigurationWithMulticastParameters(configuration, parameters);
    return setVideoEncoderConfigurationViaMedia2(configuration, m_resource);
}

} // namespace nx::vms::server::resource
