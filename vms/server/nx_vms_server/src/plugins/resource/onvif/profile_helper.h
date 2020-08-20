#pragma once

#ifdef ENABLE_ONVIF

#include <map>
#include <string>
#include <type_traits>

#include <nx/utils/log/assert.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <plugins/resource/onvif/soap_wrapper.h>

namespace nx::vms::server::plugins::onvif {

enum class ConfigurationType
{
    all,
    videoSource,
    videoEncoder,
    audioSource,
    audioEncoder,
    audioOutput,
    audioDecoder,
    metadata,
    analytics,
    ptz,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ConfigurationType)

QString toString(ConfigurationType configurationType);

struct ConfigurationSet
{
    using ConfigurationToken = std::string;

    std::string profileToken;
    std::map<ConfigurationType, ConfigurationToken> configurations;

    QString toString() const
    {
        return NX_FMT("{profileToken: %1, configurations: %2}",
            profileToken, containerString(configurations));
    }
};

class ProfileHelper
{

public:
    template<typename Profile>
    static std::string videoSourceToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->VideoSource)
                return profile->Configurations->VideoSource->SourceToken;
        }
        else
        {
            if (profile->VideoSourceConfiguration)
                return profile->VideoSourceConfiguration->SourceToken;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string videoSourceConfigurationToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->VideoSource)
                return profile->Configurations->VideoSource->token;
        }
        else
        {
            if (profile->VideoSourceConfiguration)
                return profile->VideoSourceConfiguration->token;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string videoEncoderConfigurationToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->VideoEncoder)
                return profile->Configurations->VideoEncoder->token;
        }
        else
        {
            if (profile->VideoEncoderConfiguration)
                return profile->VideoEncoderConfiguration->token;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string audioSourceToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->AudioSource)
                return profile->Configurations->AudioSource->SourceToken;
        }
        else
        {
            if (profile->AudioSourceConfiguration)
                return profile->AudioSourceConfiguration->SourceToken;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string audioSourceConfigurationToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->AudioSource)
                return profile->Configurations->AudioSource->token;
        }
        else
        {
            if (profile->AudioSourceConfiguration)
                return profile->AudioSourceConfiguration->token;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string audioEncoderConfigurationToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->AudioEncoder)
                return profile->Configurations->AudioEncoder->token;
        }
        else
        {
            if (profile->AudioEncoderConfiguration)
                return profile->AudioEncoderConfiguration->token;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string ptzConfigurationToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->PTZ)
                return profile->Configurations->PTZ->token;
        }
        else
        {
            if (profile->PTZConfiguration)
                return profile->PTZConfiguration->token;
        }

        return std::string();
    }

    template<typename Profile>
    static std::string metadataConfigurationToken(const Profile* profile)
    {
        if (!NX_ASSERT(profile))
            return std::string();

        if constexpr (std::is_same_v<Profile, onvifMedia2__MediaProfile>)
        {
            if (profile->Configurations && profile->Configurations->Metadata)
                return profile->Configurations->Metadata->token;
        }
        else
        {
            if (profile->MetadataConfiguration)
                return profile->MetadataConfiguration->token;
        }

        return std::string();
    }

    static CameraDiagnostics::Result addMediaConfigurations(
        const QnPlOnvifResourcePtr& device,
        const ConfigurationSet& configurationSet);

    static CameraDiagnostics::Result addMedia2Configurations(
        const QnPlOnvifResourcePtr& device,
        ConfigurationSet configurationSet);
};

} // namespace nx::vms::server::plugins::onvif

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::plugins::onvif::ConfigurationType, (lexical))

#endif // ENABLE_ONVIF
