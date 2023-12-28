// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_server_statistics_storage.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::desktop;

namespace {

constexpr int kNoDataValue = -1;
constexpr int kDefaultUpdatePeriod = 500;
// Resend request if we are not receiving replies for this time.
constexpr int kRetryTimeoutMs = 5000;

} // namespace

QnMediaServerStatisticsStorage::QnMediaServerStatisticsStorage(
    SystemContext* systemContext,
    const QnUuid& serverId,
    int pointsLimit,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    m_serverId(serverId),
    m_pointsLimit(pointsLimit),
    m_updatePeriod(kDefaultUpdatePeriod),
    m_timer(new QTimer())
{
    NX_VERBOSE(this, nx::format("Created for server %1.").arg(m_serverId));
    connect(m_timer, &QTimer::timeout, this, &QnMediaServerStatisticsStorage::update);
    m_timer->start(m_updatePeriod);
}

QnMediaServerStatisticsStorage::~QnMediaServerStatisticsStorage()
{
    NX_VERBOSE(this, nx::format("Deleted for server %1.").arg(m_serverId));
}

void QnMediaServerStatisticsStorage::registerConsumer(
    QObject* target, std::function<void()> callback)
{
    connect(this, &QnMediaServerStatisticsStorage::statisticsChanged, target, callback);
    ++m_listeners;
    NX_VERBOSE(this, nx::format("Consumer added: 0x%1").arg((quint64) target, 0, 16));
}

void QnMediaServerStatisticsStorage::unregisterConsumer(QObject* target)
{
    disconnect(target);
    --m_listeners;
    NX_VERBOSE(this, nx::format("Consumer removed: 0x%1").arg((quint64) target, 0, 16));
}

QnStatisticsHistory QnMediaServerStatisticsStorage::history() const
{
    return m_history;
}

qint64 QnMediaServerStatisticsStorage::historyId() const
{
    if (m_history.isEmpty())
        return -1;

    return m_lastId;
}

int QnMediaServerStatisticsStorage::updatePeriod() const
{
    return m_updatePeriod;
}

qint64 QnMediaServerStatisticsStorage::uptimeMs() const
{
    return m_uptimeMs;
}

void QnMediaServerStatisticsStorage::setFlagsFilter(Qn::StatisticsDeviceType deviceType, int flags)
{
    m_flagsFilter[deviceType] = flags;
}

void QnMediaServerStatisticsStorage::update()
{
    const auto server =
        resourcePool()->getResourceById<QnMediaServerResource>(m_serverId);
    auto api = connectedServerApi();
    const bool canRequest = api
        && server
        && server->getStatus() == nx::vms::api::ResourceStatus::online;

    if (!m_listeners || m_updateRequests > 0 || !canRequest)
    {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        ++m_lastId;

        for (auto& stats: m_history)
        {
            NX_ASSERT(stats.values.size() == m_pointsLimit);
            stats.values.dequeue();
            stats.values.enqueue(kNoDataValue);
        }
    }

    emit statisticsChanged();

    if (!m_listeners || !canRequest)
        return;

    if (m_updateRequests == 0 || m_updateRequests * m_updatePeriod > kRetryTimeoutMs)
    {
        auto callback = nx::utils::guarded(this,
            [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
            {
                handleStatisticsReply(success, handle, result);
            });

        m_updateRequestHandle = api->getStatistics(m_serverId, callback, thread());
        m_updateRequests = 0;
        NX_VERBOSE(this, "Update requested. Handle: %1.", m_updateRequestHandle);
    }
    ++m_updateRequests;
}

void QnMediaServerStatisticsStorage::handleStatisticsReply(
    bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
{
    NX_VERBOSE(this, "Reply received. Handle: %1, success: %2, code: %3, error: %4",
        handle, success, result.error, result.errorString);

    if (handle != m_updateRequestHandle)
        return;

    m_updateRequests = 0;
    if (!success)
        return;

    m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
    ++m_lastId;

    const auto& reply = result.deserialized<QnStatisticsReply>();

    if (reply.updatePeriod > 0 && m_updatePeriod != reply.updatePeriod)
    {
        m_updatePeriod = (int) reply.updatePeriod;
        m_timer->stop();
        m_timer->start(m_updatePeriod);
        NX_VERBOSE(this, nx::format("Update period changed to %1.").arg(m_updatePeriod));
    }

    auto notUpdated = nx::utils::toQSet(m_history.keys());

    m_uptimeMs = reply.uptimeMs;

    for (const auto& nextData: reply.statistics)
    {
        if (m_flagsFilter.contains(nextData.deviceType)
            && (m_flagsFilter[nextData.deviceType] & nextData.deviceFlags)
                != m_flagsFilter[nextData.deviceType])
        {
            continue;
        }

        const auto id = nextData.description;
        notUpdated.remove(id);

        auto& stats = m_history[id];
        if (stats.values.empty())
        {
            stats.values.reserve(m_pointsLimit);
            std::fill_n(std::back_inserter(stats.values), m_pointsLimit - 1, kNoDataValue);
        }
        else
        {
            NX_ASSERT(stats.values.size() == m_pointsLimit);
            stats.values.dequeue();
        }

        stats.deviceType = nextData.deviceType;
        stats.description = nextData.description;
        stats.values.enqueue(nextData.value);
    }

    for (const auto& id: notUpdated)
    {
        auto& stats = m_history[id];
        NX_ASSERT(stats.values.size() == m_pointsLimit);
        stats.values.dequeue();

        const auto isValid = [](qreal value) { return value != kNoDataValue; };

        if (std::any_of(stats.values.cbegin(), stats.values.cend(), isValid))
            stats.values.enqueue(kNoDataValue);
        else
            m_history.remove(id);
    }
}
