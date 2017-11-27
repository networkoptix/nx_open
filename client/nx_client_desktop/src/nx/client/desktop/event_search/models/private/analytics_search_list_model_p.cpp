#include "analytics_search_list_model_p.h"

#include <chrono>

#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

namespace nx {
namespace client {
namespace desktop {

using namespace analytics::storage;

namespace {

static constexpr int kFetchBatchSize = 25;

static qint64 startTimeUs(const DetectedObject& object)
{
    NX_ASSERT(!object.track.empty());
    return object.track.empty() ? 0 : object.track.front().timestampUsec;
}

static const auto upperBoundPredicate =
    [](qint64 left, const DetectedObject& right)
    {
        return left > startTimeUs(right);
    };

} // namespace

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(),
    q(q)
{
}

AnalyticsSearchListModel::Private::~Private()
{
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::Private::camera() const
{
    return m_camera;
}

void AnalyticsSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
    clear();

    // TODO: #vkutin Subscribe to analytics metadata in RTSP stream. Process it as required.
}

int AnalyticsSearchListModel::Private::count() const
{
    return int(m_data.size());
}

void AnalyticsSearchListModel::Private::clear()
{
    m_prefetch.clear();
    m_fetchedAll = false;

    if (!m_data.empty())
    {
        ScopedReset reset(q);
        m_data.clear();
    }

    m_earliestTimeMs = std::numeric_limits<qint64>::max();
}

bool AnalyticsSearchListModel::Private::canFetchMore() const
{
    return !m_fetchedAll && m_camera && m_prefetch.empty()
        && q->accessController()->hasGlobalPermission(Qn::GlobalViewLogsPermission);
}

bool AnalyticsSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    //TODO: #vkutin This is a stub. Fill it when actual analytics fetch is implemented.
    return false;

    return true;
}

void AnalyticsSearchListModel::Private::commitPrefetch(qint64 latestStartTimeMs)
{
    if (!m_success)
        return;

    const auto latestTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(latestStartTimeMs)).count();

    const auto end = std::upper_bound(m_prefetch.begin(), m_prefetch.end(),
        latestTimeUs, upperBoundPredicate);

    const auto first = this->count();
    const auto count = std::distance(m_prefetch.begin(), end);

    if (count > 0)
    {
        ScopedInsertRows insertRows(q, QModelIndex(), first, first + count - 1);
        for (auto iter = m_prefetch.begin(); iter != end; ++iter)
            m_data.push_back(std::move(*iter));
    }

    m_fetchedAll = count == m_prefetch.size() && m_prefetch.size() < kFetchBatchSize;
    m_earliestTimeMs = latestStartTimeMs;
    m_prefetch.clear();
}

} // namespace
} // namespace client
} // namespace nx
