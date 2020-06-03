#include "profile_helper.h"

#include <nx/fusion/model_functions.h>

#include "onvif_resource.h"

namespace nx::vms::server::plugins::onvif {

QString toString(ConfigurationType configurationType)
{
    return QnLexical::serialized(configurationType);
}

template<typename ConfigurationAdder>
CameraDiagnostics::Result addMediaConfigurationInternal(
    const QnPlOnvifResourcePtr& device,
    const std::string& profileToken,
    const std::string& configurationToken)
{
    ConfigurationAdder configurationAdder(device);
    typename ConfigurationAdder::Request request;

    request.ProfileToken = profileToken;
    request.ConfigurationToken = configurationToken;

    configurationAdder.performRequest(request);

    if (!configurationAdder)
        return configurationAdder.requestFailedResult();

    return CameraDiagnostics::NoErrorResult();
}

static CameraDiagnostics::Result addMediaConfiguration(
    const QnPlOnvifResourcePtr& device,
    const std::string& profileToken,
    ConfigurationType configurationType,
    const std::string& configurationToken)
{
    switch (configurationType)
    {
        case ConfigurationType::videoSource:
            return addMediaConfigurationInternal<Media::VideoSourceConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::videoEncoder:
            return addMediaConfigurationInternal<Media::VideoEncoderConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::audioSource:
            return addMediaConfigurationInternal<Media::AudioSourceConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::audioEncoder:
            return addMediaConfigurationInternal<Media::AudioEncoderConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::audioOutput:
            return addMediaConfigurationInternal<Media::AudioOutputConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::audioDecoder:
            return addMediaConfigurationInternal<Media::AudioDecoderConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::metadata:
            return addMediaConfigurationInternal<Media::MetadataConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::analytics:
            return addMediaConfigurationInternal<Media::VideoAnalyticsConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::ptz:
            return addMediaConfigurationInternal<Media::PtzConfigurationAdder>(
                device, profileToken, configurationToken);
        case ConfigurationType::all:
            return CameraDiagnostics::InternalServerErrorResult(
                NX_FMT("Configuration type 'All' is not supported"));
    }

    NX_ASSERT(false, "Device %1, unsupported configuration type: %2",
        device, configurationType);

    return CameraDiagnostics::InternalServerErrorResult(
        NX_FMT("Got unexpected configuration type %1", configurationType));
}

/*static*/
CameraDiagnostics::Result ProfileHelper::addMediaConfigurations(
    const QnPlOnvifResourcePtr& device,
    const ConfigurationSet& configurationSet)
{
    for (const auto& [configurationType, configurationToken]: configurationSet.configurations)
    {
        CameraDiagnostics::Result result = addMediaConfiguration(
            device, configurationSet.profileToken, configurationType, configurationToken);

        if (!result)
        {
            NX_DEBUG(NX_SCOPE_TAG,
                "Device %1, failed to add configuration of type %2, profile token %3, "
                "configuration token %4, error: %5",
                device, configurationType, configurationSet.profileToken, configurationToken,
                result.toString(device->commonModule()->resourcePool()));

            if (configurationType != ConfigurationType::audioEncoder //< Ignore audio errors.
                && configurationType != ConfigurationType::audioSource)
            {
                return result;
            }
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

/*static*/
CameraDiagnostics::Result ProfileHelper::addMedia2Configurations(
    const QnPlOnvifResourcePtr& device,
    ConfigurationSet configurationSet)
{
    NX_DEBUG(NX_SCOPE_TAG, "Device %1, adding a set of configurations: %2",
        device, configurationSet);

    Media2::ConfigurationAdder configurationAdder(device);

    std::vector<onvifMedia2__ConfigurationRef> configurationRefList;
    for (auto& [configurationType, configurationToken]: configurationSet.configurations)
    {
        onvifMedia2__ConfigurationRef configurationRef;
        configurationRef.Type = QnLexical::serialized(configurationType).toStdString();
        configurationRef.Token = &configurationToken;
        configurationRefList.push_back(configurationRef);
    }

    typename Media2::ConfigurationAdder::Request request;
    request.ProfileToken = configurationSet.profileToken;

    for (onvifMedia2__ConfigurationRef& configurationRef: configurationRefList)
        request.Configuration.push_back(&configurationRef);

    if (!configurationAdder.performRequest(request))
    {
        const CameraDiagnostics::Result result = configurationAdder.requestFailedResult();
        NX_DEBUG(NX_SCOPE_TAG, "Device %1, request 'AddConfiguration' failed, error %2",
            device, result.toString(device->commonModule()->resourcePool()));
        return configurationAdder.requestFailedResult();
    }

    return CameraDiagnostics::NoErrorResult();
}

} // namespace nx::vms::server::plugins::onvif

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::plugins::onvif, ConfigurationType,
    (nx::vms::server::plugins::onvif::ConfigurationType::all, "All")
    (nx::vms::server::plugins::onvif::ConfigurationType::videoSource, "VideoSource")
    (nx::vms::server::plugins::onvif::ConfigurationType::videoEncoder, "VideoEncoder")
    (nx::vms::server::plugins::onvif::ConfigurationType::audioSource, "AudioSource")
    (nx::vms::server::plugins::onvif::ConfigurationType::audioEncoder, "AudioEncoder")
    (nx::vms::server::plugins::onvif::ConfigurationType::audioOutput, "AudioOutput")
    (nx::vms::server::plugins::onvif::ConfigurationType::audioDecoder, "AudioDecoder")
    (nx::vms::server::plugins::onvif::ConfigurationType::metadata, "Metadata")
    (nx::vms::server::plugins::onvif::ConfigurationType::analytics, "Analytics")
    (nx::vms::server::plugins::onvif::ConfigurationType::ptz, "PTZ"))
