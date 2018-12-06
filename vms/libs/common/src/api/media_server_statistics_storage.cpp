#include "media_server_statistics_storage.h"

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>
#include <api/server_rest_connection.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/synctime.h>
#include <common/common_module.h>

namespace {

constexpr int kNoDataValue = -1;
constexpr int kDefaultUpdatePeriod = 500;
// Resend request if we are not receiving replies for this time.
constexpr int kRetryTimeoutMs = 5000;

/**
 * Set size of values to pointsLimit. Fill by noDataValue or cut from the beginning if needed.
 */
void normalizeValuesList(QLinkedList<qreal>& values, int pointsLimit)
{
    while (values.size() < pointsLimit)
        values.prepend(kNoDataValue);
    while (values.size() > pointsLimit)
        values.removeFirst();
}

} // namespace

QnMediaServerStatisticsStorage::QnMediaServerStatisticsStorage(
    const QnUuid& serverId,
    int pointsLimit,
    QObject* parent)
    :
    QObject(parent),
    QnCommonModuleAware(parent),
    m_serverId(serverId),
    m_pointsLimit(pointsLimit),
    m_updatePeriod(kDefaultUpdatePeriod),
    m_timer(new QTimer())
{
    NX_VERBOSE(this, lm("Created for server %1.").arg(m_serverId));
    connect(m_timer, &QTimer::timeout, this, &QnMediaServerStatisticsStorage::update);
    m_timer->start(m_updatePeriod);
}

QnMediaServerStatisticsStorage::~QnMediaServerStatisticsStorage()
{
    NX_VERBOSE(this, lm("Deleted for server %1.").arg(m_serverId));
}

void QnMediaServerStatisticsStorage::registerConsumer(QObject* target, const char* slot)
{
    connect(this, SIGNAL(statisticsChanged()), target, slot);
    ++m_listeners;
    NX_VERBOSE(this, lm("Consumer added: 0x%1").arg((quint64) target, 0, 16));
}

void QnMediaServerStatisticsStorage::unregisterConsumer(QObject* target)
{
    disconnect(target);
    --m_listeners;
    NX_VERBOSE(this, lm("Consumer removed: 0x%1").arg((quint64) target, 0, 16));
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
    NX_VERBOSE(this, "Update requested.");

    const auto server =
        commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(m_serverId);
    bool canRequest = server && server->getStatus() == Qn::Online;

    if (!m_listeners || m_updateRequests > 0 || !canRequest)
    {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        ++m_lastId;

        for (QnStatisticsData& stats: m_history)
        {
            stats.values.append(kNoDataValue);
            normalizeValuesList(stats.values, m_pointsLimit);
        }
    }

    emit statisticsChanged();

    if (!m_listeners || !canRequest)
    {
        NX_VERBOSE(this, "Can't send request.");
        return;
    }

    if (m_updateRequests == 0 || m_updateRequests * m_updatePeriod > kRetryTimeoutMs)
    {
        m_updateRequestHandle = server->restConnection()->getStatistics(
            [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
            {
                handleStatisticsReply(success, handle, result);
            },
            thread());
        m_updateRequests = 0;
        NX_VERBOSE(this, lm("Update requested. Handle: %1.").arg(m_updateRequestHandle));
    }
    ++m_updateRequests;
}

void QnMediaServerStatisticsStorage::handleStatisticsReply(
    bool success, rest::Handle handle, const QnJsonRestResult& result)
{
    NX_VERBOSE(this, lm("Reply received. Handle: %1, Status: %2.").args(handle, success));

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
        NX_VERBOSE(this, lm("Update period changed to %1.").arg(m_updatePeriod));
    }

    QSet<QString> notUpdated = m_history.keys().toSet();

    m_uptimeMs = reply.uptimeMs;

    for(const QnStatisticsDataItem& nextData: reply.statistics)
    {
        if (m_flagsFilter.contains(nextData.deviceType)
            && (m_flagsFilter[nextData.deviceType] & nextData.deviceFlags)
                != m_flagsFilter[nextData.deviceType])
        {
            continue;
        }

        QString id = nextData.description;
        notUpdated.remove(id);

        QnStatisticsData& stats = m_history[id];
        stats.deviceType = nextData.deviceType;
        stats.description = nextData.description;
        stats.values.append(nextData.value);
        normalizeValuesList(stats.values, m_pointsLimit);
    }

    for(const QString& id: notUpdated)
    {
        QnStatisticsData& stats = m_history[id];
        stats.values.append(kNoDataValue);
        normalizeValuesList(stats.values, m_pointsLimit);
        if (stats.values.count(kNoDataValue) == stats.values.size())
            m_history.remove(id);
    }
}
