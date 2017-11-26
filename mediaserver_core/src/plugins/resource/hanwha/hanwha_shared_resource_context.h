#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QMap>
#include <QtCore/QElapsedTimer>

#include <nx/mediaserver/resource/abstract_shared_resource_context.h>
#include <nx/mediaserver/server_module_aware.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_time_synchronizer.h>
#include <plugins/resource/hanwha/hanwha_utils.h>
#include <recording/time_period_list.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

static const std::chrono::seconds kUpdateCacheTimeout(30);
static const std::chrono::seconds kUnsuccessfulUpdateCacheTimeout(10);

class HanwhaChunkLoader;

struct HanwhaInformation
{
    QString deviceType;
    QString firmware;
    QString macAddress;
    QString model;
    int channelCount = 0;
    HanwhaAttributes attributes;
};

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

private:
    const std::function<HanwhaResult<Value>()> m_getter;
    const std::chrono::milliseconds m_timeout;

    QnMutex m_mutex;
    nx::utils::ElapsedTimer m_timer;
    HanwhaResult<Value> m_value;
};

using ClientId = QnUuid;

struct SessionContext
{
    SessionContext(const QString& sessionId, const QnUuid& clientId):
        sessionId(sessionId),
        clientId(clientId)
    {};

    QString sessionId;
    ClientId clientId;
};

using SessionContextPtr = QSharedPointer<SessionContext>;
using SessionContextWeakPtr = QWeakPointer<SessionContext>;

class HanwhaSharedResourceContext:
    public nx::mediaserver::resource::AbstractSharedResourceContext,
    public std::enable_shared_from_this<HanwhaSharedResourceContext>
{

public:
    HanwhaSharedResourceContext(
        const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId& sharedId);

    // TODO: Better to make class HanwhaAccess and keep these fields separate from context.
    void setRecourceAccess(const QUrl& url, const QAuthenticator& authenticator);
    void setLastSucessfulUrl(const QUrl& value);

    QUrl url() const;
    QAuthenticator authenticator() const;
    QnSemaphore* requestSemaphore();

    void startServices(bool hasVideoArchive);

    SessionContextPtr session(
        HanwhaSessionType sessionType,
        const QnUuid& clientId,
        bool generateNewOne = false);

    QnTimePeriodList chunks(int channelNumber) const;
    QnTimePeriodList chunksSync(int channelNumber) const;
    qint64 chunksStartUsec(int channelNumber) const;
    qint64 chunksEndUsec(int channelNumber) const;
    boost::optional<int> overlappedId() const;

    std::chrono::seconds timeZoneShift() const;
    void setDateTime(const QDateTime& dateTime);

    // NOTE: function objects return HanwhaResult<T>.
    HanwhaCachedData<HanwhaInformation> information;
    HanwhaCachedData<HanwhaCgiParameters> cgiParamiters;
    HanwhaCachedData<HanwhaResponse> eventStatuses;
    HanwhaCachedData<HanwhaResponse> videoSources;
    HanwhaCachedData<HanwhaResponse> videoProfiles;

private:
    HanwhaResult<HanwhaInformation> loadInformation();
    HanwhaResult<HanwhaCgiParameters> loadCgiParamiters();
    HanwhaResult<HanwhaResponse> loadEventStatuses();
    HanwhaResult<HanwhaResponse> loadVideoSources();
    HanwhaResult<HanwhaResponse> loadVideoProfiles();

    void cleanupUnsafe();

    const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId m_sharedId;

    mutable QnMutex m_dataMutex;
    QUrl m_resourceUrl;
    QAuthenticator m_resourceAuthenticator;
    QUrl m_lastSuccessfulUrl;
    nx::utils::ElapsedTimer m_lastSuccessfulUrlTimer;

    mutable QnMutex m_sessionMutex;

    QMap<HanwhaSessionType, QMap<ClientId, SessionContextWeakPtr>> m_sessions;

    QnSemaphore m_requestSemaphore;
    std::shared_ptr<HanwhaChunkLoader> m_chunkLoader;
    std::unique_ptr<HanwhaTimeSyncronizer> m_timeSynchronizer;

    std::atomic<std::chrono::seconds> m_timeZoneShift{std::chrono::seconds::zero()};

    mutable QnMutex m_servicesMutex;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
