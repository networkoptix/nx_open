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
    virtual ~HanwhaStreamReader() override;

    void setPositionUsec(qint64 value);
    void setSessionType(HanwhaSessionType value);
    void setClientId(const QString& id);
    void setRateControlEnabled(bool enabled);
    void setPlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec);
    void setOverlappedId(int overlappedId);

protected:
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

    friend class HanwhaArchiveDelegate;
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

    QString toHanwhaPlaybackTime(int64_t timestamp) const;

private:
    HanwhaResourcePtr m_hanwhaResource;
    bool m_rateControlEnabled = true;
    HanwhaSessionType m_sessionType = HanwhaSessionType::live;
    QString m_clientId;
    int64_t m_startTimeUsec = 0;
    int64_t m_endTimeUsec = 0;
    int m_overlappedId = 0;
    SessionGuard m_sessionGuard;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
