#pragma once

#include "hanwha_request_helper.h"

#include <nx/network/http/http_async_client.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaResource;

class HanwhaTimeSyncronizer
{
public:
    HanwhaTimeSyncronizer(const HanwhaResource* resource);
    ~HanwhaTimeSyncronizer();

    HanwhaTimeSyncronizer(const HanwhaTimeSyncronizer&) = delete;
    HanwhaTimeSyncronizer& operator=(const HanwhaTimeSyncronizer&) = delete;
    HanwhaTimeSyncronizer(HanwhaTimeSyncronizer&&) = delete;
    HanwhaTimeSyncronizer& operator=(HanwhaTimeSyncronizer&&) = delete;

private:
    void verifyDateTime();
    void setDateTime(const QDateTime& dateTime);
    void retryVerificationIn(std::chrono::milliseconds timeout);

    void doRequest(
        const QString& action, std::map<QString, QString> params,
        bool isList, utils::MoveOnlyFunc<void(HanwhaResponse)> handler);

    const HanwhaResource* const m_resource;
    QMetaObject::Connection m_syncTymeConnection;
    network::aio::Timer m_timer;
    nx_http::AsyncClient m_httpClient;
    uint64_t m_currentTimeZoneShiftSecs = 0;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
