#pragma once

#if defined(ENABLE_HANWHA)

#include <core/resource/resource_fwd.h>

#include <plugins/resource/onvif/dataprovider/rtp_stream_provider.h>
#include <plugins/resource/hanwha/hanwha_video_profile.h>
#include <plugins/resource/hanwha/hanwha_response.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaStreamReader: public QnRtpStreamReader
{
    using ProfileNumber = int;
    using ProfileMap = std::map<ProfileNumber, HanwhaVideoProfile>;

    using ProfileParameterName = QString;
    using ProfileParameterValue = QString;
    using ProfileParameters = std::map<QString, QString>;

public:
    HanwhaStreamReader(const HanwhaResourcePtr& res);

    virtual ~HanwhaStreamReader() override;

protected: 
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

private:
    ProfileMap parseProfiles(const HanwhaResponse& response) const;

    QString nxProfileName() const;

    bool isNxProfile(const QString& profileName) const;

    ProfileParameters makeProfileParameters(
        int profileNumber,
        const QnLiveStreamParams& parameters) const;

    CameraDiagnostics::Result findProfile(
        int* outProfileNumber,
        int* profileToRemoveIfProfilesExhausted);

    CameraDiagnostics::Result createProfile(int* outProfileNumber);

    CameraDiagnostics::Result updateProfile(
        int profileNumber,
        const QnLiveStreamParams& parameters);

    CameraDiagnostics::Result removeProfile(int profileNumber);

    CameraDiagnostics::Result streamUri(int profileNumber, QString* outUrl);

    QString rtpTransport() const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
