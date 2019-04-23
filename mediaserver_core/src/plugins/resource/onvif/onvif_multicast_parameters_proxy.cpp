#include "onvif_multicast_parameters_proxy.h"

#include <optional>
#include <type_traits>

#include <plugins/resource/onvif/onvif_resource.h>
#include <core/resource/camera_advanced_param.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

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
    return lm("address: %1. port: %2, ttl: %3").args(
        address ? *address : "",
        port ? *port : 0,
        ttl ? *ttl : 0);
}

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
    if (!setVideoEncoderConfigurationViaMedia1(configuration, &media, m_resource))
        return false;

    // NOTE: Configuring multicast for audio too, 'cause some cameras have separate multicast
    // address for audio. We does not care here if setAudioEncoder fails' cause it most probably
    // will not affect behavior of most of the cameras.
    setAudioEncoderMulticastParameters(parameters);

    NX_DEBUG(this, lm("Camera [%1], change multicast parameters, reopen stream: %2")
        .args(m_resource->getId(), QnLexical::serialized(m_streamIndex)));
    m_resource->reopenStream(m_streamIndex);
    return true;
}

bool OnvifMulticastParametersProxy::setAudioEncoderMulticastParameters(
    MulticastParameters &parameters)
{
    std::string audioToken = m_resource->audioEncoderConfigurationToken(m_streamIndex);
    if (audioToken.empty())
    {
        NX_VERBOSE(this, "Skipping audio encoder configuration: no audio token");
        return false;
    }

    const auto auth = m_resource->getAuth();
    MediaSoapWrapper media(
        m_resource->getMediaUrl().toStdString().c_str(),
        auth.user(),
        auth.password(),
        m_resource->getTimeDrift());

    auto audioConfiguration = getAudioEncoderConfiguration(audioToken, &media);
    if (!audioConfiguration)
    {
        NX_VERBOSE(this,
            lm("Skipping audio encoder configuration: can't get audio configuration (%1)")
                .args(media.getLastError()));
        return false;
    }

    MediaSoapWrapper mediaSet(
        m_resource->getMediaUrl().toStdString().c_str(),
        auth.user(),
        auth.password(),
        m_resource->getTimeDrift());
    updateConfigurationWithMulticastParameters(audioConfiguration, parameters);
    if (!setAudioEncoderConfiguration(audioConfiguration, &mediaSet))
    {
        NX_VERBOSE(this,
            lm("Skipping audio encoder configuration: can't set audio configuration (%1)")
                .args(media.getLastError()));
        return false;
    }
    return true;
}

} // namespace resource
} // namespace server
} // namespace vms
} // namespace nx
