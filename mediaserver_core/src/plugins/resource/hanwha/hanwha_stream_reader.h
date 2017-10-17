#pragma once

#if defined(ENABLE_HANWHA)

#include <core/resource/resource_fwd.h>

#include <plugins/resource/onvif/dataprovider/rtp_stream_provider.h>
#include <plugins/resource/hanwha/hanwha_video_profile.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_utils.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaStreamReader: public QnRtpStreamReader
{
    
public:
    HanwhaStreamReader(const HanwhaResourcePtr& res);

    void setPositionUsec(qint64 value);
    void setSessionType(HanwhaSessionType value);
    virtual ~HanwhaStreamReader() override;
protected: 
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;
    
    friend class HanwhaNvrArchiveDelegate;
private:
    HanwhaProfileParameters makeProfileParameters(
        int profileNumber,
        const QnLiveStreamParams& parameters) const;

    CameraDiagnostics::Result updateProfile(
        int profileNumber,
        const QnLiveStreamParams& parameters);

    QSet<int> availableProfiles(int channel) const;
    int chooseNvrChannelProfile(Qn::ConnectionRole role) const;
    bool isCorrectProfile(int profileNumber) const;

    CameraDiagnostics::Result streamUri(int profileNumber, QString* outUrl);

    QString rtpTransport() const;
    QnRtspClient& rtspClient();
private:
    HanwhaResourcePtr m_hanwhaResource;
    HanwhaSessionType m_sessionType = HanwhaSessionType::live;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
