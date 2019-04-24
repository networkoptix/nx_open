#include "onvif_multicast_parameters_proxy.h"

#include <optional>
#include <type_traits>

#include <plugins/resource/onvif/onvif_resource.h>
#include <core/resource/camera_advanced_param.h>

namespace nx::vms::server::resource {

namespace {

onvifXsd__VideoEncoderConfiguration* getVideoEncoderConfigurationViaMedia1(
    std::string token, Media::VideoEncoderConfiguration* request)
{
    _onvifMedia__GetVideoEncoderConfiguration videoEncoderConfigurationRequest;
    videoEncoderConfigurationRequest.ConfigurationToken = std::move(token);

    if (!request->receiveBySoap(videoEncoderConfigurationRequest))
        return nullptr;

    return request->get()->Configuration;
}

onvifXsd__VideoEncoder2Configuration* getVideoEncoderConfigurationViaMedia2(
    std::string token, Media2::VideoEncoderConfigurations* request)
{
    onvifMedia2__GetConfiguration videoEncoderConfigurationRequest2;
    videoEncoderConfigurationRequest2.ConfigurationToken = &token;

    if (!request->receiveBySoap(videoEncoderConfigurationRequest2))
        return nullptr;

    const auto response = request->get();
    if (response->Configurations.empty())
        return nullptr;

    return response->Configurations[0];
}

onvifXsd__AudioEncoderConfiguration* getAudioEncoderConfiguration(
    std::string token, MediaSoapWrapper* soapWrapper)
{
    AudioConfigResp response;
    AudioConfigReq request;
    request.ConfigurationToken = std::move(token);
    int result = soapWrapper->getAudioEncoderConfiguration(request, response);
    if (result != SOAP_OK || !response.Configuration)
        return nullptr;
    return response.Configuration;
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
    Configuration* inOutEncoderConfiguration,
    MulticastParameters& multicastParameters)
{
    const auto multicastConfiguration = inOutEncoderConfiguration->Multicast;

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

MulticastParameters getMulticastParametersViaMedia1(std::string token, QnPlOnvifResource* resource)
{
    Media::VideoEncoderConfiguration media(resource);
    const auto configuration = getVideoEncoderConfigurationViaMedia1(token, &media);

    if (!configuration)
        return {};

    return multicastParameters(configuration);
}

MulticastParameters getMulticastParametersViaMedia2(std::string token, QnPlOnvifResource* resource)
{
    Media2::VideoEncoderConfigurations media2(resource);
    const auto configuration = getVideoEncoderConfigurationViaMedia2(token, &media2);

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

bool setAudioEncoderConfiguration(
    onvifXsd__AudioEncoderConfiguration* configuration, MediaSoapWrapper* soapWrapper)
{
    SetAudioConfigResp response;
    SetAudioConfigReq request;
    request.Configuration = configuration;
    int result = soapWrapper->setAudioEncoderConfiguration(request, response);
    return result == SOAP_OK;
}

} // namespace

QString MulticastParameters::toString() const
{
    return lm("address: %1. port: %2, ttl: %3").args(address, port, ttl);
}

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
    if (m_streamIndex == nx::vms::api::StreamIndex::secondary)
    {
        // secondary stream may absent
        return MulticastParameters();
    }
    else
    {
        // primary stream may not absent
        if (!NX_ASSERT(!token.empty()))
            return MulticastParameters();
    }

    if (m_resource->getMedia2Url().isEmpty())
        return getMulticastParametersViaMedia1(token, m_resource);

    return getMulticastParametersViaMedia2(token, m_resource);
}

bool OnvifMulticastParametersProxy::setMulticastParameters(MulticastParameters parameters)
{
    NX_VERBOSE(this, "Setting multicast parameters to [%1]", parameters);

    if (!setVideoEncoderMulticastParameters(parameters))
        return false;

    // NOTE: Configuring multicast for audio too, 'cause some cameras have separate multicast
    // address for audio. We does not care here if setAudioEncoder fails' cause it most probably
    // will not affect behavior of most of the cameras.
    setAudioEncoderMulticastParameters(parameters);
    return true;
}

bool OnvifMulticastParametersProxy::setVideoEncoderMulticastParameters(
    MulticastParameters& parameters)
{
    std::string token = m_resource->videoEncoderConfigurationToken(m_streamIndex);
    if (!NX_ASSERT(!token.empty()))
        return false;

    const auto errorFormatString =
        "Error setting multicast parameters: can't %1 video encoder configuration (%2)";
    if (m_resource->getMedia2Url().isEmpty())
    {
        Media::VideoEncoderConfiguration media(m_resource);
        const auto configuration = getVideoEncoderConfigurationViaMedia1(token, &media);
        if (!configuration)
        {
            NX_DEBUG(this, errorFormatString, "get", media.soapErrorAsString());
            return false;
        }

        updateConfigurationWithMulticastParameters(configuration, parameters);
        if (!setVideoEncoderConfigurationViaMedia1(configuration, m_resource))
        {
            NX_DEBUG(this, errorFormatString, "set", media.soapErrorAsString());
            return false;
        }
        return true;
    }

    Media2::VideoEncoderConfigurations media2(m_resource);
    const auto configuration = getVideoEncoderConfigurationViaMedia2(token, &media2);
    if (!configuration)
    {
        NX_DEBUG(this, errorFormatString, "get", media2.soapErrorAsString());
        return false;
    }

    updateConfigurationWithMulticastParameters(configuration, parameters);
    if (!setVideoEncoderConfigurationViaMedia2(configuration, m_resource))
    {
        NX_DEBUG(this, errorFormatString, "set", media2.soapErrorAsString());
        return false;
    }

    NX_DEBUG(this, "Camera [%1], change multicast parameters, reopen stream: %2",
        m_resource->getId(), m_streamIndex);
    m_resource->reopenStream(m_streamIndex);
    return true;
}

bool OnvifMulticastParametersProxy::setAudioEncoderMulticastParameters(
    MulticastParameters &parameters)
{
    std::string audioToken = m_resource->audioEncoderConfigurationToken();
    if (audioToken.empty())
    {
        NX_VERBOSE(this, "Skipping audio encoder configuration: no audio token");
        return false;
    }

    MediaSoapWrapper getSoapWrapper(m_resource);
    auto audioConfiguration = getAudioEncoderConfiguration(audioToken, &getSoapWrapper);
    if (!audioConfiguration)
    {
        NX_VERBOSE(this, "Skipping audio encoder configuration: can't get audio configuration (%1)",
            getSoapWrapper.getLastErrorDescription());
        return false;
    }

    MediaSoapWrapper setSoapWrapper(m_resource);
    updateConfigurationWithMulticastParameters(audioConfiguration, parameters);
    if (!setAudioEncoderConfiguration(audioConfiguration, &setSoapWrapper))
    {
        NX_VERBOSE(this, "Skipping audio encoder configuration: can't set audio configuration (%1)",
            setSoapWrapper.getLastErrorDescription());
        return false;
    }
    return true;
}

} // namespace nx::vms::server::resource
