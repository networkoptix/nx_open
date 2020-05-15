#pragma once

#ifdef ENABLE_ONVIF

#include <string>
#include <type_traits>

#include <nx/utils/log/assert.h>

#include <plugins/resource/onvif/soap_wrapper.h>

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
};

#endif // ENABLE_ONVIF
