#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/abstract_remote_archive_manager.h>

#include <streaming/rtp_stream_reader.h>
#include <plugins/resource/hanwha/hanwha_video_profile.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_utils.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaStreamReader: public QnRtpStreamReader
{
    using base_type = QnRtpStreamReader;
public:
    HanwhaStreamReader(const HanwhaResourcePtr& res);
    virtual ~HanwhaStreamReader() override;

    void setPositionUsec(qint64 value);
    void setSessionType(HanwhaSessionType value);
    void setClientId(const QnUuid& id);
    void setRateControlEnabled(bool enabled);
    void setPlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec);
    void setOverlappedId(nx::core::resource::OverlappedId overlappedId);

    SessionContextPtr sessionContext();
protected:
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

    virtual void closeStream() override;
    virtual QnAbstractMediaDataPtr getNextData() override;

    friend class HanwhaArchiveDelegate;
private:
    CameraDiagnostics::Result updateProfile(const QnLiveStreamParams& parameters);

    bool isCorrectProfile(int profileNumber) const;

    CameraDiagnostics::Result streamUri(QString* outUrl);

    QString rtpTransport() const;
    QnRtspClient& rtspClient();

    QnAbstractMediaDataPtr createEmptyPacket();
    QString forcedUrl(Qn::ConnectionRole role) const;
private:
    HanwhaResourcePtr m_hanwhaResource;
    bool m_rateControlEnabled = true;
    HanwhaSessionType m_sessionType = HanwhaSessionType::live;
    QnUuid m_clientId;
    SessionContextPtr m_sessionContext;
    int64_t m_startTimeUsec = 0;
    int64_t m_endTimeUsec = 0;
    qint64 m_lastTimestampUsec = AV_NOPTS_VALUE;
    nx::utils::ElapsedTimer m_timeSinceLastFrame;
    boost::optional<nx::core::resource::OverlappedId> m_overlappedId;
    HanwhaProfileParameters m_prevProfileParameters;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
