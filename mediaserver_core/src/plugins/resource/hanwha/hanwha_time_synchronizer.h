#pragma once

#if defined(ENABLE_HANWHA)

#include "hanwha_request_helper.h"

#include <nx/network/http/http_async_client.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaResource;

/**
 * Synchronizes camera/NVR time with qSyncTime and keeps time zone shift updated.
 */
class HanwhaTimeSyncronizer
{
public:
    HanwhaTimeSyncronizer();
    ~HanwhaTimeSyncronizer();

    HanwhaTimeSyncronizer(const HanwhaTimeSyncronizer&) = delete;
    HanwhaTimeSyncronizer& operator=(const HanwhaTimeSyncronizer&) = delete;
    HanwhaTimeSyncronizer(HanwhaTimeSyncronizer&&) = delete;
    HanwhaTimeSyncronizer& operator=(HanwhaTimeSyncronizer&&) = delete;

    void start(HanwhaSharedResourceContext* resourceConext);
    void setTimeSynchronizationEnabled(bool enabled);
    using TimeZoneShiftHandler = utils::MoveOnlyFunc<void(std::chrono::seconds)>;
    void setTimeZoneShiftHandler(TimeZoneShiftHandler handler);
    void setDateTime(const QDateTime& dateTime);

private:
    void verifyDateTime();
    void retryVerificationIn(std::chrono::milliseconds timeout);
    void updateTimeZoneShift(std::chrono::seconds value);
    void fireStartPromises();

    void doRequest(
        const QString& action, std::map<QString, QString> params,
        bool isList, utils::MoveOnlyFunc<void(HanwhaResponse)> handler);

    HanwhaSharedResourceContext* m_resourceConext = nullptr;
    QMetaObject::Connection m_syncTymeConnection;
    network::aio::Timer m_timer;
    nx_http::AsyncClient m_httpClient;

    std::list<utils::promise<void>*> m_startPromises;
    TimeZoneShiftHandler m_timeZoneHandler;
    std::chrono::seconds m_timeZoneShift{0};
    std::atomic<bool> m_timeSynchronizationEnabled{true};
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
