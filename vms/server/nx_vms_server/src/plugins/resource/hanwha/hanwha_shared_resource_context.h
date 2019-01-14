#pragma once

#include <atomic>

#include <QtCore/QMap>
#include <QtCore/QElapsedTimer>

#include <nx/vms/server/resource/abstract_shared_resource_context.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <nx/utils/thread/rw_lock.h>
#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_time_synchronizer.h>
#include <plugins/resource/hanwha/hanwha_utils.h>
#include <plugins/resource/hanwha/hanwha_codec_limits.h>
#include <plugins/resource/hanwha/hanwha_chunk_reader.h>
#include <plugins/resource/hanwha/hanwha_information.h>
#include <recording/time_period_list.h>
#include <core/resource/abstract_remote_archive_manager.h>

namespace nx {
namespace vms::server {
namespace plugins {

static const std::chrono::seconds kUpdateCacheTimeout(30);
static const std::chrono::seconds kUnsuccessfulUpdateCacheTimeout(10);

template<typename Value>
struct HanwhaResult
{
    CameraDiagnostics::Result diagnostics;
    Value value;

    inline operator bool() const { return diagnostics.errorCode == CameraDiagnostics::ErrorCode::noError; }
    inline Value* operator->() { return &value; }
    inline const Value* operator->() const { return &value; }
};

template<typename Value>
class HanwhaCachedData
{
public:
    HanwhaCachedData(
        std::function<HanwhaResult<Value>()> getter,
        std::chrono::milliseconds timeout)
        :
        m_getter(std::move(getter)), m_timeout(timeout)
    {
    }

    HanwhaResult<Value> operator()()
    {
        QnMutexLocker locker(&m_mutex);
        const bool needUpdate = m_value
            ? m_timer.hasExpired(kUpdateCacheTimeout)
            : m_timer.hasExpired(kUnsuccessfulUpdateCacheTimeout);

        if (needUpdate)
        {
            m_value = m_getter();
            m_timer.restart();
        }

        return m_value;
    }

    void invalidate()
    {
        QnMutexLocker locker(&m_mutex);
        m_timer.invalidate();
    }

    std::chrono::milliseconds timeout() { return m_timeout; }

private:
    const std::function<HanwhaResult<Value>()> m_getter;
    const std::chrono::milliseconds m_timeout;

    QnMutex m_mutex;
    nx::utils::ElapsedTimer m_timer;
    HanwhaResult<Value> m_value;
};

using ClientId = QnUuid;

struct SeekPosition
{
    static const qint64 kInvalidPosition = std::numeric_limits<qint64>::min();

    SeekPosition(qint64 value = kInvalidPosition);
    bool canJoinPosition(const SeekPosition& position) const;

    nx::utils::ElapsedTimer timer;
    qint64 position = kInvalidPosition;
};

struct SessionContext
{
    SessionContext(const QString& sessionId, const QnUuid& clientId):
        sessionId(sessionId),
        clientId(clientId)
    {};

    QString sessionId;
    ClientId clientId;
    SeekPosition lastSeekPos;

    qint64 currentPositionUsec() const;
    void updateCurrentPositionUsec(
        qint64 positionUsec,
        bool isForwardPlayback,
        bool force);

private:
    mutable QnMutex m_mutex;
    qint64 m_lastPositionUsec = AV_NOPTS_VALUE;
};

using SessionContextPtr = QSharedPointer<SessionContext>;
using SessionContextWeakPtr = QWeakPointer<SessionContext>;

class HanwhaSharedResourceContext:
    public nx::vms::server::resource::AbstractSharedResourceContext,
    public std::enable_shared_from_this<HanwhaSharedResourceContext>
{
public:
    HanwhaSharedResourceContext(
        const nx::vms::server::resource::AbstractSharedResourceContext::SharedId& sharedId);

    // TODO: Better to make class HanwhaAccess and keep these fields separate from context.
    void setResourceAccess(const nx::utils::Url& url, const QAuthenticator& authenticator);
    void setLastSucessfulUrl(const nx::utils::Url& value);

    nx::utils::Url url() const;
    QAuthenticator authenticator() const;
    nx::utils::RwLock* requestLock();

    void startServices(bool hasVideoArchive, const HanwhaInformation& information);

    SessionContextPtr session(
        HanwhaSessionType sessionType,
        const QnUuid& clientId,
        bool generateNewOne = false);

    nx::core::resource::OverlappedTimePeriods overlappedTimeline(int channelNumber) const;
    nx::core::resource::OverlappedTimePeriods overlappedTimelineSync(int channelNumber) const;
    qint64 timelineStartUs(int channelNumber) const;
    qint64 timelineEndUs(int channelNumber) const;
    boost::optional<int> overlappedId() const;
    std::chrono::milliseconds timeShift() const;

    void setChunkLoaderSettings(const HanwhaChunkLoaderSettings& settings);

    // NOTE: function objects return HanwhaResult<T>.
    HanwhaCachedData<HanwhaInformation> information;
    HanwhaCachedData<HanwhaResponse> eventStatuses;
    HanwhaCachedData<HanwhaResponse> videoSources;
    HanwhaCachedData<HanwhaResponse> videoProfiles;
    HanwhaCachedData<HanwhaCodecInfo> videoCodecInfo;
    HanwhaCachedData<bool> isBypassSupported;
    HanwhaCachedData<std::set<int>> ptzCalibratedChannels;

private:
    HanwhaResult<HanwhaInformation> loadInformation();
    HanwhaResult<HanwhaResponse> loadEventStatuses();
    HanwhaResult<HanwhaResponse> loadVideoSources();
    HanwhaResult<HanwhaResponse> loadVideoProfiles();
    HanwhaResult<HanwhaCodecInfo> loadVideoCodecInfo();
    HanwhaResult<bool> checkBypassSupport();
    HanwhaResult<std::set<int>> loadPtzCalibratedChannels();

    void cleanupUnsafe();
    int totalAmountOfSessions(bool isLive) const;
private:
    static const int kDefaultNvrMaxLiveSessions = 10;
    static const int kDefaultNvrMaxArchiveSessions = 3;

    const nx::vms::server::resource::AbstractSharedResourceContext::SharedId m_sharedId;

    mutable QnMutex m_dataMutex;
    nx::utils::Url m_resourceUrl;
    QAuthenticator m_resourceAuthenticator;
    nx::utils::Url m_lastSuccessfulUrl;
    nx::utils::ElapsedTimer m_lastSuccessfulUrlTimer;

    mutable QnMutex m_sessionMutex;

    // key: live = true, archive = false
    QMap<HanwhaSessionType, QMap<ClientId, SessionContextWeakPtr>> m_sessions;

    nx::utils::RwLock m_requestLock;
    std::shared_ptr<HanwhaChunkLoader> m_chunkLoader;
    std::unique_ptr<HanwhaTimeSyncronizer> m_timeSynchronizer;

    HanwhaChunkLoaderSettings m_chunkLoaderSettings;

    // We care only about archive sesions because normally we use only one
    // live connection independently of the number of client connections.
    std::atomic<int> m_maxArchiveSessions{kDefaultNvrMaxArchiveSessions};

    mutable QnMutex m_servicesMutex;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
